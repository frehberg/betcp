/*
 * main.c
 *
 *  Created on: Jan 9, 2010
 *      Author: frehberg
 */

#include "rttcp_debug.h"
#include "rttcp.h"
#include <stdio.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <boost/thread.hpp>
#include <list>
#include <algorithm>
#include <exception>

using namespace std;
using namespace boost;

in_addr_t addr = inet_addr("127.0.0.1");
unsigned short port = 4444;
const struct timeval microtv = { 0, 50*1000 };
char   sendbuf[] = { 0,1,2,3,4,5,6,7 };
size_t sendbuflen = sizeof(sendbuf);
#define SOCKET_BUFSIZE (1024)

static int read_count = 0;
static int send_count = 0;
void
client_main()
{
	struct timeval tv = microtv;

	rttcp_t fd = rttcp_client(addr, port, SOCKET_BUFSIZE, &tv);
	assert(fd!=-1);

	for(int i=0; i<1000; ++i) {
		tv = microtv;

		int retval = rttcp_send(fd, sendbuf, sendbuflen, &tv);
		printf("rttcp_send() = %d\n", retval);
		assert(retval == 0 || retval == sendbuflen);
		if (retval>0)
			++send_count;
	}

	rttcp_close(fd);
}

void
server_task(rttcp_t fd)
{
	char recvbuf[SOCKET_BUFSIZE];
	size_t recvbuflen = sizeof(recvbuf);

	while(1) {
		struct timeval tv = microtv;

		int retval = rttcp_recv(fd, &recvbuf[0], recvbuflen, &tv); /* infinite wait */

		// retval one of:
		// -1 : peer closed connection
		//  0 : no message read
		//  n : read message of size n
		printf("server: rttcp_recv(%03d) returned with  %3d\n", fd, retval);
		assert(retval >= -1);

		if (retval>0) {
			++read_count;
			assert(memcmp(sendbuf, recvbuf, retval)==0);
		}

		if (retval<0) {
			// connection error: close socket and terminate
			rttcp_close(fd);
			return;
		}

		// sleep causing slow read from socket, causing congestion on sending side
		boost::xtime xt;

		boost::xtime_get(&xt, TIME_UTC);

		xt.nsec += 10*1000L*1000L;

		boost::thread::sleep(xt); // Sleep for 10 milli-seconds
	}
	return;
}

void
server_main()
{
	list<boost::thread*> tasks;
	struct timeval tv = microtv;

	rttcp_t fd = rttcp_server(addr, port, SOCKET_BUFSIZE, &tv);
	assert(fd!=-1);

	while(1) {
		struct timeval tv = microtv;

		rttcp_t newfd = rttcp_accept(fd, &tv); /* infinite wait */

		if (newfd != -1) {
			printf("spawning thread for fd %d\n", newfd);

			boost::thread *newtask = new boost::thread(&server_task, newfd);
			tasks.push_back(newtask);
		}
	}

	rttcp_close(fd);
}

int
main(int argc, char *argv[])
{
	boost::thread *server = new boost::thread(&server_main);

	sleep(1); // sleep 1 second

	boost::thread *client = new boost::thread(&client_main);

	/* wait until all sent */
	client->join(); delete client;

	try {
		/* now force server to terminate */
		server->interrupt();
		// blocking
		// server->join();

		sleep(1);

		printf("### sent %d, received %d\n", send_count, read_count);

		exit (0);

		delete server;
	}
	catch (::std::exception & exc) {
		printf("caught exception\n");
	}

}
