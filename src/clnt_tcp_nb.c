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
 *   More info can be found at:
 *   http://nfsreplay.sourceforge.net
 *
 *   and
 *
 *   http://www.gelato.unsw.edu.au/IA64wiki/AsyncRPC
 */



#include <unistd.h>
#include <fcntl.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <rpc/pmap_clnt.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <ght_hash_table.h>
#include <sys/select.h>
#include <inttypes.h>
#include <flist.h>

#include <clnt_tcp_nb.h>

/* writetcp registers this function as the callback for asynchronous
 * event on the TCP socket. It in turn calls an upper layer function
 * that processes the received buffers as RPC fragments. See recv_cb
 * in struct ct_nb_data later.
 */
static int readtcp_nb(char *, char *, int);

/* XDR Records layer calls this */
static int writetcp_nb(char *, char *, int);

/* The prototypes below are only for informational purposes
 * as the original code used them, but we dont. Keep them around since
 * someday we might have to refer to them for extending the
 * functionality to that provided by the functions below.
 */

#if 0
static void clnttcp_nb_abort(void);
static void clnttcp_nb_geterr(CLIENT *handle, struct rpc_err *err);
static bool_t clnttcp_nb_freeres(CLIENT *handle, xdrproc_t xdr_op , caddr_t args);
static bool_t clnttcp_nb_control(CLIENT *handle, int option, char *val);
static struct clnt_ops tcp_nb_ops =
{
	clnttcp_nb_call,
	NULL /*clnttcp_nb_abort*/,
	NULL /*clnttcp_nb_geterr*/,
	NULL /*clnttcp_nb_freeres*/,
	clnttcp_nb_destroy,
	NULL /*clnttcp_nb_control*/
};
#endif

/*
 * RPC record fragments represented using this structure
 * in memory.
 * Each incoming or outgoing fragment is inserted 
 * into an ordered list that contains fragments that constitute the
 * full RPC Record.
 * See RFC 1831, Section on Record Marking Standard.
 */
struct frag_buffer {
	struct flist_head fb_list;
	char *fb_base;
	char *fb_current;
	int fb_len;
};

/* RPC Record Stream State
 * See RFC 1831, Section on Record Marking Standard.
 *
 * This structure is used to manage the received RPC records.
 */
struct rpc_record_state {

	/* List of fragments read for the current record
	 * Fragments that constitute a full RPC
	 * record are stored in this list till the last
	 * fragment is received.
	 */
	struct flist_head rs_frag_list;

	/* Size of the record in bytes.
	 * This field is updated each time a  new fragment
	 * is associated with this record.
	 */
	int rs_recordsize;


	/* Members below are all state related to the current
	 * fragment. Its possible that we dont get the full frag in
	 * one read call.
	 */

	/* Bytes needed to complete the current fragment.
	 */
	long rs_frag_remaining;

	/* Pointer to the start of the buffer which stores the
	 * current incomplete frag
	 */
	caddr_t rs_frag_buf_base;

	/* Total size of the buffer, i.e. the size of the fragment got
	 * from the Record marker.
	 * See RFC 1831, Record Marking Standard.
	 */
	u_long rs_frag_bufsz;

	/* Offset into the buffer, where any more bytes for this
	 * fragment will be placed
	 */
	u_long rs_frag_offset;

	/* In a TCP stream it is possible that we receive even the
	 * record markers split over two read syscalls or TCP
	 * segments. We'll need to handle that before we can process
	 * any fragments.
	 * This field contains remaining size to be read, for 
	 * the fragment header.
	 */
	int rs_fh_remaining;

	/* Stores the frag header till we receive all 4 bytes
	 */
	char rs_fraghdr[4];

	/* TRUE if the last frag bit was set in the Fragment Header
	 */
	bool_t rs_last_frag;

};


/* Socket specific data */
struct ct_data
{
	int ct_sock;
	struct sockaddr_in ct_addr;
	XDR ct_xdrs;
	struct rpc_err ct_error;

	/******************************************
	 * State touched by the transmission path *
	 *****************************************/

	/* Size of buffer allocated for a single transmission
	 * message */
	int ct_sbufsz;

