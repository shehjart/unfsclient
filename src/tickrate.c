/*
 * 
 * Code for measuring and calibrating the tick rate.
 * Based on the realfeel code by Andrew Morton.
 *
 * Modifications and clean-up by Shehjar Tikoo.
 *
 *    Copyright (C) 2008 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *    More info is available at:
 *
 *    http://www.gelato.unsw.edu.au/IA64wiki/ShehjarTikoo
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




#include <tickrate.h>
#include <unistd.h>
#include <stdlib.h>


static void
selectsleep(unsigned us) {
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = us;

        select(0,0,0,0,&tv);
}



static inline u_int64_t
timeofday_usecs(void)
{
	u_int64_t r;
	struct timeval t = { 0, 0 };

	gettimeofday(&t, NULL);

	r = (u_int64_t)t.tv_sec * 1000000;
	r += t.tv_usec;

	return r;
}

#if defined(__ia64__)
inline u_int64_t
tick_count(void)
{
	u_int64_t result;
	__asm__ __volatile__("mov %0=ar.itc;;" : "=r"(result) :: "memory");
	return result;
}

#elif defined(__i386__)
inline u_int64_t 
tick_count(void)
{
	u_int64_t result;
	__asm__ __volatile__("rdtsc" : "=A" (result));
	return result;
}

#elif defined(__amd64__)
inline u_int64_t 
tick_count(void)
{
	u_int64_t result;
	unsigned int a,d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	result = ((unsigned long)a) | (((unsigned long)d)<<32);
	return result;
}

#else
inline u_int64_t
tick_count(void)
{
	return timeofday_usecs();
}

#endif


double 
calibrate_tickrate(int runs)
{
	double sumx = 0;
	double sumy = 0;
	double sumxx = 0;
	double sumxy = 0;
	double slope;
	u_int64_t start;
	u_int64_t now;
	unsigned i;

	/* least squares linear regression of ticks onto real time
	 * as returned by gettimeofday.
	 */

  
	for (i = 0; i < runs; i++) {
		double real,ticks,sleeptime, ran;
    
		ran = drand48();
		sleeptime = (100000 + ran * 200000);

		now = timeofday_usecs();
		start = tick_count();

		selectsleep((unsigned int) sleeptime);

		ticks = tick_count() - start;
		real = timeofday_usecs() - now;

		sumx += real;
		sumxx += real * real;
		sumxy += real * ticks;
		sumy += ticks;
	}
	slope = ((sumxy - (sumx*sumy) / runs) /
		 (sumxx - (sumx*sumx) / runs));
	return slope;
}
