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


void *
nfs3_actor(void * arg)
{
	struct nfsclientd_context * ctx = NULL;
	struct nfscd_request * rq = NULL;
	
	assert(arg != NULL);
	ctx = (struct nfsclientd_context *)arg;
	fprintf(stderr, "Sleeping on request..\n");
	rq = nfscd_next_request(ctx);

	fprintf(stderr, "Got request..\n");
	return NULL;
}
