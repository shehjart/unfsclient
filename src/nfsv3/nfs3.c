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


#include <nfs3.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <errno.h>

const char nfsv3_proc_vals[22][15]= {
        "NULL" ,
        "GETATTR",
        "SETATTR",
        "LOOKUP",
        "ACCESS",
        "READLINK",
        "READ",
        "WRITE",
        "CREATE",
        "MKDIR",
        "SYMLINK",
        "MKNOD" ,
        "REMOVE",
        "RMDIR",
        "RENAME",
        "LINK",
        "READDIR",
        "READDIRPLUS",
        "FSSTAT",
        "FSINFO",
        "PATHCONF",
        "COMMIT"
};



struct nfs3stat_to_str {
	nfsstat3 stat;
	char errstr[100];
};


struct nfs3stat_to_str nfs3stat_str_tbl[] = {
	{ NFS3_OK,		"Call completed successfully."			},
	{ NFS3ERR_PERM,		"Not owner"					},
	{ NFS3ERR_NOENT,	"No such file or directory"			},
	{ NFS3ERR_IO,		"I/O error"					},
	{ NFS3ERR_NXIO, 	"I/O error"					},
	{ NFS3ERR_ACCES,	"Permission denied"				},
	{ NFS3ERR_EXIST,	"File exists"					},
	{ NFS3ERR_XDEV,		"Attempt to do a cross-device hard link"	},
	{ NFS3ERR_NODEV,	"No such device"				},
	{ NFS3ERR_NOTDIR,	"Not a directory"				},
	{ NFS3ERR_ISDIR,	"Not a directory"				},
	{ NFS3ERR_INVAL,	"Invalid argument for operation"		},
	{ NFS3ERR_FBIG,		"File too large"				},
	{ NFS3ERR_NOSPC,	"No space left on device"			},
	{ NFS3ERR_ROFS,		"Read-only file system"				},
	{ NFS3ERR_MLINK,	"Too many hard links"				},
	{ NFS3ERR_NAMETOOLONG,	"Filename in operation was too long"		},
	{ NFS3ERR_NOTEMPTY,	"Directory not empty"				},
	{ NFS3ERR_DQUOT,	"Resource (quota) hard limit exceeded"		},
	{ NFS3ERR_STALE,	"Invalid file handle"				},
	{ NFS3ERR_REMOTE,	"Too many levels of remote in path"		},
	{ NFS3ERR_BADHANDLE, 	"Illegal NFS file handle"			},
	{ NFS3ERR_NOT_SYNC,	"Update synchronization mismatch detected"	},
	{ NFS3ERR_BAD_COOKIE,	"READDIR or READDIRPLUS cookie is stale"	},
	{ NFS3ERR_NOTSUPP,	"Operation is not supported"			},
	{ NFS3ERR_TOOSMALL,	"Buffer or request is too small"		},
	{ NFS3ERR_SERVERFAULT,	"Error occurred on the server or IO Error"	},
	{ NFS3ERR_BADTYPE, 	"Type not supported by the server"		},
	{ NFS3ERR_JUKEBOX,	"Cannot complete server initiated request"	},
	{ -1		,	"IO Error"	}
};


char *
nfsstat3_strerror(int stat)
{
        int i;
	for(i = 0; nfs3stat_str_tbl[i].stat != -1; i++) {
		if (nfs3stat_str_tbl[i].stat == stat)
			return nfs3stat_str_tbl[i].errstr;
	}

	return nfs3stat_str_tbl[i].errstr;
}


static enum clnt_stat
nfs3_call(int proc, void *arg, xdrproc_t xdr_proc,
		nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	int sockp = RPC_ANYSOCK;
	int flag = 1;
	struct callback_info cbi;

	if(!check_ctx(ctx))
		return RPC_SYSTEMERROR;

	if(ctx->nfs_cl == NULL) {
		if(ctx->nfs_connflags & NFSC_CFL_NONBLOCKING)
			ctx->nfs_cl = clnttcp_nb_create(ctx->nfs_srv, NFS_PROGRAM,
					NFS_V3, &sockp,	ctx->nfs_wsize,
					ctx->nfs_rsize);
		else
			ctx->nfs_cl = clnttcp_b_create(ctx->nfs_srv, NFS_PROGRAM,
					NFS_V3, &sockp,	ctx->nfs_wsize,
					ctx->nfs_rsize);

		if(ctx->nfs_connflags & NFSC_CFL_DISABLE_NAGLE)
			setsockopt(sockp, IPPROTO_TCP, TCP_NODELAY, (char *)&flag,
					sizeof(flag));
	}

	if(ctx->nfs_cl == NULL)
		return RPC_SYSTEMERROR;

	cbi.callback = u_cb;
	cbi.cb_private = priv;
	return clnttcp_nb_call(ctx->nfs_cl, proc, xdr_proc, (caddr_t)arg, cbi);

}


