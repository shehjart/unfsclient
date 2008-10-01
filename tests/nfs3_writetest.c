/*
 *    Test code for nfsclient library.
 *    Copyright (C) 2007 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available at:
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


#include <rpc/rpc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <inttypes.h>
#include <sys/time.h>
#include <assert.h>

#include <nfsclient.h>
#include <tickrate.h>


#define timeval_to_usecs(ts) (((u_int64_t)ts.tv_sec * 1000000) + ts.tv_usec)
#define usecs_to_secs(t) ((double)(((u_int64_t)(t)) / (double)1000000))

#define WRITE_FILESYNC 2
#define WRITE_DATASYNC 1
#define WRITE_UNSTABLE 0

struct client_state;

struct rt_data {
	int call_idx;
	u_int64_t offset;
	u_int64_t callticks;
	u_int64_t replyticks;
	struct client_state * cs;
};

struct client_state {
	nfs_ctx *ctx;
	fhandle3 mntfh;
	int callstat;
	void *privdat;
	u_int64_t requestcount;
	struct rt_data * rt;
	u_int64_t rtidx;
	u_int64_t tickrate;
	struct timeval starttime;
	struct timeval endtime;
	char * wrbuffer;
	u_int64_t async_bytes_written;
};


struct run_params {
	u_int64_t startoff;
	u_int32_t wsize;
	u_int64_t filesize;
	int runcount;
	int slotcount;
	char * filename;
	int stable;
};




void
nfs_create_cb(void *msg, int len, void *priv_ctx)
{
	CREATE3res *cres = NULL;
	int fh_len;
	char *fh;
	struct client_state * ctx = NULL;
	fhandle3 *cfh = NULL;

	cres = xdr_to_CREATE3res(msg, len);
	assert(cres != NULL);

	ctx = (struct client_state *)priv_ctx;
	ctx->callstat = cres->status;
	ctx->privdat = NULL;
	if(cres->status != NFS3_OK) {
		free_CREATE3res(cres);
		return;
	}
	
	fh_len = cres->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_len;
	fh = cres->CREATE3res_u.resok.obj.post_op_fh3_u.handle.data.data_val;
	cfh = (fhandle3 *)malloc(sizeof(fhandle3));
	cfh->fhandle3_len = fh_len;
	cfh->fhandle3_val = (char *)mem_alloc(fh_len);
	assert(cfh->fhandle3_val != NULL);

	memcpy(cfh->fhandle3_val, fh, fh_len);
	free_CREATE3res(cres);
	ctx->privdat = cfh;
}

void 
nfs_mnt_cb(void *msg, int len, void *priv_ctx)
{
	mountres3 *mntres = NULL; 
	char *fh = NULL;
	u_long fh_len;
	struct client_state * ctx = NULL;

	ctx = (struct client_state *)priv_ctx;
	mntres = xdr_to_mntres3(msg, len);
	if(mntres == NULL)
		return;

	ctx->callstat = mntres->fhs_status;
	if(mntres->fhs_status != MNT3_OK) {
		fprintf(stderr, "Mount call failed..\n");
		free_mntres3(mntres);
		return;
	}
	
	fh_len = mntres->mountres3_u.mountinfo.fhandle.fhandle3_len;
	fh = mntres->mountres3_u.mountinfo.fhandle.fhandle3_val;
	ctx->mntfh.fhandle3_val = (char *)mem_alloc(fh_len);
	ctx->mntfh.fhandle3_len = fh_len;
	if(ctx->mntfh.fhandle3_val == NULL) {
		free_mntres3(mntres);
		return;
	}

	memcpy(ctx->mntfh.fhandle3_val, fh, fh_len);
	free_mntres3(mntres);
	return;
}


static fhandle3 * 
create_file(struct client_state *ctx, fhandle3 *dirfh, char *name)
{
	int stat;
	CREATE3args cr;
	if((ctx == NULL) || (name == NULL))
		return NULL;

	if(dirfh == NULL) {
		cr.where.dir.data.data_len = ctx->mntfh.fhandle3_len;
		cr.where.dir.data.data_val = ctx->mntfh.fhandle3_val;
	}
	else {
		cr.where.dir.data.data_len = dirfh->fhandle3_len;
		cr.where.dir.data.data_val = dirfh->fhandle3_val;
	}

	cr.where.name = name;
	cr.how.mode = UNCHECKED;
	memset(&cr.how.createhow3_u, 0, sizeof(cr.how.createhow3_u));
	cr.how.createhow3_u.obj_attributes.mode.set_it = 1;
	cr.how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = 32767;
	cr.how.createhow3_u.obj_attributes.uid.set_it = 1;
	cr.how.createhow3_u.obj_attributes.gid.set_it = 1;
	cr.how.createhow3_u.obj_attributes.size.set_it = 1;
	cr.how.createhow3_u.obj_attributes.atime.set_it = SET_TO_SERVER_TIME;
	cr.how.createhow3_u.obj_attributes.mtime.set_it = SET_TO_SERVER_TIME;

	stat = nfs3_create(&cr, ctx->ctx, nfs_create_cb, (void *)ctx);
	if(stat != RPC_SUCCESS) {
		return NULL;
	}

	nfs_complete(ctx->ctx, RPC_BLOCKING_WAIT);

	return (fhandle3 *)ctx->privdat;
}

	

void
nfs_remove_cb(void * msg, int len, void * priv)
{
	REMOVE3res * res = NULL;
	struct client_state * cs = NULL;

	res = xdr_to_REMOVE3res(msg, len);
	if(res == NULL)
		return;

	cs = (struct client_state *)priv;
	cs->callstat = res->status;
	if(res->status != NFS3_OK) {
		free_REMOVE3res(res);
		return;
	}
}


int
remove_file(struct client_state * cs, fhandle3 *dirfh, char * name)
{
	REMOVE3args re;
	int stat;

	if((cs == NULL) || (name == NULL))
		return -1;

	if(dirfh == NULL) {
		re.object.dir.data.data_len = cs->mntfh.fhandle3_len;
		re.object.dir.data.data_val = cs->mntfh.fhandle3_val;
	}
	else {
		re.object.dir.data.data_len = dirfh->fhandle3_len;
		re.object.dir.data.data_val = dirfh->fhandle3_val;
	}

	re.object.name = name;
	stat = nfs3_remove(&re, cs->ctx, nfs_remove_cb, cs);
	if(stat != RPC_SUCCESS) {
		fprintf(stderr, "Could not send NFS WRITE call\n");
		return -1;
	}

	nfs_complete(cs->ctx, RPC_BLOCKING_WAIT);
	return 0;
}


struct client_state *
client_init(char * srv, char * dir, struct run_params * rp)
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	struct client_state * cstate = NULL;
	int rtdata_size, requestcount;

	if((srv == NULL) || (dir == NULL) || (rp == NULL))
		return NULL;

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	if((err = getaddrinfo(srv, NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "Could resolve name: %s: %s\n", srv,
				gai_strerror(err));
		return NULL;
	}
	
	cstate = (struct client_state *)malloc(sizeof(struct client_state));
	if(cstate == NULL) {
		fprintf(stderr, "Could not allocate client state\n");
		return NULL;
	}

	cstate->ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr,
			IPPROTO_TCP, NFSC_CFL_NONBLOCKING);
	if(cstate->ctx == NULL) {
		fprintf(stderr, "Could init nfs context\n");
		return NULL;
	}

	freeaddrinfo(srv_addr);
	cstate->ctx->nfs_rsize = rp->wsize + 1024;
	cstate->ctx->nfs_wsize =  rp->wsize + 1024;
	cstate->requestcount = 0;
	stat = mount3_mnt(&dir, cstate->ctx, nfs_mnt_cb, (void *)cstate);
	if(stat != RPC_SUCCESS) {
		fprintf(stderr, "Could not send NFS MOUNT call\n");
		return NULL;
	}

	if(rp->filesize % rp->wsize)
		requestcount = rp->filesize / rp->wsize + 1;
	else
		requestcount = rp->filesize / rp->wsize;

	rtdata_size = sizeof(struct rt_data) * (requestcount + 100);
	cstate->rt = (struct rt_data *)malloc(rtdata_size);
	memset(cstate->rt, 0, rtdata_size);
	cstate->rtidx = 0;
	cstate->tickrate = calibrate_tickrate(10);
	cstate->async_bytes_written = 0;

	cstate->wrbuffer = (char *)malloc(rp->wsize * sizeof(char));
	assert(cstate->wrbuffer != NULL);
	memset(cstate->wrbuffer, 5, rp->wsize);
	return cstate;
}



void
usage()
{
	fprintf(stdout, "USAGE: \n");
	fprintf(stdout, "\t\t--server <server>\n");
	fprintf(stdout, "\t\t--dir <remote_dir>\n");
	fprintf(stdout, "\t\t--filesize <filesize>\n");
	fprintf(stdout, "\t\t--wsize <wsize>\n");
	fprintf(stdout, "\t\t--startoff <offset>\n"
			"\t\t\tNOTE: Default starting offset is 0.\n");
	fprintf(stdout, "\t\t--filename <filename>\n");
	fprintf(stdout, "\t\t--sync <file|data|unstable>\n");
	fprintf(stdout, "\t\t--runcount <count>\n"
			"\t\t\tNOTE: Default runcount is 1.\n");
	fprintf(stdout, "\t\t--slots <slotcount>\n"
			"\t\t\tNOTE: Default slotcount is 1.\n");
}

void 
nfs_sync_write_cb(void *msg, int len, void *priv_ctx)
{
	struct rt_data * rt = NULL;
	WRITE3res *res = NULL;
	struct client_state * cs;

	rt = (struct rt_data *)priv_ctx;
	res = xdr_to_WRITE3res(msg, len);
	assert(res != NULL);
	cs = rt->cs;
	cs->callstat = res->status;
	rt->replyticks = tick_count();
	
	cs->privdat = res;
	return;
}

	
static void 
nfs_async_write_cb(void *msg, int len, void *priv_ctx)
{
	struct rt_data * rt = NULL;
	WRITE3res *res = NULL;
	struct client_state * cs;

	rt = (struct rt_data *)priv_ctx;
	res = xdr_to_WRITE3res(msg, len);
	assert(res != NULL);
	cs = rt->cs;
	cs->callstat = res->status;
	rt->replyticks = tick_count();
	cs->async_bytes_written += res->WRITE3res_u.resok.count;
	free_WRITE3res(res);
	return;
}



static u_int32_t
sync_write_file_bytes(struct client_state * ctx, fhandle3 *fh, u_int64_t offset,
		u_int32_t wsize, int stable)
{
	WRITE3args wr;
	WRITE3res * res;
	int stat;
	struct rt_data * rt = NULL;
	u_int32_t countres;

	if((ctx == NULL) || (fh == NULL))
		return -1;

	wr.file.data.data_len = fh->fhandle3_len;
	wr.file.data.data_val = fh->fhandle3_val;
	wr.offset = offset;
	wr.count = wsize;
	wr.stable = stable;
	
	wr.data.data_len = wr.count;
	wr.data.data_val = ctx->wrbuffer;

	ctx->requestcount++;
	rt = &ctx->rt[ctx->rtidx++];
	rt->call_idx = ctx->requestcount;
	rt->offset = offset;
	rt->callticks = tick_count();
	rt->cs = ctx;
	stat = nfs3_write(&wr, ctx->ctx, nfs_sync_write_cb, (void *)rt);
	if(stat != RPC_SUCCESS) {
		return -1;
	}

	nfs_complete(ctx->ctx, RPC_BLOCKING_WAIT);
	res = ctx->privdat;
	countres = res->WRITE3res_u.resok.count;
	free_WRITE3res(res);

	return countres;
}


static int
async_write_file_bytes(struct client_state * ctx, fhandle3 *fh, u_int64_t offset,
		u_int32_t wsize, int stable)
{
	WRITE3args wr;
	int stat;
	struct rt_data * rt = NULL;

	if((ctx == NULL) || (fh == NULL))
		return -1;

	wr.file.data.data_len = fh->fhandle3_len;
	wr.file.data.data_val = fh->fhandle3_val;
	wr.offset = offset;
	wr.count = wsize;
	wr.stable = stable;
	
	wr.data.data_len = wr.count;
	wr.data.data_val = ctx->wrbuffer;

	ctx->requestcount++;
	rt = &ctx->rt[ctx->rtidx++];
	rt->call_idx = ctx->requestcount;
	rt->offset = offset;
	rt->callticks = tick_count();
	rt->cs = ctx;
	stat = nfs3_write(&wr, ctx->ctx, nfs_async_write_cb, (void *)rt);
	if(stat != RPC_SUCCESS) {
		return -1;
	}

	return 1;
}



static u_int64_t
async_write_file(struct client_state * cstate, fhandle3 * testfile, 
		u_int64_t startoff, u_int64_t filesize, u_int32_t wsize, 
		int stable, int slotcount)
{
	u_int64_t written, offset;
	int slotsent = 0;
	int replies_recvd = 0;
	int stat = 0;

	offset = startoff;
	for(written = 0; written < filesize; offset += wsize) {
retry_slot:
		if(slotsent < slotcount) {
			stat = async_write_file_bytes(cstate, testfile, offset,
					wsize, stable);
			++slotsent;
			replies_recvd = nfs_complete(cstate->ctx,
					RPC_NONBLOCK_WAIT);
			slotsent -= replies_recvd;
		}
		else {
			replies_recvd = nfs_complete(cstate->ctx,
					RPC_BLOCKING_WAIT);
			slotsent -= replies_recvd;
			goto retry_slot;
		}

		if(stat <=0)
			break;

		written += wsize;

	}

	while(slotsent > 0) {
		replies_recvd = nfs_complete(cstate->ctx, RPC_BLOCKING_WAIT);
		slotsent -= replies_recvd;
	}

	return cstate->async_bytes_written;
}



static u_int64_t
sync_write_file(struct client_state * cstate, fhandle3 * testfile, 
		u_int64_t startoff, u_int64_t filesize, u_int32_t wsize, int stable)
{
	u_int64_t written, offset, count;

	offset = startoff;
	count = 0;
	for(written = 0; written < filesize; offset += wsize) {
		
		count = sync_write_file_bytes(cstate, testfile, offset, wsize,
				stable);
		if(count <=0)
			break;
		else
			written += count;
	}

	return written;
}

	
static void
process_rtdata(struct client_state * cs, struct run_params rp)
{
	int i;
	struct rt_data * rt = NULL;
	u_int64_t startusecs, endusecs, totalusecs = 0;
	double rt_usecs;

	for(i = 0; i < cs->requestcount; i++) {
		rt = &cs->rt[i];
		if(rt == NULL)
			break;

		startusecs = ticks_to_usecs(rt->callticks, cs->tickrate);
		endusecs = ticks_to_usecs(rt->replyticks, cs->tickrate);
		rt_usecs = (endusecs - startusecs);
		totalusecs += rt_usecs;
	
		fprintf(stdout, "%d %"PRIu64" %10.3lf\n", rt->call_idx, rt->offset,
				rt_usecs);
	}

	fprintf(stdout, "#Avg. Response Time: %10.3lf\n", 
			(double)(totalusecs / (double)cs->requestcount));

}



static void
write_test(struct client_state * cs, struct run_params rp)
{
	char runfile[50];
	fhandle3 * testfile = NULL;
	u_int64_t bytes_written;
	u_int64_t startusecs, endusecs;
	double duration, tx_tput, rx_tput;
	double data_tx_tput;

	strcpy(runfile, rp.filename);
	if((testfile = create_file(cs, NULL, runfile)) == NULL) {
		fprintf(stderr, "Could not create file\n");
		return;
	}

	if(rp.slotcount == 1) {
		gettimeofday(&cs->starttime, NULL);
		bytes_written = sync_write_file(cs, testfile, rp.startoff, 
				rp.filesize, rp.wsize, rp.stable);
		gettimeofday(&cs->endtime, NULL);
	}
	else {
		gettimeofday(&cs->starttime, NULL);
		bytes_written = async_write_file(cs, testfile, rp.startoff, 
				rp.filesize, rp.wsize, rp.stable, rp.slotcount);
		gettimeofday(&cs->endtime, NULL);
	}

	strcpy(runfile, rp.filename);
	remove_file(cs, NULL, runfile);
	
	fprintf(stdout, "#Bytes Written: %"PRIu64"\n", bytes_written);
	startusecs = timeval_to_usecs(cs->starttime);
	endusecs = timeval_to_usecs(cs->endtime);
	duration = usecs_to_secs((endusecs - startusecs));
	fprintf(stdout, "#Run Duration: %10.2lf secs\n", duration);

	tx_tput = (double)((clnttcp_datatx(cs->ctx->nfs_cl)) / duration);
	fprintf(stdout, "#TX Throughput: %.2lf MiB/sec\n", 
			(double)(tx_tput / (1024*1024)));

	rx_tput = (double)((clnttcp_datarx(cs->ctx->nfs_cl)) / duration);
	fprintf(stdout, "#RX Throughput: %.2lf MiB/sec\n",
			(double)(rx_tput / (1024*1024)));
	
	data_tx_tput = (double)((bytes_written / duration) / (1024 * 1024));
	fprintf(stdout, "#Data Read Throughput: %.2lf MiB/sec\n", data_tx_tput);
	process_rtdata(cs, rp);

}

void init_runparams(struct run_params *rp)
{
	
	//Default values.
	rp->startoff = 0;
	rp->wsize = 0;
	rp->filesize = 0;
	rp->runcount = 1;
	rp->slotcount = 1;
	rp->filename = NULL;
	rp->stable = -1;
};




int 
main(int argc, char *argv[])
{
	struct client_state * cstate = NULL;
	char *srv, *fname, *remote_dir;
	int i;
	struct run_params rp;
	char * endptr = NULL;
	char * sync_str = NULL;

	srv = fname = remote_dir = NULL;	
	if(argc < 12) {
		fprintf(stderr, "Not enough arguments\n");
		usage();
		return -1;
	}

	init_runparams(&rp);
	for(i = 0; i < argc; i++) {
		if((strcmp(argv[i], "--runcount")) == 0) {
			rp.runcount = atoi(argv[++i]);
			continue;
		}

		if((strcmp(argv[i], "--server")) == 0) {
			srv = argv[++i];
			continue;
		}

		if((strcmp(argv[i], "--dir")) == 0) {
			remote_dir = argv[++i];
			continue;
		}

		if((strcmp(argv[i], "--filesize")) == 0) {
			rp.filesize = strtoll(argv[++i], &endptr, 10);
			continue;
		}
	
		if((strcmp(argv[i], "--wsize")) == 0) {
			rp.wsize = strtoll(argv[++i], &endptr, 10);
			continue;
		}

		if((strcmp(argv[i], "--filename")) == 0) {
			fname = argv[++i];
			continue;
		}

		if((strcmp(argv[i], "--startoff")) == 0) {
			rp.startoff = strtoll(argv[++i], &endptr, 10);
			continue;
		}

		if((strcmp(argv[i], "--slots")) == 0) {
			rp.slotcount = atoi(argv[++i]);
			continue;
		}
		
		if((strcmp(argv[i], "--sync")) == 0) {
			sync_str = argv[++i];
			continue;
		}
	}

	if(fname == NULL) {
		fprintf(stderr, "No filename given\n");
		usage();
		return -1;
	}
	else
		rp.filename = fname;

	if(srv == NULL) {
		fprintf(stderr, "No server name given.\n");
		usage();
		return -1;
	}

	if(remote_dir == NULL) {
		fprintf(stderr, "No remote directory name given.\n");
		usage();
		return -1;
	}

	if(sync_str == NULL) {
		fprintf(stderr, "No sync type given.\n");
		usage();
		return -1;
	}

	if(strcmp(sync_str, "file") == 0)
		rp.stable = WRITE_FILESYNC;
	else if(strcmp(sync_str, "data") == 0)
		rp.stable = WRITE_DATASYNC;
	else if(strcmp(sync_str, "unstable") == 0)
		rp.stable = WRITE_UNSTABLE;
	else {
		fprintf(stderr, "Unknown sync type.\n");
		usage();
		return -1;
	}

	fprintf(stderr, "filename %s, server %s, remote dir %s, filesize %"PRIu64
			", wsize %d, offset %"PRIu64", runs %d, slots %d"
			", stable %s\n",
			rp.filename, srv, remote_dir, rp.filesize, rp.wsize,
			rp.startoff, rp.runcount, rp.slotcount, sync_str);
	
	cstate = client_init(srv, remote_dir, &rp);
	if(cstate == NULL)
		return -1;

	write_test(cstate, rp);

	return 0;
}
