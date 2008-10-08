/*
 *    libnfsclient2, second incarnation of libnfsclient, a library for
 *    NFS operations from user space.
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available here:
 *
 *    http://nfsreplay.sourceforge.net
 *    
 *    and
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


/*
 * This file is included by all the programs that want to use the
 * libnfsclient.
 */

#ifndef _NFSCLIENT_H_
#define _NFSCLIENT_H_

#include <sys/socket.h>

#include <nfs3.h>
#include <nfs_ctx.h>
#include <clnt_tcp_nb.h>

extern nfs_ctx *nfs_init(struct sockaddr_in *srv, int proto, int connflags);
extern void mnt_complete(nfs_ctx * ctx);
extern int nfs_complete(nfs_ctx * ctx, int flag);
extern void nfs_destroy(nfs_ctx * ctx);
#endif

