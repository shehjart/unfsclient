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
FSINFO3resok fsinfo;

void nfs_fsinfo_cb(void *msg, int len, void *priv_ctx)
{
	FSINFO3res *res = NULL;

	res = xdr_to_FSINFO3res(msg, len);
	if(res == NULL)
		return;

	fprintf(stdout, "Inside fsinfo callback..\n");
	if(res->status != NFS3_OK) {
		fprintf(stdout, "Fsinfo call failed..\n");
		free_FSINFO3res(res);
		return;
	}

	fprintf(stdout, "NFS FSINFO Reply received: Msg Size: %d\n", len);
	fsinfo = res->FSINFO3res_u.resok;	
	free_FSINFO3res(res);
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
	fprintf(stdout, "Recd FH from Mount len: %d\n", (u_int32_t)fh_len);
	free_mntres3(mntres);
	return;
}


static void
print_fsinfo(FSINFO3resok fsi)
{
	fprintf(stdout, "Max Read Transfer Size: %ld Kb\n", (fsi.rtmax / 1024));
	fprintf(stdout, "Preferred Read Size: %ld Kb\n", (fsi.rtpref / 1024));
	fprintf(stdout, "Suggested Read Size Multiple: %ld Kb\n",
			(fsi.rtmult / 1024));
	fprintf(stdout, "Max Write Transfer Size: %ld Kb\n", (fsi.wtmax / 1024));
	fprintf(stdout, "Preferred Write Size: %ld Kb\n", (fsi.wtpref / 1024));
	fprintf(stdout, "Suggested Write Size Multiple: %ld Kb\n",
			(fsi.wtmult / 1024));
	fprintf(stdout, "Preferred READDIR Request Size: %ld Kb\n", 
			(fsi.dtpref / 1024));
	fprintf(stdout, "Maximum File Size: %lu Mb\n", 
			(fsi.maxfilesize / (1024 * 1024)));

}

int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	FSINFOargs fsi;

	if(argc < 3) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous "
				"libnfsclient NFS FSINFO call interface and "
				"callback.\n"
				"USAGE: nfs_fsinfo <server> <remote_file_dir>\n");
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

	fprintf(stdout, "Mount reply received..\n");

	fsi.fsroot.data.data_len = mntfh.fhandle3_len;
	fsi.fsroot.data.data_val = mntfh.fhandle3_val;
	fprintf(stdout, "Sending fsinfo call..\n");
	stat = nfs3_fsinfo(&fsi, ctx, nfs_fsinfo_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS FSINFO Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS FSINFO call\n");
		return 0;
	}

	fprintf(stdout, "Waiting for fsinfo reply..\n");
	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	fprintf(stdout, "Received fsinfo reply..\n");
	
	print_fsinfo(fsinfo);
	return 0;
}


