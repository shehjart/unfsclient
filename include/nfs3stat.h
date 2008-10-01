/*
 *    libnfsclient2, second incarnation of libnfsclient, a library for
 *    NFS operations from user space.
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available here:
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


#ifndef _NFS3STATS_H_
#define _NFS3STATS_H_


#include <inttypes.h>
#include <sys/time.h>
#include <stdio.h>
#include <nfsclient.h>

#define timeval_to_usecs(ts) (((u_int64_t)ts.tv_sec * 1000000) + ts.tv_usec)
#define usecs_to_sec(t) ((double)(t) / 1000000)
#define usecs_to_usec(t) ((t) % 1000000)
#define usecs_to_msec(t) ((double)(t) / 1000)

/* Stores the statistics for a particular trace.
 */
#define NFSSTAT_ROWS 22
#define NFSSTAT_COLS 4
#define NFSSTAT_COL_TOTAL 0
#define NFSSTAT_COL_SUCCESS 1
#define NFSSTAT_COL_FAILED 2
#define NFSSTAT_COL_RESTIME 3


#define NFSSTAT_TELEMETRY_ON 1
#define NFSSTAT_TELEMETRY_OFF 0
#define TELEMETRY_ON(ctx) ((ctx)->telemetry == NFSSTAT_TELEMETRY_ON)
#define NFSSTAT_TM_BUFSZ (10 * 1024 * 1024)

struct _nfsstat_ctx {

	u_int64_t packet_count;
	
	/* Stores count of requests for each message type and the number of
	 * successful and failed requests.
	 */
	u_int64_t proc_pkt_count[NFSSTAT_ROWS][NFSSTAT_COLS];
	
	struct timeval starttime;
	struct timeval endtime;

	/* This request count is only for the purpose of calculation
	 * response times, which will be calculated on the basis of
	 * the number of replies received only, not using the total
	 * packet count.
	 */
	u_int64_t restime_packet_count;
	u_int64_t restime_usecs;

	/* Determines telemetry output. */
	int telemetry;
	/* Stats and telemetry data goes in here */
	FILE * statsfile;
};

typedef struct _nfsstat_ctx nfsstat_ctx;

extern nfsstat_ctx * nfsstat_init(FILE * statsfile, int telemetry);
extern void nfsstat_dump(nfsstat_ctx * stats);
extern void nfsstat_update_packet_counter(nfsstat_ctx * stats, int procval);
extern void nfsstat_update_status_counter(nfsstat_ctx * stats, int procval, 
		int status);

extern void nfsstat_update_ops_per_sec(nfsstat_ctx * stats,
		struct timeval frametime);
extern void nfsstat_update_response_times(nfsstat_ctx * stats, int procval, 
		struct timeval call, struct timeval reply);

extern void nfsstat_dump_ops_per_sec(nfsstat_ctx * stats);
extern void nfsstat_dump_response_time(nfsstat_ctx * stats);
#endif