	/* Stores mostly static RPC header */
	char ct_mcall[MCALL_MSG_SIZE];

	/* Next offset inside static header above */
	u_int ct_mpos;
	
	/* Amount of data transferred since socket was created. */
	unsigned long ct_datatx;

	/* Amount of data received since socket was created. */
	unsigned long ct_datarx;

	/* Stores the list of pending buffers that will be sent 
	 * in the next call to writetcp_nb.
	 * Things generally end up in this buffer if the syscall
	 * that was supposed to send these returned an EAGAIN.
	*/
	struct flist_head ct_sndlist;



	/*****************************************
	 * State touched only by reception code. *
	 ****************************************/

	/* List of buffers that havent been written yet */
	struct rpc_record_state ct_record_state;
	
	/* Sizes in which socket read will be performed	 */
	int ct_rbufsz;

	/* Read buffer for use with socket. Allocated only once during
	 * initing of socket state. 
	 */
	char * ct_readbuf;


	/****************************************
	 * State used by both Tx and Rx path.	*
	 ***************************************/
	/* Maps a RPC Xid to the registered user callback */
	ght_hash_table_t *ct_xid_to_ucb;

	/* Determines whether socket is blocking or non-blocking */
	u_int64_t ct_sockflags;

	/* Number of outstanding calls */
	int ct_pendingcalls;

};

/* glibc has a function like this but its internal
 * to glibc so we need to define one for us here.
 */
unsigned long
create_xid (void)
{
	unsigned long res;
	struct timeval now;
	gettimeofday(&now, NULL);
	srand48(now.tv_sec ^ now.tv_usec);
	res = lrand48();

	return res;
}


static int
set_fd_nonblocking(int fd)
{
	int sock_flag;

	/* Set non-blocking */
	if((sock_flag = fcntl(fd, F_GETFL)) < 0)
		return -1;

	if((fcntl(fd, F_SETFL, (sock_flag | O_NONBLOCK))) < 0)
		return -1;

	return 0;
}


CLIENT *
clnttcp_b_create(struct sockaddr_in *raddr, u_long prog,
		u_long vers, int *sockp, u_int sbufsz, 
		u_int rbufsz)
{
	CLIENT *handle = NULL;
	struct ct_data *ct = NULL;
	struct rpc_msg static_cmsg;
	u_short port;
	struct rpc_createerr *cerr = NULL;

	handle = (CLIENT *)mem_alloc(sizeof(CLIENT));
	if(handle == NULL)
		return NULL;

	ct = (struct ct_data *)mem_alloc(sizeof(struct ct_data));
	if(ct == NULL)
		goto mem_free_return;

	if(raddr->sin_port == 0) {
		port = pmap_getport(raddr, prog, vers, IPPROTO_TCP);
		if(port == 0)
			/* RPC errors is already set here */
			goto mem_free_return;

		raddr->sin_port = htons(port);
	}

	if(*sockp < 0) {
		*sockp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
		if(*sockp <= 0)
			goto set_create_err_return;

		/* we dont care if a reserved port was bound */
		bindresvport(*sockp, (struct sockaddr_in *)0);
		if(connect(*sockp, (struct sockaddr *)raddr,
					sizeof(*raddr)) < 0) {
			close(*sockp);
			goto set_create_err_return;
		}
	}

	ct->ct_sock = *sockp;
	ct->ct_addr = *raddr;

	/* DEFAULT_SOCKRW_SIZE is default buffer size, in case user
	 * specified value is 0.
	 */
	ct->ct_rbufsz = (rbufsz) ? rbufsz : DEFAULT_SOCKRW_SIZE;
	ct->ct_readbuf = (char *)mem_alloc(ct->ct_rbufsz);
	if(ct->ct_readbuf == NULL)
		return NULL;

	ct->ct_sbufsz = (sbufsz) ? sbufsz : DEFAULT_SOCKRW_SIZE;

	/* By default, the socket is blocking, flushes outstanding
	 * buffers(see RPC_NO_TX_FLUSH) and blocks for
	 * replies(RPC_NO_RX).
	 */
	ct->ct_sockflags = RPC_BLOCKING_WAIT;
	ct->ct_datatx = 0;
	ct->ct_datarx = 0;
	ct->ct_pendingcalls = 0;

	expect_new_rpc_record(&ct->ct_record_state);
	ct->ct_record_state.rs_frag_buf_base = NULL;
	ct->ct_record_state.rs_frag_bufsz = 0;
	ct->ct_record_state.rs_frag_offset = 0;
	ct->ct_xid_to_ucb = ght_create(BUCKET_XID);

	INIT_FLIST_HEAD(&ct->ct_sndlist);
	INIT_FLIST_HEAD(&ct->ct_record_state.rs_frag_list);

	static_cmsg.rm_xid = create_xid();
	static_cmsg.rm_direction = CALL;
	static_cmsg.rm_call.cb_rpcvers = RPC_MSG_VERSION;
	static_cmsg.rm_call.cb_prog = prog;
	static_cmsg.rm_call.cb_vers = vers;

	xdrmem_create(&(ct->ct_xdrs), ct->ct_mcall, MCALL_MSG_SIZE,
			XDR_ENCODE);

	if(!xdr_callhdr(&(ct->ct_xdrs), &static_cmsg)) {
		close(*sockp);
		goto mem_free_return;
	}

	ct->ct_mpos = XDR_GETPOS(&(ct->ct_xdrs));
	XDR_DESTROY(&(ct->ct_xdrs));

	xdrrec_create(&(ct->ct_xdrs), sbufsz, rbufsz, 
			(caddr_t)ct, readtcp_nb, writetcp_nb);

	/* libc needs table of function pointers, but we've diverged
	 * so much that its just not needed anymore because we do our
	 * own calls to send and receive functions.
	 */
	/* handle->cl_ops = &tcp_nb_ops; */
	handle->cl_private = (caddr_t)ct;
	handle->cl_auth = authnone_create();

	return handle;

set_create_err_return:
	cerr = &get_rpc_createerr();
	cerr->cf_stat = RPC_SYSTEMERROR;
	cerr->cf_error.re_errno = errno;

mem_free_return:
	if(handle != NULL)
		mem_free(handle, sizeof(CLIENT));

	if(ct != NULL)
		mem_free(ct, sizeof(struct ct_data));

	return NULL;
}


