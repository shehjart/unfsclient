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

#include <fuse_lowlevel.h>
#include <nfsclientd.h>
#include <nfsclient.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <nfs3actor.h>
#include <assert.h>
#include <errno.h>
#include <debug_print.h>

#ifdef __NFSACTOR_DEBUG__
#define nfsactlvl 1
#endif

struct nfs3_actor_state {
	pthread_t tid;

	nfs_ctx ** ctxpool;
	struct nfsclientd_context * nfscd_ctx;

	fhandle3 rootfh;
};

static void
nfs3_mount_cb(void *msg, int len, void *priv)
{
	mountres3 * res = NULL;
	fhandle3 * rootfh = NULL;

	assert(msg != NULL && priv != NULL);

	rootfh = (fhandle3 *)priv;
	fhandle3_fh(rootfh) = NULL;
	res = xdr_to_mntres3(msg, len);
	if(res == NULL)
		return;

	if(mnt3_reply_status(res) != MNT3_OK)
		return;

	/* Duplicate the file handle because we'll free the mount
	 * response structure next.
	 */
	fhandle3_dup(mnt3_fhandle3(res), rootfh);
	free_mntres3(res);
}

static int
nfs3_actor_mount_remotedir(struct nfs3_actor_state * as)
{
	fhandle3 rootfh;
	nfs_ctx * mntctx = NULL;
	char * remotedir = NULL;
	int stat;

	assert(as != NULL);
	mntctx = as->ctxpool[0];
	assert(mntctx != NULL);
	remotedir = as->nfscd_ctx->mountopts.remotedir;

	fhandle3_fh(&rootfh) = NULL;
	stat = mount3_mnt((dirpath *)&remotedir, mntctx, nfs3_mount_cb, &rootfh,
			RPC_BLOCKING_WAIT);

	if(stat != RPC_SUCCESS)
		return -1;

	if(fhandle3_fh(&rootfh) == NULL)
		return -1;

	as->rootfh = rootfh;
	return 0;
}



static int
init_nfs3_actor_thread_state(struct nfs3_actor_state * as,
		struct nfsclientd_context * ctx)
{
	assert(as != NULL && ctx != NULL);

	if((as->ctxpool = init_per_actor_context_pool(ctx)) == NULL)
		return -1;

	as->nfscd_ctx = ctx;
	if((nfs3_actor_mount_remotedir(as)) < 0)
		return -1;

	return 0;
}


static void
nfs3_lookup_cb(void *msg, int len, void *priv)
{
	struct nfscd_request * rq = NULL;
	LOOKUP3res * res = NULL;
	int replystat;

	assert(msg != NULL && priv != NULL);

	rq = (struct nfscd_request *)priv;
	res = xdr_to_LOOKUP3res(msg, len);
	assert(res != NULL);

	replystat = nfs3_reply_status(res);
	if(replystat != NFS3_OK) {
		fuse_reply_err(rq->fuserq, nfsstat3_to_errno(replystat));
		free_LOOKUP3res(res);
	}
}

static void
fuse_to_nfs3_lookup_args(struct nfs3_actor_state * as, nflookup_args * fargs,
		LOOKUP3args * la)
{
	assert(fargs!= NULL && la != NULL);

	debug_print(nfsactlvl, "[Actor 0x%x] lookup: %s\n", as->tid, fargs->name);
	set_LOOKUP3args_fname(la, fargs->name);
	set_LOOKUP3args_dir_fhlen(la, fhandle3_fhlen(&as->rootfh));
	set_LOOKUP3args_dir_fh(la, fhandle3_fh(&as->rootfh));

	return;
}

static void
nfs3_lookup_actor(struct nfs3_actor_state * as, struct nfscd_request * rq)
{
	nfs_ctx * nfsctx = NULL;
	LOOKUP3args la;

	assert(as != NULL && rq != NULL);
	fuse_to_nfs3_lookup_args(as, &rq->args_u.lookupargs, &la);
	nfsctx = as->ctxpool[0];
	rq->actordata = (void *)as;

	nfs3_lookup(&la, nfsctx, nfs3_lookup_cb, rq, RPC_BLOCKING_WAIT);

	return;
}

static void
nfs3_act_on_request(struct nfs3_actor_state * as, struct nfscd_request * rq)
{

	assert(as != NULL && rq != NULL);

	switch(rq->request) {

		case FUSE_LOOKUP:
			nfs3_lookup_actor(as, rq);
			break;

		default:
			fuse_reply_err(rq->fuserq, EIO);
			break;
	}

	return;
}

void *
nfs3_request_actor(void * arg)
{
	struct nfsclientd_context * ctx = NULL;
	struct nfscd_request * rq = NULL;
	pthread_t tid;
	struct nfs3_actor_state as;
	
	assert(arg != NULL);
	tid = pthread_self();
	ctx = (struct nfsclientd_context *)arg;
	if((init_nfs3_actor_thread_state(&as, ctx)) < 0) {
		nfscd_notify_actor_exit(ctx, tid, errno);
		return NULL;
	}

	debug_print(nfsactlvl, "[Actor 0x%x]: Waiting for request..\n", tid);
	while(1) {
		rq = nfscd_dequeue_metadata_request(ctx);
		debug_print(nfsactlvl, "[Actor 0x%x]: Got request..\n", tid);
		nfs3_act_on_request(&as, rq);
	}

	destroy_per_actor_context_pool(ctx, as.ctxpool);
	nfscd_notify_actor_exit(ctx, tid, 0);
	return NULL;
}

int
nfs3_actor_init()
{
	return 0;
}

void
nfs3_actor_destroy()
{
	return;
}

struct protocol_actor_ops nfs3_protocol_actor = {
	.init_actor = nfs3_actor_init,
	.request_actor = nfs3_request_actor,
	.destroy_actor = nfs3_actor_destroy,
};
