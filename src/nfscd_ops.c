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

#define FUSE_USE_VERSION 26


#include <fuse_lowlevel.h>
#include <nfsclient.h>
#include <nfsclientd.h>

#include <stdio.h>
#include <assert.h>

void
nfscd_mount_init(void *userdata, struct fuse_conn_info *conn)
{
	struct nfsclientd_context * ctx = NULL;
	
	assert(userdata != NULL);
	ctx = (struct nfsclientd_context *)userdata;

	fprintf(stdout, "nfscd_mount_init: server: %s, remotedir: %s, mountpoint: "
			"%s\n", ctx->mountopts.server, ctx->mountopts.remotedir,
			ctx->mountopts.mountpoint);

}

void
nfscd_destroy(void *userdata)
{
	fprintf(stderr, "Later..\n");
}
