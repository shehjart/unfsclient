/*
 *    unfsclient is a user-space NFS client based on FUSE.
 *    For details see:
 *
 *    http://www.gelato.unsw.edu.au/IA64wiki/unfsclient
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License along
 *    with this program; if not, write to the Free Software Foundation, Inc.,
 *    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define FUSE_USE_VERSION 26


#include <fuse_lowlevel.h>
#include <nfsclient.h>
#include <nfsclientd.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <nfs3actor.h>

#ifdef __NFSCLIENTD_DEBUG__
#include <debug_print.h>
#define nfscdlvl 1
#endif

static int
init_nfsclient_context_pool(struct nfsclientd_context * nfscd_ctx)
{
	int i;
	nfs_ctx **ctxpool, *ctx;
	int psize;

	assert(nfscd_ctx != NULL);
	psize = nfscd_ctx->mountopts.threadpool * nfscd_ctx->mountopts.ctxpoolsize;
	ctxpool = (nfs_ctx **)malloc(sizeof(nfs_ctx *) * psize);
	if(ctxpool == NULL)
		return -1;

	/* Init all contexts... */
	for(i = 0; i < psize; i++) {
		ctx = nfs_init(nfscd_ctx->mountopts.srvaddr, IPPROTO_TCP,
				NFSC_CFL_NONBLOCKING);
		if(ctx == NULL)
			return -2;

		ctxpool[i] = ctx;
	}

	nfscd_ctx->nfsctx_pool = ctxpool;
	return 0;
}

static int
init_nfsclient_queues(struct nfsclientd_context * ctx)
{
	assert(ctx != NULL);
	pthread_mutex_init(&ctx->md_lock, NULL);
	INIT_FLIST_HEAD(&ctx->md_rq);

	pthread_mutex_init(&ctx->rw_lock, NULL);
	INIT_FLIST_HEAD(&ctx->rw_rq);

	sem_init(&ctx->md_notify, 0, 0);
	sem_init(&ctx->rw_notify, 0, 0);
	return 0;
}

static int
init_nfsclient_thread_pool(struct nfsclientd_context * ctx)
{
	assert(ctx != NULL);
	pthread_t tid;
	int i, tpool;

	tpool = ctx->mountopts.threadpool;
	ctx->tids = (pthread_t *)malloc(sizeof(pthread_t) * tpool);
	assert(ctx->tids != NULL);
	for(i = 0; i < tpool; i++) {
		pthread_create(&tid, NULL, nfs3actor_thread, (void *)ctx);
		ctx->tids[i] = tid;
		debug_print(nfscdlvl, "[nfsclientd]: Created actor: 0x%x\n", tid);
	}

	return 0;
}

void
nfscd_init(void *userdata, struct fuse_conn_info *conn)
{
	struct nfsclientd_context * ctx = NULL;
	
	assert(userdata != NULL);
	ctx = (struct nfsclientd_context *)userdata;

	if((init_nfsclient_context_pool(ctx)) < 0) {
		fprintf(stderr, "nfsclientd: Could not init context pool.\n");
		exit(-1);
	}

	if((init_nfsclient_queues(ctx)) < 0) {
		fprintf(stderr, "nfsclientd: Could not init request queues.\n");
		goto destroy_context;
	}

	conn->async_read = 1;
	if((init_nfsclient_thread_pool(ctx)) < 0) {
		fprintf(stderr, "nfsclientd: Could not init thread pool.\n");
		goto destroy_context;
	}

	nfscd_ctx = ctx;
	return;

destroy_context:
	nfscd_destroy((void *)ctx);
	return;
}

static void
destroy_nfsclient_context_pool(struct nfsclientd_context * ctx)
{
	int i, psize;
	nfs_ctx * nfsctx = NULL;
	if(ctx == NULL)
		return;

	psize = ctx->mountopts.threadpool * ctx->mountopts.ctxpoolsize;
	for(i = 0; i < psize; i++) {
		nfsctx = (ctx->nfsctx_pool[i]);
		nfs_destroy(nfsctx);
	}
	
	free(ctx->nfsctx_pool);
	return;
}

static void
destroy_nfsclient_queues(struct nfsclientd_context * ctx)
{

	return;
}

static void
destroy_nfsclient_thread_pool(struct nfsclientd_context * ctx)
{
	int i;
	assert(ctx != NULL);

	for(i = 0; i < ctx->mountopts.threadpool; i++) {
		debug_print(nfscdlvl, "[nfsclientd]: Canceling actor: 0x%x\n",
				ctx->tids[i]);
		pthread_cancel(ctx->tids[i]);
		pthread_join(ctx->tids[i], NULL);
	}

	free(ctx->tids);
	return;
}

void
nfscd_destroy(void *userdata)
{
	struct nfsclientd_context * ctx = (struct nfsclientd_context *)userdata;

	assert(ctx != NULL);
	destroy_nfsclient_thread_pool(ctx);
	destroy_nfsclient_queues(ctx);
	destroy_nfsclient_context_pool(ctx);

	return;
}

struct nfscd_request *
nfscd_next_request(struct nfsclientd_context * ctx)
{
	struct nfscd_request * rq = NULL;
	struct flist_head * elem = NULL;
	assert(ctx != NULL);

	sem_wait(&ctx->md_notify);
	pthread_mutex_lock(&ctx->md_lock);
	elem = flist_first(&ctx->md_rq);
	rq = flist_entry(elem, struct nfscd_request, list);
	flist_del(elem);
	pthread_mutex_unlock(&ctx->md_lock);

	return rq;
}

void
nfscd_lookup(fuse_req_t req, fuse_ino_t parent, const char * name)
{
	struct nfscd_request * rq = NULL;
	assert(req != NULL && name != NULL);

	rq = (struct nfscd_request *)malloc(sizeof(struct nfscd_request));
	assert(rq != NULL);

	rq->fuserq = req;
	rq->args_u.lookupargs.parent = parent;
	rq->args_u.lookupargs.name = strdup(name);
	INIT_FLIST_HEAD(&rq->list);

	pthread_mutex_lock(&nfscd_ctx->md_lock);
	flist_add_tail(&rq->list, &nfscd_ctx->md_rq);
	sem_post(&nfscd_ctx->md_notify);
	pthread_mutex_unlock(&nfscd_ctx->md_lock);

	return;
}