int
clnttcp_sock_setflag(CLIENT * handle, u_int64_t flags)
{
	int sockfd;
	struct ct_data * ct = NULL;

	if(handle == NULL)
		return -1;

	ct = (struct ct_data *)handle->cl_private;
	if(ct == NULL)
		return -1;

	sockfd = ct->ct_sock;

	if(is_nonblocking(flags)) {
		if(set_fd_nonblocking(sockfd) < 0) {
			return -1;
		}

		ct->ct_sockflags |= RPC_NONBLOCK_WAIT;
	}

	return 0;
}


CLIENT *
clnttcp_nb_create(struct sockaddr_in *raddr, u_long prog,
		u_long vers, int *sockp, u_int sbufsz, 
		u_int rbufsz)
{
	CLIENT * handle = NULL;

	handle = clnttcp_b_create(raddr, prog, vers, sockp, sbufsz, rbufsz);

	if(handle == NULL)
		return NULL;

	if((clnttcp_sock_setflag(handle, RPC_NONBLOCK_WAIT)) < 0) {
		clnttcp_nb_destroy(handle);
		return NULL;
	}

	return handle;
}



enum clnt_stat 
clnttcp_nb_call(CLIENT *handle, struct rpc_proc_info rpc, struct callback_info ucbi,
		int64_t callflag)
{
	struct ct_data *ct = NULL;
	u_int32_t *xid, xid_host;
	XDR *xdrs = NULL;
	struct callback_info * cbi = NULL;

	if(handle == NULL)
		return RPC_FAILED;

	ct = (struct ct_data *)handle->cl_private;
	if(ct == NULL)
		return RPC_FAILED;

	cbi = (struct callback_info *)malloc(sizeof(struct callback_info));
	if(cbi == NULL)
		return RPC_SYSTEMERROR;

	*cbi = ucbi;
	xdrs = &ct->ct_xdrs;
	/* Keep a copy of the xid being sent */
	xid = (u_int32_t *)ct->ct_mcall;
	--(*xid);
	xid_host = ntohl(*xid);
	xdrs->x_op = XDR_ENCODE;
	ct->ct_error.re_status = RPC_SUCCESS;

	/* Insert the callback into the hashtable */
	ght_insert(ct->ct_xid_to_ucb, (void *)cbi, sizeof(u_int32_t),
			(void *)&xid_host);
	/* Send the static rpc header followed by the xdr'd message */
	if((!XDR_PUTBYTES(xdrs, ct->ct_mcall, ct->ct_mpos))
			|| (!XDR_PUTLONG (xdrs, (long *) &rpc.proc))
			|| (!AUTH_MARSHALL (handle->cl_auth, xdrs))
			|| (!(*rpc.inproc)(xdrs, rpc.inargs))
			|| (!xdrrec_endofrecord(xdrs, TRUE))) {
		/* If previously set status is still RPC_SUCCESS then
		 * the problem is in encoding of args, return that */
		ct->ct_error.re_status = RPC_CANTENCODEARGS;
		return ct->ct_error.re_status;
	}

	++ct->ct_pendingcalls;
	if(callflag == RPC_DEFAULT_FLAGS)
		callflag = ct->ct_sockflags;
	clnttcp_nb_receive(handle, callflag);
	return RPC_SUCCESS;
}

