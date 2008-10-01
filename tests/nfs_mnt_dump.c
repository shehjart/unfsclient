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

#include <nfsclient.h>

void mnt_dump_cb(void *m, int len, void *p)
{
	mountlist mb, tmp;
	mb = tmp = NULL;
	if(m == NULL)
		return;

	mb = xdr_to_mountlist(m, len);
	if(mb == NULL)
		return;
	
	tmp = mb;
	fprintf(stderr, "Mount DUMP reply received: Size: %d\n", len);
	while(mb != NULL) {
		fprintf(stdout, "Hostname: %s\n", mb->ml_hostname);
		fprintf(stdout, "Directory: %s\n", mb->ml_directory);
		mb = mb->ml_next;
	}
	
	free_mountlist(tmp);
	
}


int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	if(argc < 2) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous "
				"libnfsclient MOUNT DUMP call interface and "
				"callback\n"
				"USAGE: nfs_mnt_dump <server>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	fprintf(stdout, "Resolving name %s\n", argv[1]);
	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "Cannot resolve name: %s\n", gai_strerror(err));
		return -1;
	}
	
	fprintf(stdout, "Creating NFS client context\n");
	ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr, IPPROTO_TCP, 0);
	if(ctx == NULL) {
		fprintf(stderr, "Cant init nfs context\n");
		return 0;
	}

	fprintf(stdout, "Sending mount dump call\n");
	stat = mount3_dump(ctx, mnt_dump_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "MOUNT DUMP Call sent successfully\n");
	else
		fprintf(stderr, "Could not send MOUNT NULL call\n");

	return 0;
}
