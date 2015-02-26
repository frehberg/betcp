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
#include <sys/time.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <boost/thread/thread.hpp>
#include <list>
#include <algorithm>


using namespace std;

/**
 */
typedef struct {
	struct timeval creationTime;
	unsigned int sequenceId;
} payload_t;


in_addr_t addr = inet_addr("127.0.0.1");
unsigned short port = 4444;
const struct timeval microtv = { 0, 50*1000 };


char sendbuf[sizeof(payload_t)];
size_t sendbuflen = sizeof(payload_t);

#define SOCKET_BUFSIZE (1024)

static int read_count = 0;
static int send_count = 0;

/**
 *
 */
static void
payload_init(payload_t *p, unsigned int sequenceId)
{
	gettimeofday(&(p->creationTime), NULL);
	p->sequenceId = sequenceId;
}

/**
 *
 */
static void
payload_serialize(payload_t *p, char *buf)
{
	payload_t *net = (payload_t*) buf;

	net->creationTime.tv_sec = htonl(p->creationTime.tv_sec);
	net->creationTime.tv_usec = htonl(p->creationTime.tv_usec);
	net->sequenceId = htonl(p->sequenceId);
}

#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

static char*
print_time (struct timeval *tv, char *buf, size_t buflen)
{
	struct tm* ptm;
	char time_string[40];
	long milliseconds;

	/* Obtain the time of day, and convert it to a tm struct. */
	ptm = localtime (&(tv->tv_sec));
	/* Format the date and time, down to a single second. */
	strftime (time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", ptm);
	/* Compute milliseconds from microseconds. */
	milliseconds = tv->tv_usec / 1000;
	/* Print the formatted time, in seconds, followed by a decimal point
	and the milliseconds. */
	snprintf (buf, buflen, "%s.%03ld\n", time_string, milliseconds);

	return buf;
}

/**
 *
 */
static void
payload_init_from_buf(payload_t *p, char *buf)
{
	payload_t *net = (payload_t*) buf;

	p->creationTime.tv_sec = ntohl(net->creationTime.tv_sec);
	p->creationTime.tv_usec = ntohl(net->creationTime.tv_usec);
	p->sequenceId = ntohl(net->sequenceId);
}

void
client_main()
{
	struct timeval tv = microtv;
	payload_t payload;
	payload_t copy;
	char time_string[80];

	rttcp_t fd = rttcp_client(addr, port, SOCKET_BUFSIZE, &tv);
	assert(fd!=-1);

	for(int i=0; i<1000; ++i) {
		tv = microtv;

		payload_init(&payload, i);
		payload_serialize(&payload, sendbuf);
		payload_init_from_buf(&copy, sendbuf);
		assert(copy.creationTime.tv_sec == payload.creationTime.tv_sec);
		assert(copy.creationTime.tv_usec == payload.creationTime.tv_usec);

		printf("sending %4u at %s\n", i,
				print_time(&(payload.creationTime), time_string, sizeof(time_string)));
		int retval = rttcp_send(fd, sendbuf, sendbuflen, &tv);

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
	payload_t payload;
	char time_string[80];

	while(1) {
		struct timeval now;
		struct timeval delay;

		struct timeval tv = microtv;

		gettimeofday(&now, NULL);

		int retval = rttcp_recv(fd, &recvbuf[0], recvbuflen, &tv); /* infinite wait */
		assert(retval >= -1);

		assert(retval == 0 || retval == sizeof(payload_t));

		payload_init_from_buf(&payload, recvbuf);

		timersub(&(payload.creationTime), &now, &delay);

		// should not be in range of seconds
		// assert(delay.tv_sec == 0);
		// assert(delay.tv_usec >= 0);

		// retval one of:
		// -1 : peer closed connection
		//  0 : no message read
		//  n : read message of size n
		printf("server: received id (%05u) - delayed with secs %s\n", payload.sequenceId,
				print_time(&delay, time_string, sizeof(time_string)));

		printf("                            - created at %s\n",
				print_time(&(payload.creationTime), time_string, sizeof(time_string)));

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
		// boost::xtime xt;
		//
		// boost::xtime_get(&xt, boost::TIME_UTC);
		//
		// xt.nsec += 10*1000L*1000L;
		//
		// boost::thread::sleep(xt); // Sleep for 10 milli-seconds
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
	catch (exception & exc) {
		printf("caught exception\n");
	}

}
