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



#ifndef _NFS_CTX_H_
#define _NFS_CTX_H_
#include <sys/socket.h>

#include <clnt_tcp_nb.h>

#define NFSC_CFL_NONBLOCKING		0x01
#define NFSC_CFL_BLOCKING		0x02
#define NFSC_CFL_DISABLE_NAGLE		0x04

/* Stores the state of each remote NFS mount
 * performed by the client
 */
typedef struct _nfs_ctx {
	/* Socket for identifying the NFS server IP and port */
	struct sockaddr_in *nfs_srv;

	/* Socket for identifying the MOUNT server IP and port */
	struct sockaddr_in *nfs_mnt;
	int nfs_transport;	/* IPPROTO_TCP or IPPROTO_UDP */
	int nfs_errno;		/* Reason for previous error */	
	
	/* RPC handle for the connection to mountd */
	CLIENT *nfs_mnt_cl;
	
	/* RPC handle for the connection to nfsd */
	CLIENT *nfs_cl;

	/* Connection flags */
	int nfs_connflags;

	/* Read frag size */
	int nfs_rsize;

	/* Write frag size */
	int nfs_wsize;

}nfs_ctx;

extern int check_ctx(nfs_ctx *);
#endif
