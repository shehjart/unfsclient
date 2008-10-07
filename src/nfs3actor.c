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

#ifdef __NFSACTOR_DEBUG__
#include <debug_print.h>
#define nfsactlvl 1
#endif


void *
nfs3_request_actor(void * arg)
{
	struct nfsclientd_context * ctx = NULL;
	struct nfscd_request * rq = NULL;
	pthread_t tid;
	
	assert(arg != NULL);
	tid = pthread_self();
	ctx = (struct nfsclientd_context *)arg;
	debug_print(nfsactlvl, "[Actor 0x%x]: Waiting for request..\n", tid);

	while(1) {
		rq = nfscd_dequeue_metadata_request(ctx);
		debug_print(nfsactlvl, "[Actor 0x%x]: Got request..\n", tid);




	}

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
