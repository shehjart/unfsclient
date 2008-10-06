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

#define FUSE_USE_VERSION	26

#include <fuse_lowlevel.h>
#include <nfsclient.h>
#include <flist.h>

#define DEFAULT_CTXPOOL_SIZE	1
#define MAX_CTXPOOL_SIZE	20

#define DEFAULT_THREADPOOL_SIZE		1
#define MAX_THREADPOOL_SIZE		20

struct nfsclientd_opts {
	char * server;
	char * remotedir;
	char * mountpoint;

	struct sockaddr_in * srvaddr;
	int ctxpoolsize;
	int threadpool;
};

struct nfsclientd_context {

	/* The libnfsclient context pool. */
	nfs_ctx ** nfsctx_pool;

	/* The options and configurables */
	struct nfsclientd_opts mountopts;

	/* Metadata request queue */
	struct flist_head md_rq;

	/* Read write request queue */
	struct flist_head rw_rq;

	/* Protocol private state. */
};



/* nfsclientd's FUSE operations. */
extern void nfscd_init(void *userdata, struct fuse_conn_info *conn);
extern void nfscd_destroy(void *userdata);
