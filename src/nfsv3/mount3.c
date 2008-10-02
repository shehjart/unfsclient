/*
 *    libnfsclient2, second incarnation of libnfsclient, a library for
 *    NFS operations from user space.
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available here:
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


#include <sys/types.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netdb.h>
#include <rpc/pmap_clnt.h>

#include <nfs3.h>
#include <nfs_ctx.h>
#include <clnt_tcp_nb.h>


/* Common internal interface to MOUNT protocol */
static enum clnt_stat
mount3_call(int proc, void *arg, xdrproc_t xdr_proc,
		nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	int sockp = RPC_ANYSOCK;
	enum clnt_stat call_stat;
	struct callback_info cbi;

	if(!check_ctx(ctx))
		return RPC_SYSTEMERROR;

	if(ctx->nfs_mnt_cl == NULL)
		ctx->nfs_mnt_cl = clnttcp_b_create(ctx->nfs_mnt,
				MOUNT_PROGRAM, MOUNT_V3, &sockp,
				0, 0);

	if(ctx->nfs_mnt_cl == NULL)
		return RPC_SYSTEMERROR;

	cbi.callback = u_cb;
	cbi.cb_private = priv;
	call_stat = clnttcp_nb_call(ctx->nfs_mnt_cl, proc, xdr_proc, arg, cbi);

	return call_stat;
}


enum clnt_stat
mount3_mnt(dirpath *dirp, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return	mount3_call(MOUNT3_MNT, (caddr_t)dirp,
			(xdrproc_t)xdr_dirpath, ctx, u_cb, priv);

}


enum clnt_stat
mount3_null(nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return mount3_call(MOUNT3_NULL, NULL, (xdrproc_t)xdr_void,
			ctx, u_cb, priv);
}


enum clnt_stat 
mount3_dump(nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return mount3_call(MOUNT3_DUMP, NULL, (xdrproc_t)xdr_void,
			ctx, u_cb, priv);
}


enum clnt_stat 
mount3_umnt(dirpath *dirp, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return mount3_call(MOUNT3_UMNT, (caddr_t)dirp, 
			(xdrproc_t)xdr_dirpath, ctx, u_cb, priv);
}


enum clnt_stat 
mount3_umntall(nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return mount3_call(MOUNT3_UMNTALL, NULL, (xdrproc_t)xdr_void,
			ctx, u_cb, priv);
}

enum clnt_stat 
mount3_export(nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return mount3_call(MOUNT3_EXPORT, NULL, (xdrproc_t)xdr_void,
			ctx, u_cb, priv);
}


mountres3 *
xdr_to_mntres3(char *msg, int len)
{
	XDR xdr;
	mountres3 *mntres = NULL; 

	if(msg == NULL)
		return NULL;

	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	mntres = (mountres3 *)mem_alloc(sizeof(mountres3));
	if(mntres == NULL)
		return NULL;

	/* xdr_mountres3 and subsequent xdr decoding calls assume this
	 * to be NULL if we want them to do the memory allocation for
	 * the auth array. Make sure this is NULL here, because
	 * there's a bunch of embedded structs in mntres3, its
	 * possible that this pointer ends up being non-NULL but
	 * pointing to a memory we did not allocate.
	 */
	mntres->mountres3_u.mountinfo.auth_flavors.auth_flavors_val = NULL;
	mntres->mountres3_u.mountinfo.fhandle.fhandle3_val = NULL;
	if(!xdr_mountres3(&xdr, mntres)) {
		mem_free(mntres, sizeof(mntres3));
		return NULL;
	}

	return mntres;
}


mountlist
xdr_to_mountlist(char * msg, int len)
{
	XDR xdr;
	mountbody * mb = NULL;

	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	mb = (mountbody *)mem_alloc(sizeof(mountbody));
	if(mb == NULL)
		return NULL;

	mb->ml_hostname = NULL;
	mb->ml_directory = NULL;
	mb->ml_next = NULL;

	if(!xdr_mountlist(&xdr, &mb)) {
		mem_free(mb, sizeof(mountbody));
		return NULL;
	}

	return mb;
}


exports 
xdr_to_exports(char * msg, int len)
{
	exportnode * ex = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	ex = (exportnode *)mem_alloc(sizeof(exportnode));
	if(ex == NULL)
		return NULL;

	ex->ex_dir = NULL;
	ex->ex_groups = NULL;
	ex->ex_next = NULL;

	if(!xdr_exports(&xdr, &ex)) {
		mem_free(ex, sizeof(exportnode));
		return NULL;
	}

	return ex;
}


void 
free_mntres3(mountres3 *msg)
{
	if(msg == NULL)
		return;

	if(msg->fhs_status == MNT3_OK) {
		mem_free(msg->mountres3_u.mountinfo.fhandle.fhandle3_val,
				msg->mountres3_u.mountinfo.fhandle.fhandle3_len);
	}

	mem_free(msg, sizeof(mountres3));
}

void 
free_mountlist(mountlist msg)
{
	mountlist list = NULL;
	if(msg == NULL)
		return;

	list = msg;
	while(list != NULL) {
		msg = list->ml_next;
		mem_free(list->ml_hostname, strlen(list->ml_hostname) + 1);
		mem_free(list->ml_directory, strlen(list->ml_hostname) + 1);
		mem_free(list, sizeof(mountbody));
		list = msg;
	}
	
	return;
}


void
free_groups(groups gr)
{
	groups n;
	if(gr == NULL)
		return;
	n = gr;
	while(n != NULL) {
		gr = n->gr_next;
		mem_free(n->gr_name, strlen(n->gr_name) + 1);
		mem_free(n, sizeof(groupnode));
		n = gr;
	}

	return;
}

void 
free_exports(exports ex)
{
	exports en = NULL;
	if(ex == NULL)
		return;

	en = ex;
	while(en != NULL) {
		ex = en->ex_next;
		mem_free(en->ex_dir, strlen(en->ex_dir) + 1);
		free_groups(en->ex_groups);	
		en = ex;
	}


	return;
}



