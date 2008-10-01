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


#include <rpc/rpc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#include <nfsclient.h>
#include <clnt_tcp_nb.h>

int main(int argc, char *argv[])
{
	CLIENT *cl = NULL;
	struct addrinfo *srv_addr, hints;
	int sockp = RPC_ANYSOCK;
	int err;

	if(argc < 2) {
		fprintf(stderr, "Not enough arguments\n"
				"This tests the asynchronous clnt_create "
				"implementation for name resolution, connection setup and port mapping."
				"\nUSAGE: clnt_create <server>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	fprintf(stdout, "Resolving address %s\n", argv[1]);
	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "Cannot resolve name: %s\n", gai_strerror(err));
		return -1;
	}
	
	fprintf(stdout, "Creating client handle\n");
	cl = clnttcp_nb_create((struct sockaddr_in *)srv_addr->ai_addr, MOUNT_PROGRAM,
			MOUNT_V3, &sockp, 0, 0);

	if(cl == NULL) {
		fprintf(stderr, "Client creation failed\n");
		return -1;
	}

	fprintf(stdout, "Destroying client handle\n");
	clnttcp_nb_destroy(cl);
	fprintf(stderr, "Done\n");
	return 0;
}