/* If nocopy is true, just add the pointer in buffer to the buffer
 * list instead of copying the buffer.
 */
static int 
add_buffer_list(struct flist_head *head, char *buf, int len, bool_t nocopy)
{
	struct frag_buffer *new_frag = NULL;
	int bufsize;

	if((head == NULL) || (buf == NULL))
		return -1;

	if (!nocopy)
		bufsize = sizeof(struct frag_buffer) + (sizeof(char) + len);
	else
		bufsize = sizeof(struct frag_buffer);

	new_frag = (struct frag_buffer *)malloc(bufsize);
	if(new_frag == NULL)
		return -1;

	if(!nocopy) {
		new_frag->fb_base = (char *)(new_frag + 1);
		if(new_frag->fb_base == NULL) {
			free(new_frag);
			return -1;
		}
		memcpy(new_frag->fb_base, buf, len);
	}
	else
		new_frag->fb_base = buf;

	new_frag->fb_len = len;
	new_frag->fb_current = new_frag->fb_base;

	flist_add_tail(&new_frag->fb_list, head);

	return 0;
}

static int 
update_new_frag_state(struct rpc_record_state *rs, char *buf, int bufsz)
{
	int consumed, rm_len;
	u_int32_t fraghdr;
	char *fhdr = NULL;

	consumed = 0;
	if((rs == NULL) || (buf == NULL))
		return 0;

	/* If we havent received the full frag header, get that first.
	 */
	if(rs->rs_fh_remaining) {
		if(bufsz < rs->rs_fh_remaining)
			rm_len = bufsz;
		else
			rm_len = rs->rs_fh_remaining;
		
		memcpy((&rs->rs_fraghdr[0]), buf, rm_len);
		rs->rs_fh_remaining -= rm_len;
		consumed += rm_len;
	}

	/* If the frag header is still not complete
	 * return to caller and come back with more data in the buffer
	 * after reading from socket.
	 */
	if(rs->rs_fh_remaining)
		return consumed;

	fhdr = &rs->rs_fraghdr[0];
	memcpy(&fraghdr,fhdr, sizeof(u_int32_t));
	rs->rs_frag_bufsz = FRAG_SIZE((u_int32_t)(ntohl(fraghdr)));
	rs->rs_last_frag = LAST_FRAG((u_int32_t)(ntohl(fraghdr)));
	rs->rs_frag_buf_base = (caddr_t)mem_alloc(rs->rs_frag_bufsz);
	if(rs->rs_frag_buf_base == NULL)
		return 0;

	rs->rs_frag_remaining = rs->rs_frag_bufsz;
	rs->rs_frag_offset = 0;

	return consumed;
}

