#include <rpc/rpc.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>
#include <unistd.h>

#include <nfsclient.h>

void mnt_umnt_cb(void *m, int len, void *p)
{
	fprintf(stderr, "Mount UMNT reply received: Size: %d\n",
			len);
}


int main(int argc, char *argv[])
{
	struct addrinfo *srv_addr, hints;
	int err;
	enum clnt_stat stat;
	nfs_ctx *ctx = NULL;

	if(argc < 3) {
		fprintf(stderr, "Not enough arguments\nThis tests the asynchronous libnfsclient MOUNT UMNT call interface and callback\n"
				"USAGE: nfs_mnt_dump <server> <remote_dir>\n");
		return 0;
	}

	/* First resolve server name */
	hints.ai_family = AF_INET;
	hints.ai_protocol = 0;
	hints.ai_socktype = 0;
	hints.ai_flags = 0;

	if((err = getaddrinfo(argv[1], NULL, &hints, &srv_addr)) != 0) {
		fprintf(stderr, "%s: Cannot resolve name: %s: %s\n",
				argv[0], argv[1], gai_strerror(err));
		return -1;
	}
	
	ctx = nfs_init((struct sockaddr_in *)srv_addr->ai_addr, IPPROTO_TCP, 0);
	if(ctx == NULL) {
		fprintf(stderr, "%s: Cant init nfs context\n", argv[0]);
		return 0;
	}

	stat = mount3_umnt(&argv[2], ctx, mnt_umnt_cb, NULL);
	if(stat == RPC_SUCCESS)
		fprintf(stderr, "MOUNT UMNT Call sent successfully\n");
	else
		fprintf(stderr, "Could not send MOUNT UMNT call\n");

	sleep(5);
	return 0;
}
