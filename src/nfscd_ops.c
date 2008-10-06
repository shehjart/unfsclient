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
	for(i = 0; i < DEFAULT_CTXPOOL_SIZE; i++) {
		ctx = ctxpool[i];
		ctx = nfs_init(nfscd_ctx->mountopts.srvaddr, IPPROTO_TCP,
				NFSC_CFL_NONBLOCKING);
		if(ctx == NULL)
			return -2;
	}

	nfscd_ctx->nfsctx_pool = ctxpool;
	return 0;
}

static int
init_nfsclient_queues(struct nfsclientd_context * ctx)
{
	return 0;
}

static int
init_nfsclient_thread_pool(struct nfsclientd_context * ctx)
{
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

	return;

destroy_context:
	nfscd_destroy((void *)ctx);
	return;
}

static void
destroy_nfsclient_context_pool(struct nfsclientd_context * ctx)
{
	int i, psize;
	if(ctx == NULL)
		return;

	psize = ctx->mountopts.threadpool * ctx->mountopts.ctxpoolsize;
	for(i = 0; i < psize; i++)
		nfs_destroy(ctx->nfsctx_pool[i]);

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
