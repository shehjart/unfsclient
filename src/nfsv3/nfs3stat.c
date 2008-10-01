/*
 *    libnfsclient2, second incarnation of libnfsclient, a library for
 *    NFS operations from user space.
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info at:
 *    http://nfsreplay.sourceforge.net
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
#include <inttypes.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <nfs3stat.h>

void
nfsstat_dump_response_time(nfsstat_ctx * stats)
{
	if(stats == NULL) {
		fprintf(stderr, "No stats\n");
		return;
	}

	/* Average in ms */
	float avg_restime_msecs = ((float) stats->restime_usecs/1000)
		/ (float)stats->restime_packet_count;

	fprintf(stats->statsfile, "Average Response Time (msec): %5.2f",
			avg_restime_msecs);
}


void 
nfsstat_dump_ops_per_sec(nfsstat_ctx * stats)
{
	u_int64_t opsec;
	u_int64_t diffusecs;
	time_t etime;
	float avg_restime_msecs;

	if(stats == NULL) {
		fprintf(stderr, "No stats\n");
		return;
	}
	diffusecs = (timeval_to_usecs(stats->endtime) -
			timeval_to_usecs(stats->starttime));
	opsec = stats->packet_count / usecs_to_sec(diffusecs);
	etime = usecs_to_sec(timeval_to_usecs(stats->starttime));
	fprintf(stats->statsfile, "Start time: %s", ctime(&etime));
	etime = usecs_to_sec(timeval_to_usecs(stats->endtime));
	fprintf(stats->statsfile, "End time: %s", ctime(&etime));
	fprintf(stats->statsfile, "Client Operations Per Second: %"PRIu64, opsec);

	/* Now find the theoretical ops per sec, by using the avg
	 * response time from the server.
	 */
	avg_restime_msecs = ((float) stats->restime_usecs/1000)
		/ (float)stats->restime_packet_count;

	fprintf(stats->statsfile, "\nServer Operations Per Second: %"PRIu64, 
			(u_int64_t)(1000 / avg_restime_msecs));
}


void
nfsstat_update_packet_counter(nfsstat_ctx * stats, int procval)
{
	if(stats == NULL)
		return;
	++stats->packet_count;
	++stats->proc_pkt_count[procval][0];
}

void 
nfsstat_update_status_counter(nfsstat_ctx * stats, int procval, int status)
{
	if(stats == NULL)
		return;

	if(status == NFS3_OK)
		++stats->proc_pkt_count[procval][1];
	else
		++stats->proc_pkt_count[procval][2];
}

static void
dump_packet_counter(nfsstat_ctx * stats)
{
	int i;
	u_int64_t success, failed, usecs, count;
	success = failed = 0;
	if(stats == NULL) {
		fprintf(stderr, "No stats\n");
		return;
	}
	
	fprintf(stats->statsfile, "Packet Count: %"PRIu64"\n", stats->packet_count);
	fprintf(stats->statsfile, "Request Count Success Failed AvgResponseTime(msecs)");
	for(i = 0; i < 22; ++i) {
		success += stats->proc_pkt_count[i][NFSSTAT_COL_SUCCESS];
		failed += stats->proc_pkt_count[i][NFSSTAT_COL_FAILED];
		usecs = stats->proc_pkt_count[i][NFSSTAT_COL_RESTIME];
		count = stats->proc_pkt_count[i][NFSSTAT_COL_TOTAL];

		fprintf(stats->statsfile, "\n%s %"PRIu64" %"PRIu64" %"PRIu64,
				nfsv3_proc_vals[i], count,
				stats->proc_pkt_count[i][NFSSTAT_COL_SUCCESS],
				stats->proc_pkt_count[i][NFSSTAT_COL_FAILED]);

		/* Prevent a divide by zero problem */
		if(count != 0)
			fprintf(stats->statsfile, " %f",
					usecs_to_msec(usecs) / count);
		else
			fprintf(stats->statsfile, " 0");

	}

	fprintf(stats->statsfile, "\nSuccessful: %f",
			(success/(float)stats->packet_count)*100);
	fprintf(stats->statsfile, "\nFailed: %f",
			(failed/(float)stats->packet_count)*100);
}


void 
nfsstat_dump(nfsstat_ctx * stats)
{
	if(stats == NULL) {
		fprintf(stderr, "No stats\n");
		return;
	}

	/* Use this line as a marker for end of traffic stats. */
	fprintf(stats->statsfile, "==== Traffic Stats ====\n");
	dump_packet_counter(stats);
	fprintf(stats->statsfile, "\n");
	nfsstat_dump_ops_per_sec(stats);
	fprintf(stats->statsfile, "\n");
	nfsstat_dump_response_time(stats);
	fprintf(stats->statsfile, "\n");
}


void 
nfsstat_update_ops_per_sec(nfsstat_ctx * stats, struct timeval frametime)
{
	if(stats == NULL)
		return;
	if((stats->starttime.tv_sec == 0) && (stats->starttime.tv_usec == 0)) {
		stats->starttime = frametime;
		return;
	}
	
	stats->endtime = frametime;

	return;
}

void
nfsstat_update_response_times(nfsstat_ctx * stats, int procval, struct timeval call
		, struct timeval reply)
{
	u_int64_t ct, rt;

	if(stats == NULL)
		return;
	ct = timeval_to_usecs(call);
	rt = timeval_to_usecs(reply);

	stats->restime_usecs += (rt - ct);
	++stats->restime_packet_count;
	stats->proc_pkt_count[procval][NFSSTAT_COL_RESTIME] += (rt - ct);
	if(TELEMETRY_ON(stats))
		fprintf(stats->statsfile, "rt: %d %f %f\n", procval,
				usecs_to_msec(ct), usecs_to_msec(rt - ct));
}


nfsstat_ctx *
nfsstat_init(FILE * statsfile, int tmetry)
{
	nfsstat_ctx * stats = NULL;
	char * tm_buffer = NULL;

	stats = (nfsstat_ctx *)malloc(sizeof(nfsstat_ctx));
	if(stats == NULL)
		return NULL;

	stats->packet_count = 0;
	memset(&stats->proc_pkt_count[0][0], 0, 
			NFSSTAT_ROWS * (sizeof(u_int64_t) * NFSSTAT_COLS));
	
	stats->starttime = (struct timeval){0,0};
	stats->endtime = (struct timeval){0,0};
	stats->restime_usecs = 0;
	stats->restime_packet_count = 0;

	stats->telemetry = tmetry;
	stats->statsfile = statsfile;

	/* If telemetry is on then fully buffer the output file
	 * because telemetry writing overhead is just too much 
	 * so we want to buffer it all.
	 */
	if(TELEMETRY_ON(stats)) {
		tm_buffer = malloc(NFSSTAT_TM_BUFSZ);
		setvbuf(stats->statsfile, tm_buffer, _IOFBF, NFSSTAT_TM_BUFSZ);
	}

	return stats;
}

