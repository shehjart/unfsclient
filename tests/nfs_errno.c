/*
 *    Test code for libnfsclient, library for NFS operations from user space.
 *    Copyright (C) 2007 Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
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


/* This program prints out the local error messages for each of the
 * nfs error numbers.
 */

#include <nfsclient.h>
#include <errno.h>
#include <string.h>



int errors[] = {
	NFS3_OK,
	NFS3ERR_PERM,
	NFS3ERR_NOENT, 
	NFS3ERR_IO, 
	NFS3ERR_NXIO, 
	NFS3ERR_ACCES, 
	NFS3ERR_EXIST,
	NFS3ERR_XDEV ,
	NFS3ERR_NODEV, 
	NFS3ERR_NOTDIR, 
	NFS3ERR_ISDIR ,
	NFS3ERR_INVAL ,
	NFS3ERR_FBIG ,
	NFS3ERR_NOSPC ,
	NFS3ERR_ROFS ,
	NFS3ERR_MLINK ,
	NFS3ERR_NAMETOOLONG ,
	NFS3ERR_NOTEMPTY, 
	NFS3ERR_DQUOT ,
	NFS3ERR_STALE ,
	NFS3ERR_REMOTE ,
	NFS3ERR_BADHANDLE ,
	NFS3ERR_NOT_SYNC ,
	NFS3ERR_BAD_COOKIE, 
	NFS3ERR_NOTSUPP ,
	NFS3ERR_TOOSMALL ,
	NFS3ERR_SERVERFAULT ,
	NFS3ERR_BADTYPE ,
	NFS3ERR_JUKEBOX,
	-1
};


int main() {

	int i;
	char * str = NULL;

	fprintf(stdout, "NFS Err Num\tStatus String\n");

	for(i = 0; errors[i] != -1; i++) {
		str = nfsstat3_strerror(errors[i]);
		fprintf(stdout, "%d\t\t%s\n", errors[i], str);
	}

	return 0;
}