/* Size is returned through bsize */
static caddr_t
collate_buf_list(struct rpc_record_state *rs, u_long *bsize)
{
	u_long bufsize = 0;
	struct frag_buffer * fbuf = NULL;
	struct flist_head *iter, *tmp;
	iter = tmp = NULL;
	caddr_t rpc_msg, curr = NULL;

	if(rs == NULL)
		return NULL;

	bufsize = rs->rs_recordsize;
	rpc_msg = (caddr_t)mem_alloc(bufsize * sizeof(char));
	if(rpc_msg == NULL)
		return NULL;
	curr = rpc_msg;
	
	flist_for_each_safe(iter, tmp, &(rs->rs_frag_list)) {
		flist_del(iter);
		fbuf = flist_entry(iter, struct frag_buffer, fb_list);
		memcpy(curr, fbuf->fb_base, fbuf->fb_len);
		curr += fbuf->fb_len;
		free(fbuf);
	}

	*bsize = bufsize;
	return rpc_msg;
}

static void
call_user_cb(struct ct_data *ct)
{
	XDR xdr;
	caddr_t rpc_msg = NULL;
	struct rpc_record_state *rs = NULL;
	u_long bufsize;
	struct rpc_msg msg;
	struct callback_info * cbi = NULL;
	ght_iterator_t tbliter;
	u_int32_t * cached_xid = NULL;

	if(ct == NULL)
		return;

	rs = &(ct->ct_record_state);
	if(rs == NULL)
		return;

	msg.acpted_rply.ar_verf = _null_auth;
	msg.acpted_rply.ar_results.where = NULL;
	msg.acpted_rply.ar_results.proc = (xdrproc_t)(xdr_void);

	/* Aggregate the frag buffers into a contiguous area */
	if((rpc_msg = collate_buf_list(rs, &bufsize)) == NULL)
		return;
	
	xdrmem_create(&xdr, rpc_msg, bufsize, XDR_DECODE);
	if(!xdr_replymsg(&xdr, &msg))
		goto mem_free_return;

	ct->ct_datarx += bufsize;
	/* If ever we get around to having our own xdr translation
	 * library, try to move this hash table lookup and the check
	 * that follows to much before the de-xdring of the whole rpc
	 * message. There is no point spending time de-xdring the
	 * message when there is no callback which needs the de-xdr'd
	 * message. This will require the ability to extract the
	 * RPC header and the message payload separately.
	 */

	/* First check for callback info in the cached entry. */
	if((cbi = ght_first(ct->ct_xid_to_ucb, &tbliter,
					(const void **)&cached_xid)) == NULL)
		goto mem_free_return;

	if(*cached_xid != msg.rm_xid) {
		cbi = ght_remove(ct->ct_xid_to_ucb, sizeof(u_int32_t),
				(void *)&msg.rm_xid);
		if(cbi == NULL)
			goto mem_free_return;
	}
	else
		cbi = ght_remove_first(ct->ct_xid_to_ucb);

	/* This is very xdrmem specific. I need the pointer to
	 * location from which NFS data is located, right after the
	 * RPC msg body ends. x_private member is that pointer.
	 * x_handy is the remaining size.
	 */
	if(cbi->callback != NULL)
		cbi->callback(xdr.x_private, xdr.x_handy, cbi->cb_private);

	free(cbi);

mem_free_return:
	mem_free(rpc_msg, bufsize);

	return;
}

static int
update_record_state(struct ct_data *ct)
{
	struct rpc_record_state *rs = NULL;
	int called_back = 0;
	
	if(ct == NULL)
		return called_back;

	rs = &(ct->ct_record_state);
 	if(rs == NULL)
		return called_back;

	/* Add the complete fragment to the list of fragments for the
	 * currently incomplete record.
	 */
	add_buffer_list(&(rs->rs_frag_list),  rs->rs_frag_buf_base,
			rs->rs_frag_bufsz, TRUE);
	rs->rs_recordsize += rs->rs_frag_bufsz;
	
	/* If this is the last frag for the record, call the
	 * registered callback since we now have the complete RPC
	 * message.
	 */
	if(rs->rs_last_frag) {
		call_user_cb(ct);
		called_back = 1;
	}

	/* The first thing functions that follow, will look for is a
	 * the frag header which is always 4 bytes length.
	 */
	expect_new_rpc_record(rs);
	return called_back;
}


