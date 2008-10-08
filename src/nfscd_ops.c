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

#ifdef __NFSCLIENTD_DEBUG__
#include <debug_print.h>
#define nfscdlvl 1
#endif

nfs_ctx **
init_per_actor_context_pool(struct nfsclientd_context * nfscd_ctx)
{
	int i;
	nfs_ctx **ctxpool, *ctx;
	int psize;

	assert(nfscd_ctx != NULL);
	psize = nfscd_ctx->mountopts.ctxpoolsize;
	ctxpool = (nfs_ctx **)malloc(sizeof(nfs_ctx *) * psize);
	if(ctxpool == NULL)
		return NULL;

	/* Init all contexts... */
	for(i = 0; i < psize; i++) {
		ctx = nfs_init(nfscd_ctx->mountopts.srvaddr, IPPROTO_TCP,
				NFSC_CFL_NONBLOCKING);
		if(ctx == NULL)
			return NULL;

		ctxpool[i] = ctx;
	}

	return ctxpool;
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
	int (*init_actor)(void);
	void * (*request_actor)(void *);

	tpool = ctx->mountopts.threadpool;
	ctx->tids = (pthread_t *)malloc(sizeof(pthread_t) * tpool);
	assert(ctx->tids != NULL);
	
	init_actor = ctx->actor->init_actor;
	request_actor = ctx->actor->request_actor;

	(*init_actor)();
	for(i = 0; i < tpool; i++) {
		pthread_create(&tid, NULL, request_actor, (void *)ctx);
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

void
destroy_per_actor_context_pool(struct nfsclientd_context * ctx, nfs_ctx ** ctxpool)
{
	int i, psize;
	nfs_ctx * nfsctx = NULL;
	assert(ctx != NULL && ctxpool != NULL);

	psize = ctx->mountopts.ctxpoolsize;
	for(i = 0; i < psize; i++) {
		nfsctx = ctxpool[i];
		nfs_destroy(nfsctx);
	}
	
	free(ctxpool);
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

	return;
}

struct nfscd_request *
nfscd_dequeue_metadata_request(struct nfsclientd_context * ctx)
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


struct nfscd_request *
nfscd_dequeue_data_request(struct nfsclientd_context * ctx)
{
	struct nfscd_request * rq = NULL;
	struct flist_head * elem = NULL;
	assert(ctx != NULL);

	sem_wait(&ctx->rw_notify);
	pthread_mutex_lock(&ctx->rw_lock);
	elem = flist_first(&ctx->rw_rq);
	rq = flist_entry(elem, struct nfscd_request, list);
	flist_del(elem);
	pthread_mutex_unlock(&ctx->rw_lock);

	return rq;
}


void
nfscd_enqueue_metadata_request(struct nfsclientd_context * ctx,
		struct nfscd_request * rq)
{
	assert(rq != NULL && ctx != NULL);
	
	INIT_FLIST_HEAD(&rq->list);

	pthread_mutex_lock(&ctx->md_lock);
	flist_add_tail(&rq->list, &ctx->md_rq);
	sem_post(&ctx->md_notify);
	pthread_mutex_unlock(&ctx->md_lock);
}


void
nfscd_enqueue_data_request(struct nfsclientd_context * ctx, 
		struct nfscd_request * rq)
{
	assert(rq != NULL && ctx != NULL);
	
	INIT_FLIST_HEAD(&rq->list);

	pthread_mutex_lock(&ctx->rw_lock);
	flist_add_tail(&rq->list, &ctx->rw_rq);
	sem_post(&ctx->rw_notify);
	pthread_mutex_unlock(&ctx->rw_lock);
}


void
nfscd_lookup(fuse_req_t req, fuse_ino_t parent, const char * name)
{
	struct nfscd_request * rq = NULL;
	assert(req != NULL && name != NULL && nfscd_ctx != NULL);

	rq = (struct nfscd_request *)malloc(sizeof(struct nfscd_request));
	assert(rq != NULL);

	rq->fuserq = req;
	rq->args_u.lookupargs.parent = parent;
	rq->args_u.lookupargs.name = strdup(name);
	rq->request = FUSE_LOOKUP;
	rq->actordata = NULL;

	nfscd_enqueue_metadata_request(nfscd_ctx, rq);
	return;
}
