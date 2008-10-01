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

void nfs_create_cb(void *msg, int len, void *priv_ctx)
{
	CREATE3res *res = NULL;
	int fh_len;

	res = xdr_to_CREATE3res(msg, len);
	if(res == NULL)
		return;
	
	fprintf(stdout, "Inside create callback..\n");
	if(res->status != NFS3_OK) {
		fprintf(stdout, "Create call failed..\n");
		free_CREATE3res(res);
		return;
	}

	fh_len = res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_len;
	fprintf(stdout, "NFS CREATE Reply received: FH Size: %d\n", fh_len);
	
	free_CREATE3res(res);
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
	
	/* Prepare to copy mounted file handle. */
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

	CREATE3args cr;

	if(argc < 4) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous "
				"libnfsclient NFS CREATE call interface and "
				"callback\n"
				"USAGE: nfs_create <server> <remote_file_dir> "
				"<new file/dirname>\n");
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

	fprintf(stdout, "Received mount reply..\n");

	cr.where.dir.data.data_len = mntfh.fhandle3_len;
	cr.where.dir.data.data_val = mntfh.fhandle3_val;
	cr.where.name = argv[3];
	cr.how.mode = UNCHECKED;
	memset(&cr.how.createhow3_u, 0, sizeof(cr.how.createhow3_u));
	cr.how.createhow3_u.obj_attributes.mode.set_it = 1;
	cr.how.createhow3_u.obj_attributes.uid.set_it = 1;
	cr.how.createhow3_u.obj_attributes.gid.set_it = 1;
	cr.how.createhow3_u.obj_attributes.size.set_it = 1;
	cr.how.createhow3_u.obj_attributes.atime.set_it = SET_TO_SERVER_TIME;
	cr.how.createhow3_u.obj_attributes.mtime.set_it = SET_TO_SERVER_TIME;

	fprintf(stdout, "Sending create call..\n");
	stat = nfs3_create(&cr, ctx, nfs_create_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS CREATE Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS CREATE call\n");
		return 0;
	}

	fprintf(stdout, "Waiting for create reply..\n");
	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	fprintf(stdout, "Received create reply..\n");
	return 0;
}
