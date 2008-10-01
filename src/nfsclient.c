/*
 *    libnfsclient2, second incarnation of libnfsclient, a library for
 *    NFS operations from user space.
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available here:
 *    http://nfsreplay.sourceforge.net
 *    
 *    and
 *
 *    http://www.gelato.unsw.edu.au/IA64wiki/libnfsclient
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

#include <rpc/rpc.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include <nfsclient.h>

nfs_ctx *
nfs_init(struct sockaddr_in *srv, int proto, int connflags)
{
	nfs_ctx *ctx = NULL;

	if(srv == NULL)
		return NULL;

	ctx = (nfs_ctx *)malloc(sizeof(nfs_ctx));
	if(ctx == NULL)
		return NULL;

	ctx->nfs_srv = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if(ctx->nfs_srv == NULL) {
		free(ctx);
		return NULL;
	}

	memcpy(ctx->nfs_srv, srv, sizeof(struct sockaddr_in));

	ctx->nfs_mnt = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
	if(ctx->nfs_mnt == NULL) {
		free(ctx->nfs_srv);
		free(ctx);
		return NULL;
	}

	memcpy(ctx->nfs_mnt, srv, sizeof(struct sockaddr_in));
	
	ctx->nfs_transport = proto;
	ctx->nfs_connflags = connflags;
	ctx->nfs_rsize = 0;
	ctx->nfs_wsize = 0;
	ctx->nfs_cl = NULL;
	ctx->nfs_mnt_cl = NULL;

	return ctx;
}


int 
check_ctx(nfs_ctx *ctx)
{
	if(ctx == NULL)
		return 0;

	if((ctx->nfs_transport != IPPROTO_UDP)
			&& (ctx->nfs_transport != IPPROTO_TCP))
		return 0;

	return 1;
}


void 
mnt_complete(nfs_ctx * ctx)
{
	if(ctx == NULL)
		return;

	clnttcp_nb_receive(ctx->nfs_mnt_cl, 0);
}

int 
nfs_complete(nfs_ctx * ctx, int flag)
{
	if(ctx == NULL)
		return 0;

	return clnttcp_nb_receive(ctx->nfs_cl, flag);
}

