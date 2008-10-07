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
#include <pthread.h>
#include <semaphore.h>

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

	/* Semaphore to notify threads of meta data
	 * request availability.
	 */
	sem_t md_notify;

	/* Metadata request queue */
	pthread_mutex_t md_lock;
	struct flist_head md_rq;

	/* Semaphore to notify threads of data request
	 * availability.
	 */
	sem_t rw_notify;

	/* Read write request queue */
	pthread_mutex_t rw_lock;
	struct flist_head rw_rq;

	/* Protocol private state. */
};

extern struct nfsclientd_context * nfscd_ctx;

/* nfsclientd's FUSE operations. */
extern void nfscd_init(void *userdata, struct fuse_conn_info *conn);
extern void nfscd_destroy(void *userdata);
extern void nfscd_lookup(fuse_req_t req, fuse_ino_t parent, const char * name);

struct nfscd_fuse_lookup_args {
	fuse_ino_t parent;
	char * name;
};
typedef struct nfscd_fuse_lookup_args nflookup_args;


struct nfscd_fuse_forget_args {
	fuse_ino_t ino;
	unsigned long nlookup;
};
typedef struct nfscd_fuse_forget_args nfforget_args;

struct nfscd_fuse_getattr_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_getattr_args nfgetattr_args;

struct nfscd_fuse_setattr_args {
	fuse_ino_t ino;
	struct stat * attr;
	int to_set;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_setattr_args nfsetattr_args;

struct nfscd_fuse_readlink_args {
	fuse_ino_t ino;
};
typedef struct nfscd_fuse_readlink_args nfreadlink_args;

struct nfscd_fuse_mknod_args {
	fuse_ino_t parent;
	char * name;
	mode_t mode;
	dev_t rdev;
};
typedef struct nfscd_fuse_mknod_args nfmknod_args;

struct nfscd_fuse_mkdir_args {
	fuse_ino_t parent;
	char * name;
	mode_t mode;
};
typedef struct nfscd_fuse_mknod_args nfmkdir_args;

struct nfscd_fuse_unlink_args {
	fuse_ino_t parent;
	char * name;
};
typedef struct nfscd_fuse_unlink_args nfunlink_args;

struct nfscd_fuse_rmdir_args {
	fuse_ino_t parent;
	char * name;
};
typedef struct nfscd_fuse_rmdir_args nfrmdir_args;

struct nfscd_fuse_symlink_args {
	char * link;
	fuse_ino_t parent;
	char * name;
};
typedef struct nfscd_fuse_symlink_args nfsymlink_args;

struct nfscd_fuse_rename_args {
	fuse_ino_t parent;
	char * name;
	fuse_ino_t newparent;
	char * newname;
};
typedef struct nfscd_fuse_rename_args nfrename_args;

struct nfscd_fuse_link_args {
	fuse_ino_t ino;
	fuse_ino_t newparent;
	char * newname;
};
typedef struct nfscd_fuse_link_args nflink_args;

struct nfscd_fuse_open_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_open_args nfopen_args;

struct nfscd_fuse_read_args {
	fuse_ino_t ino;
	size_t size;
	off_t off;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_read_args nfread_args;

struct nfscd_fuse_write_args {
	fuse_ino_t ino;
	char * buf;
	size_t size;
	off_t off;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_write_args nfwrite_args;


struct nfscd_fuse_flush_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_flush_args nfflush_args;

struct nfscd_fuse_release_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_release_args nfrelease_args;

struct nfscd_fuse_fsync_args {
	fuse_ino_t ino;
	int datasync;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_fsync_args nffsync_args;

struct nfscd_fuse_opendir_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_opendir_args nfopendir_args;

struct nfscd_fuse_readdir_args {
	fuse_ino_t ino;
	size_t size;
	off_t off;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_readdir_args nfreaddir_args;

struct nfscd_fuse_releasedir_args {
	fuse_ino_t ino;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_releasedir_args nfreleasedir_args;

struct nfscd_fuse_fsyncdir_args {
	fuse_ino_t ino;
	int datasync;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_fsyncdir_args nffsyncdir_args;

struct nfscd_fuse_statfs_args {
	fuse_ino_t ino;
};
typedef struct nfscd_fuse_statfs_args nfstatfs_args;

struct nfscd_fuse_access_args {
	fuse_ino_t ino;
	int mask;
};
typedef struct nfscd_fuse_access_args nfaccess_args;

struct nfscd_fuse_create_args {
	fuse_ino_t parent;
	char * name;
	mode_t mode;
	struct fuse_file_info * fi;
};
typedef struct nfscd_fuse_create_args nfcreate_args;

struct nfscd_request {
	struct flist_head list;
	fuse_req_t fuserq;

	union {
		nflookup_args lookupargs;
		nfforget_args forgetargs;
		nfgetattr_args getattrargs;
		nfsetattr_args setattrargs;
		nfreadlink_args readlinkargs;
		nfmknod_args mknodargs;
		nfmkdir_args mkdirargs;
		nfunlink_args unlinkargs;
		nfrmdir_args rmdirargs;
		nfsymlink_args symlinkargs;
		nfrename_args renameargs;
		nfopen_args openargs;
		nfread_args readargs;
		nfwrite_args writeargs;
		nfflush_args flushargs;
		nfrelease_args releaseargs;
		nfopendir_args opendirargs;
		nfreaddir_args readdirargs;
		nfreleasedir_args releasedirargs;
		nffsyncdir_args fsyncdir_args;
		nfstatfs_args statfsargs;
		nfaccess_args accessargs;
		nfcreate_args createargs;
	} args_u;
};


extern struct nfscd_request * nfscd_next_request(struct nfsclientd_context * ctx);

