/*
 * main.c
 *
 *  Created on: Jan 9, 2010
 *      Author: frehberg
 */

#include "rttcp_debug.h"
#include "rttcp.h"
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

int
main(int argc, char *argv[])
{
	in_addr_t addr = inet_addr("127.0.0.1");
	unsigned short port = 4444;
	struct timeval shortTime = { 0, 10*1000 };

	rttcp_t to = rttcp_server(addr, port, 1024, &shortTime);
	assert(to!=-1);

	{
		char buf[] = { 0,1,2,3,4,5,6,7 };
		int i;

		printf("buffer size = %ld", sizeof(buf));

		rttcp_t from = rttcp_client(addr, port, 1024, &shortTime);
		assert(from!=-1);

		for(i=0; i<1000; ++i) {
			int retval = rttcp_send(from, buf, sizeof(buf), &shortTime);
			/* either transport succeeded, or nothing has been sent, but it should never be -1 */
			printf("rttcp_send() = %d\n", retval);
			assert(retval == 0 || retval == sizeof(buf));
		}

		rttcp_close(from);
	}
	rttcp_close(to);
}