enum clnt_stat 
nfs3_null(nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_NULL, NULL, (xdrproc_t)xdr_void,
			ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_lookup(LOOKUP3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_LOOKUP, args, (xdrproc_t)xdr_LOOKUP3args,
			ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_getattr(GETATTR3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_GETATTR, args,
			(xdrproc_t)xdr_GETATTR3args, ctx, u_cb, priv);
}
//#define NFS3_SETATTR 2
enum clnt_stat 
nfs3_setattr(SETATTR3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_SETATTR, args,
			(xdrproc_t)xdr_SETATTR3args, ctx, u_cb, priv);
}

enum clnt_stat 
nfs3_access(ACCESS3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_ACCESS, args,
			(xdrproc_t)xdr_ACCESS3args, ctx, u_cb, priv);
}

enum clnt_stat 
nfs3_readlink(READLINK3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_READLINK, args,
			(xdrproc_t)xdr_READLINK3args, ctx, u_cb, priv);
}

enum clnt_stat 
nfs3_read(READ3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_READ, args, 
			(xdrproc_t)xdr_READ3args, ctx, u_cb, priv);
}

enum clnt_stat 
nfs3_write(WRITE3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_WRITE, args,
			(xdrproc_t)xdr_WRITE3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_create(CREATE3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_CREATE, args,
			(xdrproc_t)xdr_CREATE3args, ctx, u_cb, priv);
}

