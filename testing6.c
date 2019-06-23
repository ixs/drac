/*
 * Test client for dracauth
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

main(argc, argv)
	int argc;
	char *argv[];
{
        int rc;
	char *host;
	unsigned char ip6[16];
	char *err;

	if (argc < 3) {
		printf("usage:  %s server_host client_addr\n", argv[0]);
		exit(1);
	}
	host = argv[1];
	inet_pton(AF_INET6, argv[2], ip6);
	rc = dracauth6(host, ip6, &err);
	if (rc != 0) printf("%s: %s\n", argv[0], err);
}
