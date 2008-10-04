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

static void
init_ctxpool_mount_cb(void *msg, int bufsz, void *priv)
{
	int * mountstatus = NULL;

	assert(priv != NULL);
	mountstatus = (int *)priv;
	*mountstatus = 1;
}

static int
init_nfsclient_context_pool(struct nfsclientd_context * nfscd_ctx)
{
	int i;
	assert(nfscd_ctx != NULL);
	nfs_ctx **ctxpool, *ctx;
	int mountstatus;

	ctxpool = (nfs_ctx **)malloc(sizeof(nfs_ctx *) * DEFAULT_CTXPOOL_SIZE);
	if(ctxpool == NULL)
		return -1;

	for(i = 0; i < DEFAULT_CTXPOOL_SIZE; i++) {
		ctx = ctxpool[i];
		ctx = nfs_init(nfscd_ctx->mountopts.srvaddr, IPPROTO_TCP,
				NFSC_CFL_NONBLOCKING);
		if(ctx == NULL)
			return -2;

		mountstatus = 0;
		mount3_mnt((dirpath *)&nfscd_ctx->mountopts.remotedir, ctx,
				init_ctxpool_mount_cb, &mountstatus,
				RPC_BLOCKING_WAIT);

		if(mountstatus != 1)
			return -3;
	}

	nfscd_ctx->nfsctx_pool = ctxpool;
	return 0;
}

void
nfscd_mount_init(void *userdata, struct fuse_conn_info *conn)
{
	struct nfsclientd_context * ctx = NULL;
	
	assert(userdata != NULL);
	ctx = (struct nfsclientd_context *)userdata;

	if((init_nfsclient_context_pool(ctx)) < 0) {
		fprintf(stderr, "nfsclientd: Could not init context pool.\n");
		exit(-1);
	}
}

void
nfscd_destroy(void *userdata)
{
	fprintf(stderr, "Later..\n");
}
