/*
 *    Test code for nfsclient library.
 *    Copyright (C) 2007 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available at:
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
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <nfsclient.h>

fhandle3 mntfh;

void nfs_mknod_cb(void *msg, int len, void *priv_ctx)
{
	MKNOD3res *res = NULL;
	int fh_len;

	res = xdr_to_MKNOD3res(msg, len);
	if(res == NULL)
		return;
	
	fprintf(stdout, "Inside mknod callback..\n");
	if(res->status != NFS3_OK) {
		fprintf(stdout, "Mknod call failed..\n");
		free_MKNOD3res(res);
		return;
	}

	fh_len = res->MKNOD3res_u.resok.obj.post_op_fh3_u.handle.data.data_len;
	free_MKNOD3res(res);
	fprintf(stderr, "NFS MKNOD Reply received: FH Size: %d\n", fh_len);
	
	return;
}


void nfs_mnt_cb(void *msg, int len, void *priv_ctx)
{
	mountres3 *mntres = NULL; 
	char *fh = NULL;
	u_long fh_len;

	mntres = xdr_to_mntres3(msg, len);
	if(mntres == NULL)
		return;

	fprintf(stdout, "Inside mount callback..\n");
	if(mntres->fhs_status != MNT3_OK) {
		fprintf(stdout, "Mount call failed..\n");
		free_mntres3(mntres);
		return;
	}
	
	fh_len = mntres->mountres3_u.mountinfo.fhandle.fhandle3_len;
	fh = mntres->mountres3_u.mountinfo.fhandle.fhandle3_val;
	mntfh.fhandle3_val = (char *)mem_alloc(fh_len);
	mntfh.fhandle3_len = fh_len;
	if(mntfh.fhandle3_val == NULL) {
		free_mntres3(mntres);
		return;
	}

	memcpy(mntfh.fhandle3_val, fh, fh_len);
	fprintf(stderr, "Recd FH from Mount len: %d\n", (u_int32_t)fh_len);
	free_mntres3(mntres);

	return;
}


int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	MKNOD3args mkd;

	if(argc < 4) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous "
				"libnfsclient NFS MKNOD call interface and "
				"callback\n"
				"USAGE: nfs_mknod <server> <remote_file_dir> "
				"<devicename>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	fprintf(stdout, "Resolving name %s\n", argv[1]);
	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "%s: Cannot resolve name: %s: %s\n",
				argv[0], argv[1], gai_strerror(err));
		return -1;
	}
	
	fprintf(stdout, "Creating nfs client context..\n");
	ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr, IPPROTO_TCP, 0);
	if(ctx == NULL) {
		fprintf(stderr, "%s: Cant init nfs context\n", argv[0]);
		return 0;
	}

	freeaddrinfo(srv_addr);
	mntfh.fhandle3_len = 0;
	fprintf(stdout, "Sending mount call..\n");
	stat = mount3_mnt(&argv[2], ctx, nfs_mnt_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS MOUNT Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS MOUNT call\n");
		return 0;
	}

	mkd.where.name = argv[3];
	mkd.where.dir.data.data_len = mntfh.fhandle3_len;
	mkd.where.dir.data.data_val = mntfh.fhandle3_val;
	mkd.what.type = NF3FIFO;
	memset(&mkd.what.mknoddata3_u.pipe_attributes, 0, sizeof(sattr3));
	mkd.what.mknoddata3_u.pipe_attributes.mode.set_it = 1;
	mkd.what.mknoddata3_u.pipe_attributes.uid.set_it = 1;
	mkd.what.mknoddata3_u.pipe_attributes.gid.set_it = 1;
	mkd.what.mknoddata3_u.pipe_attributes.size.set_it = 1;
	mkd.what.mknoddata3_u.pipe_attributes.atime.set_it = 1;
	mkd.what.mknoddata3_u.pipe_attributes.mtime.set_it = 1;

	fprintf(stdout, "Sending mknod call..\n");
	stat = nfs3_mknod(&mkd, ctx, nfs_mknod_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS MKNOD Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS MKNOD call\n");
		return 0;
	}

	fprintf(stdout, "Waiting for mknod reply..\n");
	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	fprintf(stdout, "Received mknod reply..\n");
	
	return 0;
}


