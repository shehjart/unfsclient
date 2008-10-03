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


/* 
 * This is the main file that contains the nfsclientd started using
 * libfuse.
 */


#define FUSE_USE_VERSION 26

#include <fuse_lowlevel.h>
#include <nfsclient.h>

#include <stdio.h>
#include <string.h>
#include <stddef.h>


struct nfsclientd_opts {
	char * server;
	char * remotedir;
};


void
usage()
{
	printf("USAGE: nfsclientd [OPTIONS] <mountpoint>\n");
	printf("Options\n");
	printf("\t--server <server>\n");
	printf("\t--remotedir <server_exported_directory>\n");
	return;
}

int
main(int argc, char * argv[])
{
	struct fuse_args fuseargs = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *fusechan = NULL;
	struct nfsclientd_opts options;

	struct fuse_opt nfsclientd_fuseopts[] = {
		{"--server=%s", offsetof(struct nfsclientd_opts, server), 0},
		{"--remotedir=%s", offsetof(struct nfsclientd_opts, remotedir), 0},
		FUSE_OPT_END
	};

	options.server = options.remotedir = NULL;
	if((fuse_opt_parse(&fuseargs, &options, nfsclientd_fuseopts, NULL)) < 0) {
		fprintf(stderr, "FUSE could not parse options.\n");
		return -1;
	}

	if((options.server == NULL) || (options.remotedir == NULL)) {
		fprintf(stderr, "NFS server and remote export directory must be"
				" specified.\n");
		return -1;
	}

	if(fuse_parse_cmdline(&fuseargs, NULL, NULL, NULL) < 0) {
		fprintf(stderr, "fuse could not parse arguments.\n");
		return -1;
	}

	fprintf(stdout, "server: %s, remotedir: %s\n", options.server,
			options.remotedir);
	return 0;
}

