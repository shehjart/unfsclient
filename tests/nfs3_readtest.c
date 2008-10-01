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
#include <nfsclient.h>
#include <tickrate.h>
#include <assert.h>

struct client_state;
#define timeval_to_usecs(ts) (((u_int64_t)ts.tv_sec * 1000000) + ts.tv_usec)
#define usecs_to_secs(t) ((double)(((u_int64_t)(t)) / (double)1000000))

struct rtdata {
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
	struct rtdata * rtdata;
	int rtidx;
	double tickrate;
	struct timeval starttime;
	struct timeval endtime;

	/* Bytes read counter for async reader. */
	u_int64_t bytes_read;
};


struct run_params {
	u_int64_t startoff;
	u_int32_t rsize;	/* NFS server defined read size */
	u_int64_t filesize;
	int runcount;	/* Number of runs */
	int slotcount;	/* Number of requests to send at once */
	char * filename;
	int requestcount;
};

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


static void
nfs_lookup_cb(void *msg, int len, void *priv)
{
	LOOKUP3res * res = NULL;
	nfs_fh3 *fhobj = NULL;
	fhandle3 * newfh = NULL;
	int fhlen;
	struct client_state *cs;

	cs = (struct client_state *)priv;
	res = xdr_to_LOOKUP3res(msg, len);

	cs->callstat = res->status;
	if(res->status != NFS3_OK) {
		return;
	}

	fhobj = &res->LOOKUP3res_u.resok.object;
	fhlen = fhobj->data.data_len;
	
	newfh = (fhandle3 *)malloc(sizeof(fhandle3));
	newfh->fhandle3_len = fhlen;
	newfh->fhandle3_val = (char *)malloc(sizeof(char) * fhlen);

	memcpy(newfh->fhandle3_val, fhobj->data.data_val, fhlen);

	cs->privdat = newfh;
	free_LOOKUP3res(res);
}


static fhandle3 *
lookup_file(struct client_state * cs, fhandle3 dirfh, char *name)
{
	LOOKUP3args lookupargs;
	enum clnt_stat stat;

	lookupargs.what.dir.data.data_len = dirfh.fhandle3_len;
	lookupargs.what.dir.data.data_val = dirfh.fhandle3_val;
	lookupargs.what.name = name;
	stat = nfs3_lookup(&lookupargs, cs->ctx, nfs_lookup_cb, cs);
	if(stat != RPC_SUCCESS)
		return NULL;
	
	nfs_complete(cs->ctx, RPC_BLOCKING_WAIT);
	return (fhandle3 *)cs->privdat;
}


static void
nfs_read_cb_sync(void * msg, int len, void * priv)
{
	READ3res * res = NULL;
	struct client_state * cs = NULL;
	struct rtdata * rt = (struct rtdata *)priv;
	assert(rt != NULL);
	cs = rt->cs;
	assert(cs != NULL);

	res = xdr_to_READ3res(msg, len, NFS3_DATA_NO_DEXDR);
	assert(res != NULL);
	cs->callstat = res->status;
	cs->privdat = NULL;
	rt->replyticks = tick_count();
	cs->privdat = res;
}


static u_int32_t
read_file_bytes_sync(struct client_state * cs, fhandle3 * fh, u_int64_t offset, 
		u_int32_t rsize)
{
	READ3args ra;
	READ3res * res;
	enum clnt_stat stat;
	u_int32_t countres;
	struct rtdata * rt = NULL;

	ra.file.data.data_len = fh->fhandle3_len;
	ra.file.data.data_val = fh->fhandle3_val;
	ra.count = rsize;
	ra.offset = offset;

	rt = &cs->rtdata[cs->rtidx];
	rt->call_idx = cs->rtidx++;
	rt->offset = offset;
	rt->cs = cs;
	rt->callticks = tick_count();
	stat = nfs3_read(&ra, cs->ctx, nfs_read_cb_sync, rt);
	if(stat != RPC_SUCCESS)
		return -1;

	nfs_complete(cs->ctx, RPC_BLOCKING_WAIT);
	res = cs->privdat;

	countres = res->READ3res_u.resok.count;
	free_READ3res(res, NFS3_DATA_NO_DEXDR);
	
	return countres;

}



