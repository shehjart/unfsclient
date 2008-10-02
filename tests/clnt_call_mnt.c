/*
 *    Test code for Asynchronous RPC library.
 *    Copyright (C) 2007 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available at:
 *
 *    http://www.gelato.unsw.edu.au/IA64wiki/AsyncRPC
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
#include <unistd.h>
#include <rpc/rpc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include <nfsclient.h>
#include <nfs3.h>
#include <clnt_tcp_nb.h>


void mount_cb(void *msg_buf, int bufsz, void *priv)
{
	fprintf(stdout, "Inside mount_cb..\n");
}

int main(int argc, char *argv[])
{
	CLIENT *cl = NULL;
	struct addrinfo *srv_addr, hints;
	int sockp = RPC_ANYSOCK;
	int err;
	enum clnt_stat stat;
	struct callback_info ucbi;
	struct rpc_proc_info rpc;

	if(argc < 3) {
		fprintf(stderr, "Not enough arguments\nThis tests the"
				"asynchronous clnt_call function, using mount"
				" request\n"
				"USAGE: clnt_call_mnt <server> <remote_dir>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	fprintf(stdout, "Resolving address: %s\n", argv[1]);
	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "Cannot resolve name: %s\n", gai_strerror(err));
		return -1;
	}
	
	fprintf(stderr, "Creating client handle\n");
	cl = clnttcp_nb_create((struct sockaddr_in *)srv_addr->ai_addr,
			MOUNT_PROGRAM, MOUNT_V3, &sockp, 0, 0);
	freeaddrinfo(srv_addr);

	if(cl == NULL) {
		fprintf(stderr, "Client handle creation failed\n");
		return -1;
	}

	ucbi.callback = mount_cb;
	ucbi.cb_private = NULL;
	rpc.proc = MOUNT3_MNT;
	rpc.inproc = (xdrproc_t)xdr_dirpath;
	rpc.inargs = (caddr_t)&argv[2];
	fprintf(stdout, "Sending mount call\n");
	stat = clnttcp_nb_call(cl, rpc, ucbi, RPC_DEFAULT_FLAGS);

	if(stat == RPC_SUCCESS)
		fprintf(stderr, "Mount call sent with success\n");
	else
		fprintf(stderr, "Mount call failed\n");

	fprintf(stdout, "Blocking wait for reply..\n");
	clnttcp_nb_receive(cl, RPC_BLOCKING_WAIT);
	fprintf(stdout, "Mount reply received.\n");

	clnttcp_nb_destroy(cl);
	return 0;
}


