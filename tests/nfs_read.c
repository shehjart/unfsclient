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
fhandle3 lfh;

void nfs_read_cb(void *msg, int len, void *priv_ctx)
{
	READ3res *res = NULL;
	int read_len;	
	res = xdr_to_READ3res(msg, len, NFS3_DATA_DEXDR);
	if(res == NULL)
		return;
	if(res->status != NFS3_OK) {
		fprintf(stderr, "ERROR READING\n");
		return;
	}
	read_len = res->READ3res_u.resok.data.data_len;
	fprintf(stderr, "NFS READ Reply received: Record Size: %d,"
			"Read Size: %d\n", len, read_len);
	fprintf(stderr, "Read data: \n%s\n",
			res->READ3res_u.resok.data.data_val);
	fflush(stderr);
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

	if(lookupres->status != NFS3_OK)
		return;
	
	fh_len = lookupres->LOOKUP3res_u.resok.object.data.data_len;
	fh = lookupres->LOOKUP3res_u.resok.object.data.data_val;

	lfh.fhandle3_len = fh_len;
	lfh.fhandle3_val = (char *)mem_alloc(fh_len);
	if(lfh.fhandle3_val == NULL) {
		mem_free(lookupres->LOOKUP3res_u.resok.object.data.data_val, fh_len);
		mem_free(lookupres, sizeof(LOOKUP3res));
		return;
	}

	memcpy(lfh.fhandle3_val, fh, fh_len);

	mem_free(lookupres->LOOKUP3res_u.resok.object.data.data_val, fh_len);
	mem_free(lookupres, sizeof(LOOKUP3res));

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

	if(mntres->fhs_status != MNT3_OK) {
		mem_free(mntres, sizeof(mntres3));
		return;
	}
	
	fh_len = mntres->mountres3_u.mountinfo.fhandle.fhandle3_len;
	fh = mntres->mountres3_u.mountinfo.fhandle.fhandle3_val;
	mntfh.fhandle3_val = (char *)mem_alloc(fh_len);
	if(mntfh.fhandle3_val == NULL) {
		mem_free(mntres, sizeof(mntres3));
		return;
	}

	memcpy(mntfh.fhandle3_val, fh, fh_len);
	fprintf(stderr, "Recd FH from Mount len: %d\n",
			(u_int32_t)fh_len);
	mntfh.fhandle3_len = fh_len;
	mem_free(mntres->mountres3_u.mountinfo.fhandle.fhandle3_val,
			fh_len);
	mem_free(mntres, sizeof(mntres3));
	return;
}


int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	LOOKUP3args largs;
	READ3args read;

	if(argc < 6) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous libnfsclient NFS READ call interface and callback\n"
				"USAGE: nfs_read <server> <remote_file_dir> <file/dirname> <offset> <count>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "%s: Cannot resolve name: %s: %s\n",
				argv[0], argv[1], gai_strerror(err));
		return -1;
	}
	
	ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr, IPPROTO_TCP, 0);
	if(ctx == NULL) {
		fprintf(stderr, "%s: Cant init nfs context\n", argv[0]);
		return 0;
	}

	freeaddrinfo(srv_addr);
	mntfh.fhandle3_len = 0;
	stat = mount3_mnt(&argv[2], ctx, nfs_mnt_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS MOUNT Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS MOUNT call\n");
		return 0;
	}

	largs.what.dir.data.data_len = mntfh.fhandle3_len;
	largs.what.dir.data.data_val = mntfh.fhandle3_val;
	largs.what.name = argv[3];

	lfh.fhandle3_len = 0;
	stat = nfs3_lookup(&largs, ctx, nfs_lookup_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS LOOKUP Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS LOOKUP call\n");
		return 0;
	}

	mem_free(mntfh.fhandle3_val, mntfh.fhandle3_len);
	nfs_complete(ctx, RPC_BLOCKING_WAIT);

	read.file.data.data_len = lfh.fhandle3_len;
	read.file.data.data_val = lfh.fhandle3_val;
	read.offset = atol(argv[4]);
	read.count = atol(argv[5]);

	stat = nfs3_read(&read, ctx, nfs_read_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "NFS READ Call sent successfully\n");
	else {
		fprintf(stderr, "Could not send NFS READ call\n");
		return 0;
	}

	nfs_complete(ctx, RPC_BLOCKING_WAIT);
	
	return 0;
}
