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




#ifndef _TICKRATE_H_
#define _TICKRATE_H_

#include <sys/time.h>
#include <time.h>
#include <sys/types.h>


/* Returns the number of ticks per usec. */
extern double calibrate_tickrate(int runs);
extern u_int64_t tick_count();


#define ticks_to_usecs(ticks, tickrate) ((u_int64_t)(ticks / tickrate))

#define ticks_to_usecs_typed(ticks, tickrate, type) ((type)(ticks / (type)tickrate))

#endif