static u_int64_t
sync_read_file(struct client_state * cs, fhandle3 * fh, u_int64_t startoff,
		u_int64_t readlen, u_int32_t rsize)
{
	u_int64_t offset;
	u_int32_t count;
	u_int64_t bytes_read = 0;

	for(offset = startoff; offset < startoff + readlen; offset += rsize) {

		count = read_file_bytes_sync(cs, fh, offset, rsize);
		if(count <= 0)
			break;
		else
			bytes_read += count;
	}

	return bytes_read;
}


static void
nfs_read_cb_async(void * msg, int len, void * priv)
{
	READ3res * res = NULL;
	struct client_state * cs = NULL;
	struct rtdata * rt = (struct rtdata *)priv;
	assert(rt != NULL);
	cs = rt->cs;
	assert(cs != NULL);

	res = xdr_to_READ3res(msg, len, NFS3_DATA_NO_DEXDR);
	assert(res != NULL);
	cs->callstat = res->status;
	cs->privdat = NULL;
	rt->replyticks = tick_count();
	if(res->status != NFS3_OK)
		return;

	cs->bytes_read += res->READ3res_u.resok.count;
	free_READ3res(res, NFS3_DATA_NO_DEXDR);
}



static int 
read_file_bytes_async(struct client_state * cs, fhandle3 * fh, u_int64_t offset, 
		u_int32_t rsize)
{
	READ3args ra;
	enum clnt_stat stat;
	struct rtdata * rt = NULL;

	ra.file.data.data_len = fh->fhandle3_len;
	ra.file.data.data_val = fh->fhandle3_val;
	ra.count = rsize;
	ra.offset = offset;

	rt = &cs->rtdata[cs->rtidx];
	rt->call_idx = cs->rtidx++;
	rt->offset = offset;
	rt->cs = cs;
	rt->callticks = tick_count();
	stat = nfs3_read(&ra, cs->ctx, nfs_read_cb_async, rt);
	if(stat != RPC_SUCCESS)
		return -1;

	return 0;
}



static u_int64_t
async_read_file(struct client_state * cs, fhandle3 * fh, u_int64_t startoff, 
		u_int64_t readlen, u_int32_t rsize, int slotcount)
{
	u_int64_t offset;
	int slotsent, replies_recvd;
	int stat;

	slotsent = replies_recvd = 0;

	for(offset = startoff; offset < startoff + readlen; offset += rsize) {
retry_slot:	
		if(slotsent < slotcount) {
			stat = read_file_bytes_async(cs, fh, offset, rsize);
			++slotsent;
			replies_recvd = nfs_complete(cs->ctx, RPC_NONBLOCK_WAIT);
			slotsent -= replies_recvd;
		}
		else {
			replies_recvd = nfs_complete(cs->ctx, RPC_BLOCKING_WAIT);
			slotsent -= replies_recvd;
			goto retry_slot;
		}
			
		if(stat < 0)
			break;
	}

	while(slotsent) {
		replies_recvd = nfs_complete(cs->ctx, RPC_BLOCKING_WAIT);
		slotsent -= replies_recvd;
	}

	return cs->bytes_read;
}


	
static void
process_rtdata(struct client_state * cs, struct run_params rp)
{

	int i;
	struct rtdata * rt = NULL;
	u_int64_t startusecs, endusecs, totalusecs = 0;
	double rt_usecs;

	for(i = 0; i < rp.requestcount; i++) {
		rt = &cs->rtdata[i];
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
			(double)(totalusecs / (double)rp.requestcount));

}



