/*
 * 
 * Tool for measuring the minimal latency of a NFS server
 * by using the NFSv3 NULL request.
 *
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
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
#include <sys/time.h>
#include <sys/types.h>
#include <inttypes.h>
#include <nfsclient.h>
#include <stdlib.h>
#include <tickrate.h>


#define TICKRATE_CALIB_RUNS 10


struct test_state {
	struct addrinfo *ts_srv;
	int ts_totalcalls;
	nfs_ctx *ts_ctx;
	double ts_tickrate;

};

struct rtdata {
	int req_id;
	int64_t callticks;
	int64_t replyticks;
};

void 
nfs_null_cb(void *m, int len, void *p)
{
#ifndef __NO_MEASUREMENTS__
	struct rtdata * rt = (struct rtdata *)p;
	rt->replyticks = tick_count();
#endif
}

void
usage()
{
	fprintf(stderr, "Not enough arguments\nThis tests the "
			"asynchronous libnfsclient NFSv3 NULL call"
			"interface and callback.\n"
			"USAGE: nulltest <OPTIONS>\n");

	fprintf(stderr, "\n\tOPTIONS\n");
	fprintf(stderr, "\t\t--server <server>\n");
	fprintf(stderr, "\t\t--requestcount <count>\n");
	//fprintf(stderr, "\t\t--minthreads <count>\n");
	//fprintf(stderr, "\t\t\--rate <rate>\n")
	
}

void
process_rtdata(struct test_state ts, struct rtdata * rttable)
{
	int i;
	struct rtdata * rt;
	double call_usecs, reply_usecs, totalrestime, trate = ts.ts_tickrate;
	call_usecs = reply_usecs = totalrestime = 0.0;

	for(i = 1; i < ts.ts_totalcalls; i++) {
		rt = &rttable[i];
		call_usecs = ticks_to_usecs_typed(rt->callticks, trate, double);
		reply_usecs = ticks_to_usecs_typed(rt->replyticks, trate, double);
		fprintf(stdout, "%d %lf\n", rt->req_id, (reply_usecs - call_usecs));
		totalrestime += (reply_usecs - call_usecs);
	}

	fprintf(stdout, "AvgRT: %lf usecs\n",
			(double)(totalrestime/(double)ts.ts_totalcalls));
}

void
runtest(struct test_state ts)
{
	int i;
	struct rtdata * rt = NULL;
#ifndef __NO_MEASUREMENTS__
	struct rtdata * rttable;

	rttable = (struct rtdata *)malloc(sizeof(struct rtdata) * ts.ts_totalcalls);
	memset(rttable, 0, sizeof(struct rtdata) * ts.ts_totalcalls);
#endif

	for(i = 0; i < ts.ts_totalcalls; i++) {
#ifndef __NO_MEASUREMENTS__
		rt = &rttable[i];
		rt->req_id = i;
		rt->callticks = tick_count();
#endif
		nfs3_null(ts.ts_ctx, nfs_null_cb, rt, RPC_BLOCKING_WAIT);
	}

#ifndef __NO_MEASUREMENTS__
	process_rtdata(ts, rttable);
#endif

}


int 
main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	char *srv = NULL;
	int totalcalls, i, err;
	struct test_state ts;
	
	totalcalls = 0;
	if(argc < 5) {
		usage();
		return -1;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	for(i = 0; i < argc; ++i) {
		if((strcmp(argv[i], "--server")) == 0) {
			srv = argv[++i];
			continue;
		}

		if((strcmp(argv[i], "--requestcount")) == 0) {
			++i;
			totalcalls = atoi(argv[i]);
			continue;
		}

	}

	if(srv == NULL) {
		fprintf(stderr, "Server not specified.\n");
		usage();
		return -1;
	}

	if(totalcalls == 0) {
		fprintf(stderr, "Request count not specified.\n");
		usage();
		return -1;
	}

	fprintf(stdout, "server: %s, requestcount: %d\n", srv, totalcalls);

	if((err = getaddrinfo(srv, NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "nulltest: Cannot resolve name: %s: %s\n",
				srv, gai_strerror(err));
		return -1;
	}

#ifndef __NO_MEASUREMENTS__
	fprintf(stdout, "Calibrating tick rate..\n");
	ts.ts_tickrate = calibrate_tickrate(TICKRATE_CALIB_RUNS);
	fprintf(stdout, "Tick rate: %lf ticks per usec\n", ts.ts_tickrate);
#endif

	ts.ts_totalcalls = totalcalls;
	ts.ts_srv = srv_addr;
	ts.ts_ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr, IPPROTO_TCP,
			NFSC_CFL_NONBLOCKING);

	if(ts.ts_ctx == NULL) {
		fprintf(stderr, "Cannot init nfs context\n");
		return -1;
	}

	runtest(ts);
	return 0;
}
