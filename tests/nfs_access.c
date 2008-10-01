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

#include <nfsclient.h>

fhandle3 mntfh;
fhandle3 lfh;

void nfs_access_cb(void *msg, int len, void *priv_ctx)
{

	ACCESS3res *res = NULL;

	res = xdr_to_ACCESS3res(msg, len);
	if(res == NULL)
		return;
	fprintf(stdout, "Inside access callback..\n");
	if(res->status != NFS3_OK) {
		fprintf(stdout, "Access call failed..\n");
		free_ACCESS3res(res);
		return;
	}

	fprintf(stderr, "NFS ACCESS Reply received, access 0x%x\n",
			(unsigned int)res->ACCESS3res_u.resok.access);

	free_ACCESS3res(res);
	return;
}


void nfs_lookup_cb(void *msg, int len, void *priv_ctx)
{
	LOOKUP3res *lookupres = NULL;
	int fh_len;
	char *fh;

	lookupres = xdr_to_LOOKUP3res(msg, len);
	if(lookupres == NULL)
		return;

	fprintf(stdout, "Inside lookup callback..\n");
	if(lookupres->status != NFS3_OK) {
		free_LOOKUP3res(lookupres);
		fprintf(stdout, "lookup call failed..\n");
		return;
	}
	
	/* Prepare to copy looked up file handle from lookup reply. */
	fh_len = lookupres->LOOKUP3res_u.resok.object.data.data_len;
	fh = lookupres->LOOKUP3res_u.resok.object.data.data_val;
	lfh.fhandle3_len = fh_len;
	lfh.fhandle3_val = (char *)mem_alloc(fh_len);
	if(lfh.fhandle3_val == NULL) {
		free_LOOKUP3res(lookupres);
		return;
	}

	memcpy(lfh.fhandle3_val, fh, fh_len);
	free_LOOKUP3res(lookupres);

	fprintf(stderr, "NFS LOOKUP reply received: FH size: %d\n",
			fh_len);
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
	

	/* Prepare to copy file handle of mount directory into the
	 * global filehandle so that we can use it in main.
	 */
	fh_len = mntres->mountres3_u.mountinfo.fhandle.fhandle3_len;
	fh = mntres->mountres3_u.mountinfo.fhandle.fhandle3_val;
	mntfh.fhandle3_val = (char *)mem_alloc(fh_len);
	if(mntfh.fhandle3_val == NULL) {
		free_mntres3(mntres);
		return;
	}

	memcpy(mntfh.fhandle3_val, fh, fh_len);
	mntfh.fhandle3_len = fh_len;
	fprintf(stdout, "Recd FH from Mount len: %d\n", (u_int32_t)fh_len);
	/* See libnfsclient/nfs3/nfs3.h for other helper functions for
	 * freeing NFS message structures.
	 */
	free_mntres3(mntres);
	return;
}


int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	LOOKUP3args largs;
	ACCESS3args ac;

	if(argc < 4) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous"
				" libnfsclient NFS ACCESS call interface and "
				" callback.\n"
				"USAGE: nfs_access <server> <remote_file_dir> "
				"<file/dirname>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	fprintf(stdout, "Resolving name: %s\n", argv[1]);
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
	largs.what.dir.data.data_len = mntfh.fhandle3_len;
	largs.what.dir.data.data_val = mntfh.fhandle3_val;
	largs.what.name = argv[3];

	lfh.fhandle3_len = 0;
	fprintf(stdout, "Sending lookup call for: %s\n", argv[3]);
	stat = nfs3_lookup(&largs, ctx, nfs_lookup_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS LOOKUP Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS LOOKUP call\n");
		return 0;
	}

	/* Mount file handle gets allocated inside the mount callback.
	 * Now that we do not need it, free it.
	 */
	mem_free(mntfh.fhandle3_val, mntfh.fhandle3_len);
	fprintf(stdout, "Waiting for lookup reply..\n");
	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	
	ac.object.data.data_len = lfh.fhandle3_len;
	ac.object.data.data_val = lfh.fhandle3_val;
	ac.access |= ACCESS3_READ;

	fprintf(stdout, "Sending access call..\n");
	stat = nfs3_access(&ac, ctx, nfs_access_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS ACCESS Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS ACCESS call\n");
		return 0;
	}

	fprintf(stdout, "Waiting for access reply..\n");
	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	fprintf(stdout, "Received access reply..\n");

	return 0;
}