static int
update_frag_state(struct ct_data *ct, char *buf, int bufsz)
{
	int tocopy = 0;
	int consumed = 0;
	struct rpc_record_state *rs = NULL;
	int called_back = 0;

	rs = &(ct->ct_record_state);
	if((rs == NULL) || (buf == NULL))
		return called_back;

	while(bufsz > consumed) {
		/* If the current frag expects more */
		if(rs->rs_frag_remaining > 0) {
			tocopy = ((bufsz - consumed) < rs->rs_frag_remaining)?
				(bufsz - consumed) : rs->rs_frag_remaining;
			memcpy((rs->rs_frag_buf_base + rs->rs_frag_offset),
					(buf + consumed), tocopy);
			rs->rs_frag_remaining -= tocopy;
			rs->rs_frag_offset += tocopy;
			consumed += tocopy;
		}

		/* If the current fragment is complete, send it a
		 * layer up to be integrated into the record it
		 * belongs to.
		 */
		if((rs->rs_frag_offset == rs->rs_frag_bufsz) &&
				(rs->rs_frag_remaining == 0))
			called_back += update_record_state(ct);

		/* We just finished a record in the middle of the read
		 * buffer, initialize the new fragment.
		 */
		if(consumed < bufsz)
			consumed += update_new_frag_state(rs, (buf + consumed),
					(bufsz - consumed));
	}

	return called_back;
}

/* Returns the count of callbacks executed. */
static int 
rpc_cb(int fd, struct ct_data *ct, int flag)
{
	char *rbuf = NULL;
	int toread, read_len = 0;
	int called_back = 0;
	fd_set rset;
	int fd_count;

	if(ct == NULL)
		return 0;

	toread = ct->ct_rbufsz;
	rbuf = ct->ct_readbuf;

	if((is_nonblocking(ct->ct_sockflags)) && (is_blocking(flag))) {
block_again:
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		fd_count = select(fd + 1, &rset, NULL, NULL, NULL);
		if(fd_count < 0)
			goto block_again;

		if(!FD_ISSET(fd, &rset))
			goto block_again;
	}

	/* Read the buffer and simply pass it onto the fragment and
	 * record handler
	 */
	while((read_len = read(fd, rbuf, toread)) > 0) {
		called_back = update_frag_state(ct, rbuf, read_len);
		/* If we processed enough buffers to invoke one or
		 * more callbacks, we should return before processing any
		 * further buffers so that the caller can have a
		 * chance of processing its part of a callback
		 * invocation.
		 */
		if(called_back)
			break;
		
		/* if socket is non-blocking but in this instance, it
		 * needs blocking behaviour, we should use select to
		 * wait and not loop in this read loop.
		 */
		if(is_nonblocking(ct->ct_sockflags)) {
			if(is_blocking(flag))
				goto block_again;
			else
				break;
		}
	}

	return called_back;
}

static int 
readtcp_nb(char *ct_handle, char *buf, int len)
{

	return 0;
}

static int
send_buffers(int sockfd, struct ct_data * ct, int flag)
{
	struct frag_buffer * buf = NULL;
	struct flist_head *iter, *tvar;
	int written;
	fd_set wset;
	int fd_count;
	struct flist_head * head = NULL;
	iter = head = tvar = NULL;

	if(ct == NULL)
		return -1;

	head = &(ct->ct_sndlist);
	if(head == NULL)
		return -1;
	
	flist_for_each_safe(iter, tvar, head) {
		buf = flist_entry(iter, struct frag_buffer, fb_list);
		/* If the socket is non-blocking but for this
		 * invocation we need a blocking.
		 */
		if((is_nonblocking(ct->ct_sockflags)) && (is_blocking(flag))) {
write_block_again:
			FD_ZERO(&wset);
			FD_SET(sockfd, &wset);
			fd_count = select(sockfd + 1, NULL, &wset, NULL, NULL);
			if(fd_count < 0)
				goto write_block_again;

			if(!FD_ISSET(sockfd, &wset))
				goto write_block_again;
		}
	
		errno = 0;
		written = write(sockfd, buf->fb_current, buf->fb_len);

		if(written < 0) {
			if(errno != EAGAIN)
				return -1;
			/* EAGAIN with written < 0 should only happen when
			 * socket is O_NONBLOCK. Now if this 
			 * invocation of send_buffers requires blocking	
			 * wait for data, then go back to select above. 
			 */
			if(is_blocking(flag))
				goto write_block_again;
			else
				return 0;
		}

		ct->ct_datatx += written;
		/* Write returned after writing partial
		 * buffer. Update the buffer state now.
		 */
		if(written < buf->fb_len) {
			buf->fb_current += written;
			buf->fb_len -= written;

			/* Again, if partial write happens, we need to
			 * know if blocking write is required. If it
			 * is, then head back up and wait for for
			 * readiness.
			 */
			if(is_blocking(flag))
				goto write_block_again;
			else
				return 0;
		}

		flist_del(iter);
		free(buf);
	}

	return 0;
}