static void
read_test(struct client_state * cs, struct run_params rp)
{
	fhandle3 * testfile = NULL;
	u_int64_t bytes_read;
	u_int64_t startusecs, endusecs;
	double duration, tx_tput, rx_tput;
	double data_rx_tput;

	if((testfile = lookup_file(cs, cs->mntfh, rp.filename)) == NULL) {
		fprintf(stderr, "Could not lookup file: %s: %s\n",
				rp.filename, nfsstat3_strerror(cs->callstat));
		return;
	}

	if(rp.slotcount == 1) {
		gettimeofday(&cs->starttime, NULL);
		bytes_read = sync_read_file(cs, testfile, rp.startoff, rp.filesize,
				rp.rsize);
		gettimeofday(&cs->endtime, NULL);
	}
	else {
		gettimeofday(&cs->starttime, NULL);
		bytes_read = async_read_file(cs, testfile, rp.startoff,
				rp.filesize, rp.rsize,rp.slotcount);
		gettimeofday(&cs->endtime, NULL);
	}
			
	fprintf(stdout, "#Bytes read: %"PRIu64"\n", bytes_read);
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
	
	data_rx_tput = (double)((bytes_read / duration) / (1024 * 1024));
	fprintf(stdout, "#Data Read Throughput: %.2lf MiB/sec\n", data_rx_tput);
	process_rtdata(cs, rp);
}



struct client_state *
client_init(char * srv, char * dir, struct run_params rp)
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	struct client_state * cstate = NULL;
	int rtdata_size;
	
	if((srv == NULL) || (dir == NULL))
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
	cstate->ctx->nfs_rsize = rp.rsize + 1024;
	cstate->ctx->nfs_wsize = rp.rsize + 1024;

	if(rp.filesize % rp.rsize)
		rp.requestcount = rp.filesize / rp.rsize + 1;
	else
		rp.requestcount = rp.filesize / rp.rsize;
	rtdata_size = sizeof(struct rtdata) * (rp.requestcount + 100);

	cstate->rtdata = (struct rtdata *)malloc(rtdata_size);
	if(cstate->rtdata == NULL) {
		fprintf(stderr, "Could not allocate memory for rtdata\n");
		return NULL;
	}
	
	memset(cstate->rtdata, 0, rtdata_size);
	cstate->rtidx = 0;
	cstate->bytes_read = 0;
	cstate->tickrate = calibrate_tickrate(10);
	stat = mount3_mnt(&dir, cstate->ctx, nfs_mnt_cb, (void *)cstate);
	if(stat != RPC_SUCCESS) {
		fprintf(stderr, "Could not send NFS MOUNT call\n");
		return NULL;
	}

	return cstate;
}



void init_runparams(struct run_params *rp)
{
	
	//Default values.
	rp->startoff = 0;
	rp->rsize = 0;
	rp->filesize = 0;
	rp->runcount = 1;
	rp->slotcount = 1;
	rp->filename = NULL;
	rp->requestcount = 0;
};



void
usage()
{
	fprintf(stdout, "USAGE: \n");
	fprintf(stdout, "\t\t--server <server>\n");
	fprintf(stdout, "\t\t--dir <remote_dir>\n");
	fprintf(stdout, "\t\t--filesize <filesize>\n");
	fprintf(stdout, "\t\t--rsize <nfs_rsize>\n");
	fprintf(stdout, "\t\t--startoff <offset>\n"
			"\t\t\tNOTE: Default starting offset is 0.\n");
	fprintf(stdout, "\t\t--filename <filename>\n");
	fprintf(stdout, "\t\t--runcount <count>\n"
			"\t\t\tNOTE: Default runcount is 1.\n");
	fprintf(stdout, "\t\t--slots <slotcount>\n"
			"\t\t\tNOTE: Default slotcount is 1.\n");
	
}
	

int 
main(int argc, char *argv[])
{
	struct client_state * cstate = NULL;
	char *srv, *fname, *remote_dir;
	int i;
	struct run_params rp;
	char *endptr = NULL;

	srv = fname = remote_dir = NULL;	
	if(argc < 11) {
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
	
		if((strcmp(argv[i], "--rsize")) == 0) {
			rp.rsize = atoi(argv[++i]);
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

	fprintf(stderr, "filename %s, server %s, remote dir %s, filesize %"PRIu64
			", rsize %d, offset %"PRIu64", runs %d, slots %d\n",
			rp.filename, srv, remote_dir, rp.filesize, rp.rsize,
			rp.startoff, rp.runcount, rp.slotcount);
	
	cstate = client_init(srv, remote_dir, rp);
	if(cstate == NULL) {
		fprintf(stderr, "Could not init client state\n");
		return -1;
	}

	read_test(cstate, rp);
	return 0;
}
