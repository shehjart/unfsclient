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

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <nfsclientd.h>
#include <assert.h>
#include <netdb.h>


struct fuse_opt nfsclientd_fuseopts[] = {
		{"--server=%s", offsetof(struct nfsclientd_opts, server), 0},
		{"--remotedir=%s", offsetof(struct nfsclientd_opts, remotedir), 0},
		{"--ctxpoolsize=%d", offsetof(struct nfsclientd_opts, ctxpoolsize),
			0},
		FUSE_OPT_END
	};

void
usage()
{
	printf("USAGE: nfsclientd [OPTIONS] <mountpoint>\n");
	printf("Options\n");
	printf("\t--server=<server>\n");
	printf("\t--remotedir=<server_exported_directory>\n");
	printf("\t--ctxpoolsize=<ctx_pool_size>\n");

	return;
}

static struct fuse_lowlevel_ops nfsclientd_ops = {
	.init		= nfscd_mount_init,
	.destroy	= nfscd_destroy,
/*
	.lookup		= nfscd_lookup,
	.forget		= nfscd_forget,
	.getattr	= nfscd_getattr,
	.setattr	= nfscd_setattr,
	.readlink	= nfscd_readlink,
	.mknod		= nfscd_mknod,
	.mkdir		= nfscd_mkdir,
	.unlink		= nfscd_unlink,
	.rmdir		= nfscd_rmdir,
	.symlink	= nfscd_symlink,
	.rename		= nfscd_rename,
	.link		= nfscd_link,
	.open		= nfscd_open,
	.read		= nfscd_read,
	.write		= nfscd_write,
	.flush		= nfscd_flush,
	.release	= nfscd_release,
	.fsync		= nfscd_fsync,
	.opendir	= nfscd_opendir,
	.readdir	= nfscd_readdir,
	.releasedir	= nfscd_releasedir,
	.fsyncdir	= nfscd_fsyncdir,
	.statfs		= nfscd_statfs,
	.access		= nfscd_access,
	.create		= nfscd_create
*/
};

static struct nfsclientd_context * 
init_nfsclientd_context(struct nfsclientd_opts opts)
{
	struct nfsclientd_context * ctx = NULL;
	ctx = (struct nfsclientd_context *)malloc(sizeof(struct nfsclientd_context));

	assert(ctx != NULL);

	fprintf(stdout, "server: %s, remotedir: %s, mountpoint: %s,"
			" ctxpoolsize: %d\n", opts.server, opts.remotedir,
			opts.mountpoint, opts.ctxpoolsize);
	memset(ctx, 0, sizeof(struct nfsclientd_context));
	ctx->mountopts.server = strdup(opts.server);
	ctx->mountopts.remotedir = strdup(opts.remotedir);
	ctx->mountopts.mountpoint = strdup(opts.mountpoint);
	ctx->mountopts.srvaddr = opts.srvaddr;
	ctx->mountopts.ctxpoolsize = opts.ctxpoolsize;

	if((opts.ctxpoolsize <= 0) || (opts.ctxpoolsize > MAX_CTXPOOL_SIZE)) {
		fprintf(stderr,"nfsclientd: Context pool must be between 1-%d\n",
				MAX_CTXPOOL_SIZE);
		return NULL;
	}

	return ctx;
}

int
main(int argc, char * argv[])
{
	struct fuse_args fuseargs = FUSE_ARGS_INIT(argc, argv);
	struct fuse_chan *fusechan = NULL;
	struct nfsclientd_opts options;
	char * mountpoint = NULL;
	struct fuse_session * se = NULL;
	struct nfsclientd_context * nfscd_ctx = NULL;
	struct addrinfo *srv_addr, hints;
	int err;
	
	options.server = options.remotedir = NULL;
	options.ctxpoolsize = DEFAULT_CTXPOOL_SIZE;
	if((fuse_opt_parse(&fuseargs, &options, nfsclientd_fuseopts, NULL)) < 0) {
		fprintf(stderr, "FUSE could not parse options.\n");
		return -1;
	}

	if((options.server == NULL) || (options.remotedir == NULL)) {
		fprintf(stderr, "NFS server and remote export directory must be"
				" specified.\n");
		return -1;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	if((err = getaddrinfo(options.server, NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "nfsclientd: Cannot resolve name: %s: %s\n",
				options.server, gai_strerror(err));
		return -1;
	}

	options.srvaddr = (struct sockaddr_in *)srv_addr->ai_addr;

	if(fuse_parse_cmdline(&fuseargs, &mountpoint, NULL, NULL) < 0) {
		fprintf(stderr, "fuse could not parse arguments.\n");
		return -1;
	}

	options.mountpoint = mountpoint;
	if((nfscd_ctx = init_nfsclientd_context(options)) == NULL) {
		fprintf(stderr, "nfsclientd: Cannot init nfsclientd context.\n");
		return -1;
	}

	if((fusechan = fuse_mount(mountpoint, &fuseargs)) == NULL) {
		fprintf(stderr, "fuse could not mount.\n");
		return -1;
	}
	if((se = fuse_lowlevel_new(&fuseargs, &nfsclientd_ops,
		sizeof(struct fuse_lowlevel_ops), nfscd_ctx)) == NULL) {
		fprintf(stderr, "fuse could not create mount.\n");
		goto unmount_exit;	
	}
	
	if(fuse_set_signal_handlers(se) < 0) {
		fprintf(stderr, "fuse could not set signal handlers.\n");
		goto session_destroy_exit;
	}

	fuse_session_add_chan(se, fusechan);
	fuse_session_loop_mt(se);
	fuse_opt_free_args(&fuseargs);

session_destroy_exit:
	fuse_session_destroy(se);

unmount_exit:
	fuse_unmount(mountpoint, fusechan);

	return 0;
}