int
clnttcp_nb_receive(CLIENT * handle, int flag)
{
	struct ct_data * ct = NULL;
	int called_back = 0;

	if(handle == NULL)
		return 0;

	ct = (struct ct_data *)handle->cl_private;
	if(ct == NULL)
		return 0;

	/* Dont flush the buffer in this invocation if the flag
	 * specifies so.
	 */
	if(flush_tx_buffer(flag))
		send_buffers(ct->ct_sock, ct, flag);
	/* If there are no pending calls, what am I supposed to
	 * receive a reply for.
	 */
	if(ct->ct_pendingcalls == 0)
		return 0;

	/* Dont go reading from the socket if the flag says so.
	 */
	if(read_rpc_response(flag)) {
		called_back = rpc_cb(ct->ct_sock, ct, flag);
		ct->ct_pendingcalls -= called_back;
	}
	return called_back;
}


static int 
writetcp_nb(char *ct_handle, char *buf, int len)
{
	struct ct_data *ct = (struct ct_data *)ct_handle;

	if(ct == NULL)
		return -1;

	if((add_buffer_list(&(ct->ct_sndlist), buf, len, FALSE)) < 0)
		return -1;

	/* At this point there is at least one buffer pending in the
	 * list.
	 * We want the record stream layer above us to continue
	 * normally, even though we might not have written all the
	 * data given to us. But we have copied it in for sending at a 
	 * time when we cant block.
	 */
	if((send_buffers(ct->ct_sock, ct, RPC_NONBLOCK_WAIT)) == 0)
		return len;
	else
		return -1;
}


void
clnttcp_nb_destroy (CLIENT *h)
{
	struct ct_data *ct = NULL;
	struct rpc_record_state *rs = NULL;
	struct frag_buffer * fb = NULL;
	struct flist_head *iter, *tmp;
	iter = tmp = NULL;

	if(h == NULL)
		return;

	ct = (struct ct_data *)h->cl_private;
	if(ct == NULL)
		goto hfree;

	close(ct->ct_sock);
	ght_finalize(ct->ct_xid_to_ucb);

	rs = &(ct->ct_record_state);
	if(rs != NULL) {
		flist_for_each_safe(iter, tmp, &(rs->rs_frag_list)) {
			fb = flist_entry(iter, struct frag_buffer, fb_list);
			flist_del(iter);
			mem_free(fb, sizeof(struct frag_base));
		}
	}

	flist_for_each_safe(iter, tmp, &(ct->ct_sndlist)) {
		fb = flist_entry(iter, struct frag_buffer, fb_list);
		flist_del(iter);
		mem_free(fb, sizeof(struct frag_base));
	}

	mem_free ((caddr_t)ct, sizeof(struct ct_data));

hfree:
	mem_free ((caddr_t)h, sizeof(CLIENT));
}

unsigned long 
clnttcp_datatx(CLIENT * handle)
{
	struct ct_data * ct = NULL;

	if(handle == NULL)
		return 0;

	ct = (struct ct_data *)handle->cl_private;
	if(ct == NULL)
		return 0;

	return ct->ct_datatx;
}

unsigned long 
clnttcp_datarx(CLIENT * handle)
{
	struct ct_data * ct = NULL;

	if(handle == NULL)
		return 0;

	ct = (struct ct_data *)handle->cl_private;
	if(ct == NULL)
		return 0;

	return ct->ct_datarx;
}
