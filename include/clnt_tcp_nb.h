/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 *   New TCP handler for glibc RPC implementation that provides non-blocking
 *   RPC calls and asynchronous reply notifications.
 *   
 *   Modified from clnt_tcp.c in glibc/sunrpc
 *   Shehjar Tikoo, <shehjart@gelato.unsw.edu.au>
 *   More info can be found at the following websites:
 *   
 *   nfsreplay Home Page
 *   http://nfsreplay.sourceforge.net
 *
 *   AsyncRPC home page
 *   http://www.gelato.unsw.edu.au/IA64wiki/AsyncRPC
 *
 *   unfsclient page
 *   http://www.gelato.unsw.edu.au/IA64wiki/unfsclient
 */


#ifndef _CLNT_TCP_NB_H_
#define _CLNT_TCP_NB_H_

#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/socket.h>


/* User callback type */
typedef void (*user_cb)(void *msg_buf, int bufsz, void *priv);

/* Size of statically serialized RPC header */
#define MCALL_MSG_SIZE 24
#define FRAG_SIZE(uint32_hdr) ((u_int32_t)(uint32_hdr & 0x7fffffffU))
#define LAST_FRAG(uint32_hdr) ((u_int32_t)(uint32_hdr & 0x80000000U))
#define EXPECT_NEW_FRAG -1

/* RPC record header in bytes. */
#define RPC_RECORD_HEADER 4

/* Default size for read and write syscalls */
#define ASYNC_READ_BUF 4096
	
/* Size of the bucket for the xid to user callback map */
#define BUCKET_XID 100000
/* Bucket size for the file descriptor to ctdata map */
#define BUCKET_FD 10

/* Use these to specify blocking or non blocking wait while
 * calling clnttcp_nb_receive().
 */
#define RPC_BLOCKING_WAIT 0x0
#define RPC_NONBLOCK_WAIT 0x1

/* Use this to specify whether to flush the send buffer when calling
 * clnttcp_nb_receive.
 */
#define RPC_NO_TX_FLUSH 0x2

/* Use this to specify to clnttcp_nb_receive not to perform any
 * read on the socket.
 */
#define RPC_NO_RX 0x4

#define expect_new_rpc_record(rs) do {				\
		(rs)->rs_fh_remaining = RPC_RECORD_HEADER;	\
		(rs)->rs_frag_remaining = EXPECT_NEW_FRAG;	\
		(rs)->rs_recordsize = 0;			\
	} while(0);

#define is_nonblocking(flag) (flag & RPC_NONBLOCK_WAIT)
#define is_blocking(flag) (!(is_nonblocking(flag)))

#define flush_tx_buffer(flag) (!((flag) & RPC_NO_TX_FLUSH))

#define read_rpc_response(flag) (!((flag) & RPC_NO_RX))


/* Creates a non-blcking RPC handle. */
extern CLIENT *clnttcp_nb_create(struct sockaddr_in *raddr, u_long prog,
		u_long vers, int *sockp, u_int sbufsz, u_int rbufsz);

/* Creates a blocking RPC handle. */
extern CLIENT * clnttcp_b_create(struct sockaddr_in *raddr, u_long prog,
		u_long vers, int *sockp, u_int sbufsz, u_int rbufsz);

extern enum clnt_stat clnttcp_nb_call(CLIENT *handle, u_long proc,
		xdrproc_t inproc, caddr_t inargs, user_cb callback,
		void * usercb_priv);
   
extern int clnttcp_nb_receive(CLIENT * handle, int flag);
extern unsigned long clnttcp_datatx(CLIENT * handle);
extern unsigned long clnttcp_datarx(CLIENT * handle);

extern void clnttcp_nb_destroy (CLIENT *h);

#endif