enum clnt_stat 
nfs3_mkdir(MKDIR3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_MKDIR, args,
			(xdrproc_t)xdr_MKDIR3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_symlink(SYMLINK3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_SYMLINK, args,
			(xdrproc_t)xdr_SYMLINK3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_rename(RENAME3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_RENAME, args,
			(xdrproc_t)xdr_RENAME3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_mknod(MKNOD3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_MKNOD, args, 
			(xdrproc_t)xdr_MKNOD3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_remove(REMOVE3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_REMOVE, args,
			(xdrproc_t)xdr_REMOVE3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_rmdir(RMDIR3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_RMDIR, args,
			(xdrproc_t)xdr_RMDIR3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_link(LINK3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_LINK, args,
			(xdrproc_t)xdr_LINK3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_readdir(READDIR3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_READDIR, args,
			(xdrproc_t)xdr_READDIR3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_fsstat(FSSTAT3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_FSSTAT, args,
			(xdrproc_t)xdr_FSSTAT3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_readdirplus(READDIRPLUS3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_READDIRPLUS, args,
			(xdrproc_t)xdr_READDIRPLUS3args, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_fsinfo(FSINFOargs *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_FSINFO, args,
			(xdrproc_t)xdr_FSINFOargs, ctx, u_cb, priv);
}


enum clnt_stat 
nfs3_pathconf(PATHCONF3args *args, nfs_ctx *ctx, user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_PATHCONF, args,
			(xdrproc_t)xdr_PATHCONF3args, ctx, u_cb, priv);
}


enum clnt_stat nfs3_commit(COMMIT3args *args, nfs_ctx *ctx,
		user_cb u_cb, void * priv)
{
	return nfs3_call(NFS3_COMMIT, args,
			(xdrproc_t)xdr_COMMIT3args, ctx, u_cb, priv);
}


LOOKUP3res * 
xdr_to_LOOKUP3res(char *msg, int len)
{
	XDR xdr;
	LOOKUP3res *lres = NULL;
	
	if(msg == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	lres = (LOOKUP3res *)mem_alloc(sizeof(LOOKUP3res));
	if(lres == NULL)
		return NULL;

	lres->LOOKUP3res_u.resok.object.data.data_val = NULL;
	if(!xdr_LOOKUP3res(&xdr, lres)) {
		mem_free(lres, sizeof(LOOKUP3res));
		return NULL;
	}

	return lres;
}

void 
free_LOOKUP3res(void * msg)
{
	LOOKUP3res * res = NULL;
	if(msg == NULL)
		return;
	
	res = (LOOKUP3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->LOOKUP3res_u.resok.object.data.data_val,
				res->LOOKUP3res_u.resok.object.data.data_len);

	mem_free(res, sizeof(LOOKUP3res));
}



LOOKUP3args * 
xdr_to_LOOKUP3args(char *msg, int len)
{
	XDR xdr;
	LOOKUP3args *args = NULL;
	
	if(msg == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args = (LOOKUP3args *)mem_alloc(sizeof(LOOKUP3args));
	if(args == NULL)
		return NULL;

	args->what.name = NULL;
	args->what.dir.data.data_val = NULL;

	if(!xdr_LOOKUP3args(&xdr, args)) {
		mem_free(args, sizeof(LOOKUP3args));
		return NULL;
	}

	return args;
}


void
free_LOOKUP3args(void * msg)
{
	LOOKUP3args *args = (LOOKUP3args *)msg;
	if(msg == NULL)
		return;
	
	args = (LOOKUP3args *)msg;

	mem_free(args->what.name, strlen(args->what.name) + 1);
	mem_free(args->what.dir.data.data_val,
			args->what.dir.data.data_len);

	mem_free(args, sizeof(LOOKUP3args));

	return;
}


GETATTR3res * 
xdr_to_GETATTR3res(char *msg, int len)
{
	XDR xdr;
	GETATTR3res *res = NULL;
	
	if(msg == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	res = (GETATTR3res *)mem_alloc(sizeof(GETATTR3res));
	if(res == NULL)
		return NULL;

	if(!xdr_GETATTR3res(&xdr, res)) {
		mem_free(res, sizeof(GETATTR3res));
		return NULL;
	}

	return res;
}

void 
free_GETATTR3res(void *msg)
{
	GETATTR3res *res = NULL;
	if(msg == NULL)
		return;

	res = (GETATTR3res *)msg;
	mem_free(res, sizeof(GETATTR3res));
	return;
}

GETATTR3args * 
xdr_to_GETATTR3args (char *msg, int len)
{
	XDR xdr;
	GETATTR3args *args = NULL;
	
	if(msg == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args = (GETATTR3args *)mem_alloc(sizeof(GETATTR3args));
	if(args == NULL)
		return NULL;

	args->object.data.data_val = NULL;
	if(!xdr_GETATTR3args(&xdr, args)) {
		mem_free(args, sizeof(GETATTR3args));
		return NULL;
	}

	return args;
}

void 
free_GETATTR3args (void * msg)
{
	GETATTR3args *args = NULL;
	if(msg == NULL)
		return;
	
	args = (GETATTR3args *)msg;

	mem_free(args->object.data.data_val, args->object.data.data_len);
	mem_free(args, sizeof(GETATTR3args));

	return;
}


SETATTR3res *
xdr_to_SETATTR3res(char *msg, int len)
{
	SETATTR3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (SETATTR3res *)mem_alloc(sizeof(SETATTR3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	if(!xdr_SETATTR3res(&xdr, res)) {
		mem_free(res, sizeof(SETATTR3res));
		return NULL;
	}

	return res;
}


void
free_SETATTR3res(void * msg)
{
	SETATTR3res * res = NULL;
	if(msg == NULL)
		return;

	res = (SETATTR3res *)msg;
	mem_free(res, sizeof(SETATTR3res));

	return;
}


SETATTR3args *
xdr_to_SETATTR3args (char *msg, int len)
{
	SETATTR3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (SETATTR3args *)mem_alloc(sizeof(SETATTR3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->object.data.data_val = NULL;
	if(!xdr_SETATTR3args(&xdr, args)) {
		mem_free(args, sizeof(SETATTR3args));
		return NULL;
	}

	return args;
}

void
free_SETATTR3args (void * msg)
{
	SETATTR3args *args = NULL;
	if(msg == NULL)
		return;

	args = (SETATTR3args *)msg;

	mem_free(args->object.data.data_val, args->object.data.data_len);
	mem_free(args, sizeof(SETATTR3args));

	return;
}


ACCESS3res *
xdr_to_ACCESS3res(char *msg, int len)
{
	ACCESS3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (ACCESS3res *)mem_alloc(sizeof(ACCESS3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	if(!xdr_ACCESS3res(&xdr, res)) {
		mem_free(res, sizeof(ACCESS3res));
		return NULL;
	}

	return res;
}

	
void
free_ACCESS3res(void * msg)
{
	ACCESS3res *res = NULL;
	if(msg == NULL)
		return;

	res = (ACCESS3res *)msg;
	mem_free(res, sizeof(ACCESS3res));

	return;
}


ACCESS3args *
xdr_to_ACCESS3args(char *msg, int len)
{
	ACCESS3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (ACCESS3args *)mem_alloc(sizeof(ACCESS3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->object.data.data_val = NULL;
	if(!xdr_ACCESS3args(&xdr, args)) {
		mem_free(args, sizeof(ACCESS3args));
		return NULL;
	}

	return args;
}
  

void
free_ACCESS3args(void *msg)
{
	ACCESS3args *args = NULL;
	if(msg == NULL)
		return;

	args = (ACCESS3args *)msg;

	mem_free(args->object.data.data_val, args->object.data.data_len);
	mem_free(args, sizeof(ACCESS3args));
	return;
}
  

READLINK3res * 
xdr_to_READLINK3res(char *msg, int len)
{
	READLINK3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (READLINK3res *)mem_alloc(sizeof(READLINK3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	res->READLINK3res_u.resok.data = NULL;
	if(!xdr_READLINK3res(&xdr, res)) {
		mem_free(res, sizeof(READLINK3res));
		return NULL;
	}

	return res;
}

	
void 
free_READLINK3res(void *msg)
{
	READLINK3res *res = NULL;
	if(msg == NULL)
		return;

	res = (READLINK3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->READLINK3res_u.resok.data,
				strlen(res->READLINK3res_u.resok.data) + 1);
	
	mem_free(res, sizeof(READLINK3res));

	return;
}



READLINK3args * 
xdr_to_READLINK3args(char *msg, int len)
{
	READLINK3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (READLINK3args *)mem_alloc(sizeof(READLINK3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->symlink.data.data_val = NULL;
	if(!xdr_READLINK3args(&xdr, args)) {
		mem_free(args, sizeof(READLINK3args));
		return NULL;
	}

	return args;
}


void 
free_READLINK3args(void *msg)
{
	READLINK3args *args = NULL;
	if(msg == NULL)
		return;

	args = (READLINK3args *)msg;
	mem_free(args->symlink.data.data_val, args->symlink.data.data_len);
	mem_free(args, sizeof(READLINK3args));
	return;
}



READ3res * 
xdr_to_READ3res(char *msg, int len, int data_xdr)
{
	READ3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (READ3res *)mem_alloc(sizeof(READ3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	if(data_xdr == NFS3_DATA_NO_DEXDR)
		xdr.x_public = __DISABLE_DATA_DEXDR_INTERNAL;
	else
		xdr.x_public = __ENABLE_DATA_DEXDR_INTERNAL;

	res->READ3res_u.resok.data.data_val = NULL;
	if(!xdr_READ3res(&xdr, res)) {
		mem_free(res, sizeof(READ3res));
		return NULL;
	}

	return res;
}


void
free_READ3res(void *msg, int data_xdr)
{
	READ3res *res = NULL;
	if(msg == NULL)
		return;

	res = (READ3res *)msg;

	if((res->status == NFS3_OK) && (data_xdr == NFS3_DATA_DEXDR))
		mem_free(res->READ3res_u.resok.data.data_val,
				res->READ3res_u.resok.data_len);

	mem_free(res, sizeof(READ3res));
	return;
}



READ3args *
xdr_to_READ3args(char *msg, int len)
{
	READ3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (READ3args *)mem_alloc(sizeof(READ3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->file.data.data_val = NULL;
	if(!xdr_READ3args(&xdr, args)) {
		mem_free(args, sizeof(READ3args));
		return NULL;
	}

	return args;
}


void
free_READ3args(void *msg)
{
	READ3args *args = NULL;
	if(msg == NULL)
		return;

	args = (READ3args *)msg;
	mem_free(args->file.data.data_val, args->file.data.data_len);
	mem_free(args, sizeof(READ3args));
	return;
}



WRITE3res * 
xdr_to_WRITE3res(char *msg, int len)
{
	WRITE3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (WRITE3res *)mem_alloc(sizeof(WRITE3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	if(!xdr_WRITE3res(&xdr, res)) {
		mem_free(res, sizeof(WRITE3res));
		return NULL;
	}

	return res;
}



void 
free_WRITE3res(void *msg)
{
	WRITE3res *res = NULL;
	if(msg == NULL)
		return;

	res = (WRITE3res *)msg;
	mem_free(res, sizeof(WRITE3res));
	return;
}


WRITE3args * 
xdr_to_WRITE3args(char *msg, int len, int data_xdr)
{
	WRITE3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (WRITE3args *)mem_alloc(sizeof(WRITE3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	if(data_xdr == NFS3_DATA_NO_DEXDR)
		xdr.x_public = __DISABLE_DATA_DEXDR_INTERNAL;
	else
		xdr.x_public = __ENABLE_DATA_DEXDR_INTERNAL;

	args->file.data.data_val = NULL;
	if(!xdr_WRITE3args(&xdr, args)) {
		mem_free(args, sizeof(WRITE3args));
		return NULL;
	}

	return args;
}


void
free_WRITE3args(void *msg, int data_xdr)
{
	WRITE3args *args = NULL;
	if(msg == NULL)
		return;

	args = (WRITE3args *)msg;
	mem_free(args->file.data.data_val, args->file.data.data_len);

	if(data_xdr == NFS3_DATA_DEXDR)
		mem_free(args->data.data_val, args->data.data_len);

	mem_free(args, sizeof(WRITE3args));
	return;
}



CREATE3res * 
xdr_to_CREATE3res(char *msg, int len)
{
	CREATE3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (CREATE3res *)mem_alloc(sizeof(CREATE3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_val = NULL;
	if(!xdr_CREATE3res(&xdr, res)) {
		mem_free(res, sizeof(CREATE3res));
		return NULL;
	}

	return res;
}


void
free_CREATE3res(void * msg)
{
	CREATE3res *res = NULL;
	if(msg == NULL)
		return;

	res = (CREATE3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_val, res->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_len);

	mem_free(res, sizeof(CREATE3res));
	return;
}


CREATE3args * 
xdr_to_CREATE3args(char *msg, int len)
{
	CREATE3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (CREATE3args *)mem_alloc(sizeof(CREATE3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->where.dir.data.data_val = NULL;
	args->where.name = NULL;

	if(!xdr_CREATE3args(&xdr, args)) {
		mem_free(args, sizeof(CREATE3args));
		return NULL;
	}

	return args;
}


void 
free_CREATE3args(void *msg)
{
	CREATE3args *args = NULL;
	if(msg == NULL)
		return;

	args = (CREATE3args *)msg;
	
	mem_free(args->where.dir.data.data_val, args->where.dir.data.data_len);
	mem_free(args->where.name, strlen(args->where.name) + 1);
	mem_free(args, sizeof(CREATE3args));
	
	return;
}


MKDIR3res *
xdr_to_MKDIR3res(char *msg, int len)
{
	MKDIR3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (MKDIR3res *)mem_alloc(sizeof(MKDIR3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	res->MKDIR3res_u.resok.obj.post_op_fh3_u.handle.data.data_val = NULL;
	if(!xdr_MKDIR3res(&xdr, res)) {
		mem_free(res, sizeof(MKDIR3res));
		return NULL;
	}

	return res;
}


void
free_MKDIR3res(void *msg)
{
	MKDIR3res *res = NULL;
	if(msg == NULL)
		return;

	res = (MKDIR3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->MKDIR3res_u.resok.obj.post_op_fh3_u.handle.data.data_val,
		res->MKDIR3res_u.resok.obj.post_op_fh3_u.handle.data.data_len);
		
	mem_free(res, sizeof(MKDIR3res));
	return;
}



MKDIR3args * 
xdr_to_MKDIR3args(char *msg, int len)
{
	MKDIR3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (MKDIR3args *)mem_alloc(sizeof(MKDIR3args));
	if(args == NULL)
		return NULL;

	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->where.name = NULL;
	args->where.dir.data.data_val = NULL;

	if(!xdr_MKDIR3args(&xdr, args)) {
		mem_free(args, sizeof(MKDIR3args));
		return NULL;
	}

	return args;
}


void
free_MKDIR3args(void *msg)
{
	MKDIR3args *args = NULL;
	if(msg == NULL)
		return;

	args = (MKDIR3args *)msg;
	mem_free(args->where.name, strlen(args->where.name) + 1);
	mem_free(args->where.dir.data.data_val, args->where.dir.data.data_len);
	mem_free(args, sizeof(MKDIR3args));
	return;
}


SYMLINK3res *
xdr_to_SYMLINK3res(char *msg, int len)
{
	SYMLINK3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (SYMLINK3res *)mem_alloc(sizeof(SYMLINK3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	res->SYMLINK3res_u.resok.obj.post_op_fh3_u.handle.data.data_val = NULL;
	if(!xdr_SYMLINK3res(&xdr, res)) {
		mem_free(res, sizeof(SYMLINK3res));
		return NULL;
	}

	return res;
}


void
free_SYMLINK3res(void * msg)
{
	SYMLINK3res *res = NULL;
	if(msg == NULL)
		return;

	res = (SYMLINK3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->SYMLINK3res_u.resok.obj.post_op_fh3_u.handle.data.data_val,
				res->SYMLINK3res_u.resok.obj.post_op_fh3_u.handle.data.data_len);

	mem_free(res, sizeof(SYMLINK3res));
	return;
}


SYMLINK3args * 
xdr_to_SYMLINK3args(char *msg, int len)
{
	SYMLINK3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (SYMLINK3args *)mem_alloc(sizeof(SYMLINK3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->where.dir.data.data_val = NULL;
	args->where.name = NULL;
	args->symlink.symlink_data = NULL;

	if(!xdr_SYMLINK3args(&xdr, args)) {
		mem_free(args, sizeof(SYMLINK3args));
		return NULL;
	}

	return args;
}

void
free_SYMLINK3args(void * msg)
{
	SYMLINK3args *args = NULL;
	if(msg == NULL)
		return;

	args = (SYMLINK3args *)msg;

	mem_free(args->where.dir.data.data_val, args->where.dir.data.data_len);
	mem_free(args->where.name, strlen(args->where.name) + 1);
	mem_free(args->symlink.symlink_data,strlen(args->symlink.symlink_data) + 1);
	
	mem_free(args, sizeof(SYMLINK3args));
	return;
}



MKNOD3res * 
xdr_to_MKNOD3res(char *msg, int len)
{
	MKNOD3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (MKNOD3res *)mem_alloc(sizeof(MKNOD3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	res->MKNOD3res_u.resok.obj.post_op_fh3_u.handle.data.data_val = NULL;
	if(!xdr_MKNOD3res(&xdr, res)) {
		mem_free(res, sizeof(MKNOD3res));
		return NULL;
	}

	return res;
}


void 
free_MKNOD3res(void *msg)
{
	MKNOD3res *res = NULL;
	if(msg == NULL)
		return;

	res = (MKNOD3res *)msg;
	if(res->status == NFS3_OK)
		mem_free(res->MKNOD3res_u.resok.obj.post_op_fh3_u.handle.data.data_val, res->MKNOD3res_u.resok.obj.post_op_fh3_u.handle.data.data_len);

	mem_free(res, sizeof(MKNOD3res));

	return;
}


MKNOD3args * 
xdr_to_MKNOD3args(char *msg, int len)
{
	MKNOD3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (MKNOD3args *)mem_alloc(sizeof(MKNOD3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->where.dir.data.data_val = NULL;
	args->where.name = NULL;
	if(!xdr_MKNOD3args(&xdr, args)) {
		mem_free(args, sizeof(MKNOD3args));
		return NULL;
	}

	return args;
}

void 
free_MKNOD3args(void *msg)
{
	MKNOD3args *args = NULL;
	if(msg == NULL)
		return;

	args = (MKNOD3args *)msg;
	mem_free(args->where.dir.data.data_val, args->where.dir.data.data_len);
	mem_free(args->where.name, strlen(args->where.name) + 1);
	
	return;
}



REMOVE3res * 
xdr_to_REMOVE3res(char *msg, int len)
{
	REMOVE3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (REMOVE3res *)mem_alloc(sizeof(REMOVE3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	if(!xdr_REMOVE3res(&xdr, res)) {
		mem_free(res, sizeof(REMOVE3res));
		return NULL;
	}

	return res;
}


void 
free_REMOVE3res(void *msg)
{
	REMOVE3res *res = NULL;
	if(msg == NULL)
		return;

	res = (REMOVE3res *)msg;
	mem_free(res, sizeof(REMOVE3res));

	return;
}


REMOVE3args * 
xdr_to_REMOVE3args(char *msg, int len)
{
	REMOVE3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (REMOVE3args *)mem_alloc(sizeof(REMOVE3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->object.dir.data.data_val = NULL;
	args->object.name = NULL;
	if(!xdr_REMOVE3args(&xdr, args)) {
		mem_free(args, sizeof(REMOVE3args));
		return NULL;
	}

	return args;
}


void 
free_REMOVE3args(void *msg)
{
	REMOVE3args *args = NULL;
	if(msg == NULL)
		return;

	args = (REMOVE3args *)msg;
	mem_free(args->object.dir.data.data_val, args->object.dir.data.data_len);
	mem_free(args->object.name, strlen(args->object.name) + 1);
	mem_free(args, sizeof(REMOVE3args));

	return;
}




RMDIR3res * 
xdr_to_RMDIR3res(char *msg, int len)
{
	RMDIR3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (RMDIR3res *)mem_alloc(sizeof(RMDIR3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	if(!xdr_RMDIR3res(&xdr, res)) {
		mem_free(res, sizeof(RMDIR3res));
		return NULL;
	}

	return res;
}

void 
free_RMDIR3res(void *msg)
{
	RMDIR3res *res = NULL;
	if(msg == NULL)
		return;

	res = (RMDIR3res *)msg;
	mem_free(res, sizeof(RMDIR3res));
	return;
}

RMDIR3args * 
xdr_to_RMDIR3args(char *msg, int len)
{
	RMDIR3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (RMDIR3args *)mem_alloc(sizeof(RMDIR3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->object.dir.data.data_val = NULL;
	args->object.name = NULL;
	if(!xdr_RMDIR3args(&xdr, args)) {
		mem_free(args, sizeof(RMDIR3args));
		return NULL;
	}

	return args;
}


void 
free_RMDIR3args(void *msg)
{
	RMDIR3args *args = NULL;
	if(msg == NULL)
		return;

	args = (RMDIR3args *)msg;
	
	mem_free(args->object.dir.data.data_val, args->object.dir.data.data_len);
	mem_free(args->object.name, strlen(args->object.name) + 1);
	mem_free(args, sizeof(RMDIR3args));

	return;
}


RENAME3res * 
xdr_to_RENAME3res(char *msg, int len)
{
	RENAME3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (RENAME3res *)mem_alloc(sizeof(RENAME3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	if(!xdr_RENAME3res(&xdr, res)) {
		mem_free(res, sizeof(RENAME3res));
		return NULL;
	}

	return res;
}

void 
free_RENAME3res(void *msg)
{
	RENAME3res *res = NULL;
	if(msg == NULL)
		return;

	res = (RENAME3res *)msg;
	mem_free(res, sizeof(RENAME3res));

	return;
}



RENAME3args * 
xdr_to_RENAME3args(char *msg, int len)
{
	RENAME3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (RENAME3args *)mem_alloc(sizeof(RENAME3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	args->from.dir.data.data_val = NULL;
	args->from.name = NULL;
	args->to.dir.data.data_val = NULL;
	args->to.name = NULL;

	if(!xdr_RENAME3args(&xdr, args)) {
		mem_free(args, sizeof(RENAME3args));
		return NULL;
	}

	return args;
}


void
free_RENAME3args(void *msg)
{
	RENAME3args *args = NULL;
	if(msg == NULL)
		return;

	args = (RENAME3args *)msg;

	mem_free(args->from.dir.data.data_val, args->from.dir.data.data_len);
	mem_free(args->from.name, strlen(args->from.name) + 1);
	mem_free(args->to.dir.data.data_val, args->to.dir.data.data_len);
	mem_free(args->to.name, strlen(args->to.name) + 1);

	mem_free(args, sizeof(RENAME3args));

	return;
}



LINK3res * 
xdr_to_LINK3res(char *msg, int len)
{
	LINK3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (LINK3res *)mem_alloc(sizeof(LINK3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	
	if(!xdr_LINK3res(&xdr, res)) {
		mem_free(res, sizeof(LINK3res));
		return NULL;
	}

	return res;
}

void 
free_LINK3res(void *msg)
{
	LINK3res *res = NULL;
	if(msg == NULL)
		return;

	res = (LINK3res *)msg;
	mem_free(res, sizeof(LINK3res));
	return;
}


LINK3args 
* xdr_to_LINK3args(char *msg, int len)
{
	LINK3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (LINK3args *)mem_alloc(sizeof(LINK3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->file.data.data_val = NULL;
	args->link.dir.data.data_val = NULL;
	args->link.name = NULL;
	if(!xdr_LINK3args(&xdr, args)) {
		mem_free(args, sizeof(LINK3args));
		return NULL;
	}

	return args;
}


void
free_LINK3args(void *msg)
{
	LINK3args *args = NULL;
	if(msg == NULL)
		return;

	args = (LINK3args *)msg;

	mem_free(args->file.data.data_val, args->file.data.data_len);
	mem_free(args->link.dir.data.data_val, link.dir.data.data_len);
	mem_free(args->link.name, strlen(args->link.name) + 1);
	mem_free(args, sizeof(LINK3args));

	return;
}



READDIR3res * 
xdr_to_READDIR3res(char *msg, int len)
{
	READDIR3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (READDIR3res *)mem_alloc(sizeof(READDIR3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	res->READDIR3res_u.resok.reply.entries = NULL;
	if(!xdr_READDIR3res(&xdr, res)) {
		mem_free(res, sizeof(READDIR3res));
		return NULL;
	}

	return res;
}


void 
free_READDIR3res(void *msg)
{
	entry3 * entry, *tmp;
	entry = tmp = NULL;
	READDIR3res *res = NULL;

	if(msg == NULL)
		return;

	res = (READDIR3res *)msg;

	if(res->status == NFS3_OK) {
		entry = res->READDIR3res_u.resok.reply.entries;
		while(entry != NULL) {
			mem_free(entry->name, strlen(entry->name) + 1);
			tmp = entry->nextentry;
			mem_free(entry, sizeof(struct entry3));
			entry = tmp;
		}
	}

	mem_free(res, sizeof(READDIR3res));
	return;
}


READDIR3args * 
xdr_to_READDIR3args(char *msg, int len)
{
	READDIR3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (READDIR3args *)mem_alloc(sizeof(READDIR3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->dir.data.data_val = NULL;
	if(!xdr_READDIR3args(&xdr, args)) {
		mem_free(args, sizeof(READDIR3args));
		return NULL;
	}

	return args;
}


void
free_READDIR3args(void *msg)
{
	READDIR3args *args = NULL;
	if(msg == NULL)
		return;

	args = (READDIR3args *)msg;
	mem_free(args->dir.data.data_val, args->dir.data.data_len);
	mem_free(args, sizeof(READDIR3args));

	return;
}



READDIRPLUS3res * 
xdr_to_READDIRPLUS3res(char *msg, int len)
{
	READDIRPLUS3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (READDIRPLUS3res *)mem_alloc(sizeof(READDIRPLUS3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	res->READDIRPLUS3res_u.resok.reply.entries = NULL;
	if(!xdr_READDIRPLUS3res(&xdr, res)) {
		mem_free(res, sizeof(READDIRPLUS3res));
		return NULL;
	}

	return res;
}


void 
free_READDIRPLUS3res(void *msg)
{
	entryplus3 * entry, *tmp;
	entry = tmp = NULL;
	READDIRPLUS3res *res = NULL;
	if(msg == NULL)
		return;

	res = (READDIRPLUS3res *)msg;
	if(res->status == NFS3_OK) {
		entry = res->READDIRPLUS3res_u.resok.reply.entries;
		while(entry != NULL) {
			mem_free(entry->name, strlen(entry->name) + 1);
			mem_free(entry->name_handle.post_op_fh3_u.handle.data.data_val, entry->name_handle.post_op_fh3_u.handle.data.data_len);
			tmp = entry->nextentry;
			mem_free(entry, sizeof(struct entryplus3));
			entry = tmp;
		}
	}

	mem_free(res, sizeof(READDIRPLUS3res));

	return;
}



READDIRPLUS3args * 
xdr_to_READDIRPLUS3args(char *msg, int len)
{
	READDIRPLUS3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (READDIRPLUS3args *)mem_alloc(sizeof(READDIRPLUS3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->dir.data.data_val = NULL;
	if(!xdr_READDIRPLUS3args(&xdr, args)) {
		mem_free(args, sizeof(READDIRPLUS3args));
		return NULL;
	}

	return args;
}

void 
free_READDIRPLUS3args(void *msg)
{
	READDIRPLUS3args *args = NULL;
	if(msg == NULL)
		return;

	args = (READDIRPLUS3args *)msg;

	mem_free(args->dir.data.data_val, args->dir.data.data_len);
	mem_free(args, sizeof(READDIRPLUS3args));
	return;
}


FSSTAT3res * 
xdr_to_FSSTAT3res(char *msg, int len)
{
	FSSTAT3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (FSSTAT3res *)mem_alloc(sizeof(FSSTAT3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	if(!xdr_FSSTAT3res(&xdr, res)) {
		mem_free(res, sizeof(FSSTAT3res));
		return NULL;
	}

	return res;
}


void 
free_FSSTAT3res(void *msg)
{
	FSSTAT3res *res = NULL;
	if(msg == NULL)
		return;

	res = (FSSTAT3res *)msg;
	mem_free(res, sizeof(FSSTAT3res));

	return;
}



FSSTAT3args * 
xdr_to_FSSTAT3args(char *msg, int len)
{
	FSSTAT3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (FSSTAT3args *)mem_alloc(sizeof(FSSTAT3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->fsroot.data.data_val = NULL;
	if(!xdr_FSSTAT3args(&xdr, args)) {
		mem_free(args, sizeof(FSSTAT3args));
		return NULL;
	}

	return args;
}



void
free_FSSTAT3args(void *msg)
{
	FSSTAT3args *args = NULL;
	if(msg == NULL)
		return;

	args = (FSSTAT3args *)msg;
	mem_free(args->fsroot.data.data_val, args->fsroot.data.data_len);
	mem_free(args, sizeof(FSSTAT3args));
	return;
}


FSINFO3res * 
xdr_to_FSINFO3res(char *msg, int len)
{
	FSINFO3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (FSINFO3res *)mem_alloc(sizeof(FSINFO3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	if(!xdr_FSINFO3res(&xdr, res)) {
		mem_free(res, sizeof(FSINFO3res));
		return NULL;
	}

	return res;
}


void
free_FSINFO3res(void *msg)
{
	FSINFO3res *res = NULL;
	if(msg == NULL)
		return;

	res = (FSINFO3res *)msg;
	mem_free(res, sizeof(FSINFO3res));

	return;
}



FSINFOargs * 
xdr_to_FSINFOargs(char *msg, int len)
{
	FSINFOargs *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (FSINFOargs *)mem_alloc(sizeof(FSINFOargs));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	args->fsroot.data.data_val = NULL;

	if(!xdr_FSINFOargs(&xdr, args)) {
		mem_free(args, sizeof(FSINFOargs));
		return NULL;
	}

	return args;
}


void 
free_FSINFOargs(void *msg)
{
	FSINFOargs *args = NULL;
	if(msg == NULL)
		return;

	args = (FSINFOargs *)msg;
	mem_free(args->fsroot.data.data_val, args->fsroot.data.data_len);
	mem_free(args, sizeof(FSINFOargs));
	return;
}



PATHCONF3res * 
xdr_to_PATHCONF3res(char *msg, int len)
{
	PATHCONF3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (PATHCONF3res *)mem_alloc(sizeof(PATHCONF3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	if(!xdr_PATHCONF3res(&xdr, res)) {
		mem_free(res, sizeof(PATHCONF3res));
		return NULL;
	}

	return res;
}


void 
free_PATHCONF3res(void *msg)
{
	PATHCONF3res *res = NULL;
	if(msg == NULL)
		return;

	res = (PATHCONF3res *)msg;
	mem_free(res, sizeof(PATHCONF3res));

	return;
}



PATHCONF3args *
xdr_to_PATHCONF3args(char *msg, int len)
{
	PATHCONF3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (PATHCONF3args *)mem_alloc(sizeof(PATHCONF3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	args->object.data.data_val = NULL;

	if(!xdr_PATHCONF3args(&xdr, args)) {
		mem_free(args, sizeof(PATHCONF3args));
		return NULL;
	}

	return args;
}


void
free_PATHCONF3args(void *msg)
{
	PATHCONF3args *args = NULL;
	if(msg == NULL)
		return;

	args = (PATHCONF3args *)msg;
	mem_free(args->object.data.data_val, args->object.data.data_len);
	mem_free(args, sizeof(PATHCONF3args));

	return;
}

COMMIT3res 
* xdr_to_COMMIT3res(char *msg, int len)
{
	COMMIT3res *res = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	res = (COMMIT3res *)mem_alloc(sizeof(COMMIT3res));
	if(res == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);
	if(!xdr_COMMIT3res(&xdr, res)) {
		mem_free(res, sizeof(COMMIT3res));
		return NULL;
	}

	return res;
}


void
free_COMMIT3res(void *msg)
{
	COMMIT3res *res = NULL;
	if(msg == NULL)
		return;

	res = (COMMIT3res *)msg;
	mem_free(res, sizeof(COMMIT3res));

	return;
}


COMMIT3args * 
xdr_to_COMMIT3args(char *msg, int len)
{
	COMMIT3args *args = NULL;
	XDR xdr;

	if(msg == NULL)
		return NULL;

	args = (COMMIT3args *)mem_alloc(sizeof(COMMIT3args));
	if(args == NULL)
		return NULL;
	
	xdrmem_create(&xdr, msg, len, XDR_DECODE);

	args->file.data.data_val = NULL;
	if(!xdr_COMMIT3args(&xdr, args)) {
		mem_free(args, sizeof(COMMIT3args));
		return NULL;
	}

	return args;
}


void 
free_COMMIT3args(void *msg)
{
	COMMIT3args *args = NULL;
	if(msg == NULL)
		return;

	args = (COMMIT3args *)msg;
	mem_free(args->file.data.data_val, args->file.data.data_len);
	mem_free(args, sizeof(COMMIT3args));

	return;
}



