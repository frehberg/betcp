/*
 * rttcp.c
 *
 *  Created on: Jan 6, 2010
 *      Author: frehberg
 */
#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include <string.h> /* for memset */
 #include <sys/ioctl.h>
#include <linux/sockios.h> /* for SIOCOUTQ */


#include "rttcp.h"

#include "rttcp_stat.h"

#if 1
#define LOG(_a0)      printf(_a0); printf("\n")
#define LOG1(_a0,_a1) printf(_a0, _a1); printf("\n")
#else
#define LOG(_a0)
#define LOG1(_a0,_a1)
#endif

#define SOCK_ERROR    (-1)
#define RTTCP_BACKLOG (5)
#define RTTCP_BUFMEM_DEF 4096
#define RTTCP_HEADER_SIZE (4)

/**
 *
 */
#define rttcp_timersub(a, b, result)                                          \
  do {                                                                        \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                             \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                          \
    if ((result)->tv_usec < 0) {                                              \
      --(result)->tv_sec;                                                     \
      (result)->tv_usec += 1000000;                                           \
    }                                                                         \
  } while (0)

/* Using the fd_set as to indicate required */
static pthread_mutex_t do_oob_mux = PTHREAD_MUTEX_INITIALIZER;
static fd_set do_oob;

/*  thread safe */
#define RTTCP_OOB_FLAG_SET(_fd) \
	do { pthread_mutex_lock(&do_oob_mux); FD_SET((_fd), &do_oob); pthread_mutex_unlock(&do_oob_mux); } while(0)

/*  thread safe */
#define RTTCP_OOB_FLAG_CLEAR(_fd) \
	do { pthread_mutex_lock(&do_oob_mux); FD_CLR((_fd),&do_oob); pthread_mutex_unlock(&do_oob_mux); } while(0)

/*  thread safe */
#define RTTCP_OOB_FLAG_ISSET(_fd) \
	FD_ISSET((_fd),&do_oob)



/**
 *
 */
static int
_rttcp_socket_new(int family,
		const struct sockaddr *addr,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv);

/**
 *
 */
static void
_rttcp_socket_close(int fd);

/**
 *
 */
static int
_rttcp_setbufsize(int fd,
		int bufsize);

/**
 *
 */
static int
_rttcp_getbufsize(int fd);

/**
 *
 */
static int
_rttcp_setlinger(int fd,
		int lon,
		int linger);

/**
 *
 */
static int
_rttcp_setnodelay(int fd,
		int noDelay);


/**
 *
 */
static int
_rttcp_connect(int fd,
		int family,
		const struct sockaddr *sin,
		const socklen_t buflen,
		struct timeval *tv);


/**
 *
 */
static int
_rttcp_setrecvtimeout(int fd,
		struct timeval *recvtv);

/**
 *
 */
static int
_rttcp_setsendtimeout(int fd,
		struct timeval *sendtv);


/**
 *
 */
static rttcp_t
_rttcp_client_new(int family,
		const struct sockaddr *local,
		const struct sockaddr *peer,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv);

/**
 *
 */
static rttcp_t
_rttcp_server_new(int family,
		const struct sockaddr *addr,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv);

/**
 *
 */
static int
_rttcp_setbufsize(int fd,
		int payloadsize)
{
	assert(fd>=0);
	{
		int result = SOCK_ERROR;
		int buffsize = payloadsize + RTTCP_HEADER_SIZE;
		result = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, sizeof(buffsize));
		if (result == SOCK_ERROR) {
			LOG("setsockopt(SO_SNDBUF) failed");
			return SOCK_ERROR;
		}
		result = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffsize, sizeof(buffsize));
		if (result == SOCK_ERROR) {
			LOG("setsockopt(SO_RCVBUF) failed");
			return SOCK_ERROR;
		}
		return 0;
	}
}

/**
 *
 */
static int
_rttcp_getbufsize(int fd)
{
	assert(fd>=0);
	{
		int buffsize;
		socklen_t lon = sizeof(buffsize);

		int result = getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffsize, &lon);
		if (result == SOCK_ERROR) {
			LOG("getsockopt(SO_SNDBUF) failed");
			return SOCK_ERROR;
		}

		return buffsize;
	}
}

/**
 *
 */
static int
_rttcp_setlinger(int fd,
		int lon,
		int linger)
{
	assert(fd>=0);
	{
		struct linger l = {(lon ? 1 : 0), linger};
		int ret = setsockopt(fd, SOL_SOCKET, SO_LINGER, &l, sizeof(l));
		if (ret == SOCK_ERROR) {
		  LOG("setsockopt(SOL_SOCKET, SO_LINGER) failed");
		  return SOCK_ERROR;
		}
		return ret;
	}
}

/**
 *
 */
static int
_rttcp_setnodelay(int fd,
		int noDelay)
{
	assert(fd>=0);
	{
		// Set socket to NODELAY
		int v = noDelay ? 1 : 0;
		int ret = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &v, sizeof(v));
		if (ret == SOCK_ERROR) {
		  LOG("setsockopt(IPPROTO_TCP, TCP_NODELAY) failed");
		  return SOCK_ERROR;
		}
		return ret;
	}
}

/**
 *
 */
static int
_rttcp_setrecvtimeout(int fd,
		struct timeval *recvtv)
{
	assert(fd>=0);

	{
		int ret = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, recvtv, sizeof(*recvtv));
		if (ret == SOCK_ERROR) {
			LOG("setsockopt(SOL_SOCKET, SO_RCVTIMEO) failed");
		}
		return ret;
	}
}

/**
 *
 */
static int
_rttcp_setsendtimeout(int fd,
		struct timeval *sendtv)
{
	assert(fd>=0);
	{
		int ret = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, sendtv, sizeof(*sendtv));
		if (ret == SOCK_ERROR) {
			LOG("setsockopt(SOL_SOCKET, SO_SNDTIMEO) failed");
		}
		return ret;
	}
}


/**
 *
 */
static int
_rttcp_socket_new(int family,
		const struct sockaddr *sin,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv)
{
	assert(sin!=NULL);
	{
		int fd;
		int retval;

		fd = socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
		if(fd == SOCK_ERROR)
		{
			LOG("error opening socket");
			return SOCK_ERROR;
		}

		retval = _rttcp_setbufsize(fd, bufsize);
		if (retval == SOCK_ERROR) {
			_rttcp_socket_close(fd);
			return SOCK_ERROR;
		}

		/* send packets as they are without delay, similar as done with UDP */
		retval = _rttcp_setnodelay(fd, 1 /* nodelay=TRUE */);
		if (retval == SOCK_ERROR) {
			_rttcp_socket_close(fd);
			return SOCK_ERROR;
		}

		if(bind(fd, sin, addrlen) == SOCK_ERROR)
		{
			LOG("error binding socket");
			close(fd);
			return SOCK_ERROR;
		}

		RTTCP_OOB_FLAG_CLEAR(fd);

		return fd;
	}
}

/**
 *
 */
static int
_rttcp_connect(int fd,
		int family,
		const struct sockaddr *sin,
		const socklen_t addrlen,
		struct timeval *tv)
{
	fd_set wfds;
	int result = SOCK_ERROR;
	int retval = 0;
	int val;
	socklen_t lon;

	retval = connect(fd, sin, addrlen);
	if (retval == SOCK_ERROR && errno !=  EINPROGRESS) {
		LOG("connect failed");
		return SOCK_ERROR;
	}

	FD_ZERO(&wfds);

	FD_SET(fd, &wfds);

	/* wait timeout until connection established */
	retval = select(fd+1, NULL, &wfds, NULL, tv);

	if (retval <= 0) {
		LOG("connect failed or timed out");
		return SOCK_ERROR;
	}

	// Ensure the socket is connected and that there are no errors set
	lon = sizeof(int);
	retval = getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&val, &lon);
	if (retval == SOCK_ERROR) {
		LOG("getsockopt(SOL_SOCKET, SO_ERROR) failed");
		return SOCK_ERROR;
	}

	return val==0 ? 0 : SOCK_ERROR;
}


/**
 *
 */
static void
_rttcp_socket_close(int fd)
{
	if (close(fd) == -1) {
		LOG("error closing socket");
	}
}


/**
 *
 */
static rttcp_t
_rttcp_server_new(int family,
		const struct sockaddr *addr,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv)
{
	int fd = SOCK_ERROR;
	int ret = 0;
	int retval = 0;

	fd = _rttcp_socket_new(family, addr, addrlen, bufsize, tv);
	if (fd == SOCK_ERROR) {
		return SOCK_ERROR;
	}

	retval = listen(fd, RTTCP_BACKLOG); /* todo, what is the best backlog value nowadays? */
	if (retval == SOCK_ERROR) {
		_rttcp_socket_close(fd);
		return SOCK_ERROR;
	}

 	return (rttcp_t) fd;
}

/**
 *
 */
static rttcp_t
_rttcp_client_new(int family,
		const struct sockaddr *local,
		const struct sockaddr *peer,
		const socklen_t addrlen,
		size_t bufsize,
		struct timeval *tv)
{
	int fd = SOCK_ERROR;
	rttcp_t obj = SOCK_ERROR;
	int retval = 0;

	/* bind to local default address and port */
	fd = _rttcp_socket_new(family, local, addrlen, bufsize, tv);
	if (fd == SOCK_ERROR) {
		return SOCK_ERROR;
	}

	retval = _rttcp_connect(fd, family, peer, addrlen, tv);
	if (retval == SOCK_ERROR) {
		_rttcp_socket_close(fd);
		return SOCK_ERROR;
	}

 	return (rttcp_t) fd;
}

/**
 *
 */
rttcp_t
rttcp_server(const in_addr_t addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv)
{
	rttcp_t obj = SOCK_ERROR;
	struct sockaddr_in sin;

	assert(tv!=NULL);

	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = addr; /* inet_addr(address); */
	sin.sin_family = AF_INET;

	/* default size */
	bufsize = (bufsize == 0)
			? RTTCP_BUFMEM_DEF
			: bufsize;

	obj = _rttcp_server_new(AF_INET, (struct sockaddr*) &sin, sizeof(sin), bufsize, tv);

	return obj;
}

/**
 *
 */
rttcp_t
rttcp_server6(const struct in6_addr *addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv)
{
	rttcp_t obj = SOCK_ERROR;
	struct sockaddr_in6 sin;

	assert(addr!=NULL);
	assert(tv!=NULL);

	memset(&sin, 0, sizeof(struct sockaddr_in6));

	sin.sin6_addr = *addr; /* copy struct */
	sin.sin6_port = htons(port);
	sin.sin6_family = AF_INET6; /* ipv6 */

	/* default size */
	bufsize = (bufsize == 0)
			? RTTCP_BUFMEM_DEF
			: bufsize;

	obj = _rttcp_server_new(AF_INET6, (struct sockaddr*) &sin, sizeof(sin), bufsize, tv);

	return obj;
}
/**
 *
 */
rttcp_t
rttcp_client(const in_addr_t addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv)
{
	rttcp_t obj = SOCK_ERROR;
	struct sockaddr_in sin;
	struct sockaddr_in local_sin;

	assert(tv!=NULL);

	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = addr; /* inet_addr(address); */
	sin.sin_family = AF_INET;

	local_sin.sin_port = 0;
	local_sin.sin_addr.s_addr = INADDR_ANY;
	local_sin.sin_family = AF_INET;

	/* default size */
	bufsize = (bufsize == 0)
			? RTTCP_BUFMEM_DEF
			: bufsize;


	obj = _rttcp_client_new(AF_INET,
			(struct sockaddr*) &local_sin,
			(struct sockaddr*) &sin,
			sizeof(sin),
			bufsize,
			tv);

	return obj;
}

/**
 *
 */
rttcp_t
rttcp_client6(const struct in6_addr *addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv)
{
	rttcp_t obj = SOCK_ERROR;
	struct sockaddr_in6 sin;
	struct sockaddr_in6 local_sin;

	assert(addr!=NULL);
	assert(tv!=NULL);
	memset(&sin, 0, sizeof(struct sockaddr_in6));
	memset(&local_sin, 0, sizeof(struct sockaddr_in6));

	sin.sin6_addr = *addr; /* copy struct */
	sin.sin6_port = htons(port);
	sin.sin6_family = AF_INET6; /* ipv6 */

	local_sin.sin6_addr = in6addr_any;
	local_sin.sin6_port = 0;
	local_sin.sin6_family = AF_INET6;

	/* default size */
	bufsize = (bufsize == 0)
			? RTTCP_BUFMEM_DEF
			: bufsize;


	obj = _rttcp_client_new(AF_INET6,
			(struct sockaddr*) &local_sin,
			(struct sockaddr*)  &sin,
			sizeof(sin),
			bufsize,
			tv);

	return obj;
}




/** This operation blocks until client connects,
 * to avoid blocking use operation select or pselect to wait for events.
 */
rttcp_t
rttcp_accept(rttcp_t fd,
		struct timeval *tv)
{
	int newfd = SOCK_ERROR;

	fd_set rfds;

	/* struct timeval tv; */
	int retval;

	FD_ZERO(&rfds);

	FD_SET(fd, &rfds);

	retval = select(fd+1, &rfds, NULL, NULL, tv);
	/* Don't rely on the value of tv now! */

	if (retval <= 0) {
		/* select timedout or failed */
		return SOCK_ERROR;
	}

	newfd = accept4(fd, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
	if (newfd < 0) {
		return SOCK_ERROR;
	}

	/* accept succeeded */
	return newfd;
}

#define MARKER  'D'
#define MAGIC   'X'
#define VERSION (1)
#define HIGH_BYTE(_val) ((char) (((_val) & 0xffff) > 8))
#define LOW_BYTE(_val)  ((char)  ((_val) & 0xff))

static int
_rttcp_send_message(int fd,
		size_t offset,
		const void *hdr,
		size_t hdrlen,
		const void *buf,
		size_t buflen)
{
	int retval;

	/* 4 octet header  */
	/* char prelude[4] =
	{ MAGIC, VERSION, HIGH_BYTE(len), LOW_BYTE(len) }; */

	/**  struct iovec {                    ## Scatter/gather array items
     *       void  *iov_base;              ## Starting address
     *       size_t iov_len;               ## Number of bytes to transfer
     *    };
	 */
	if (offset < hdrlen) {
		struct iovec iov[2] =
		{
				{ (void*) &hdr[offset], hdrlen-offset },
				{ (void*) buf, buflen }
		};

		struct msghdr msgheader;

		/* zero out */
		memset(&msgheader, 0, sizeof(msgheader));

		msgheader.msg_iov = &(iov[0]);
		msgheader.msg_iovlen = 2;

		/* Note: On Linux sendmsg is not atomic being used on stream-sockets,
		 * so it might be a partial write only.  */
		/* flag MSG_DONTWAIT is redundant, as the socket is nonblocking anyway */
		retval = sendmsg(fd, &msgheader, MSG_DONTWAIT);
	} else {
		const size_t bufoffset = offset - hdrlen;
		struct iovec iov[1] =
		{
				{ (void*) &buf[bufoffset], buflen-bufoffset }
		};

		struct msghdr msgheader;

		/* zero out */
		memset(&msgheader, 0, sizeof(msgheader));

		msgheader.msg_iov = &(iov[0]);
		msgheader.msg_iovlen = 1;

		/* only send if the iovecs fit into send buffer as one single write */
		/* flag MSG_DONTWAIT is redundant, as the socket is nonblocking anyway */
		retval = sendmsg(fd, &msgheader, MSG_DONTWAIT);
	}

	/* Note: on Linux (and other platforms) sendmsg is NOT atomic in combination with TCP.
	 * The linux developers argue that TCP is stream-oriented and atomicity is something the
	 * application has got to take care for. In our case this is not so easy.
	 *
	 * So, in case of overfull send-queue, the "udp-like datagram" might be sent partially.
	 * As our intereset is best-effort transport, we are not interested in storing the rest
	 * and transmit it in next iteration. Instead we use OOB to tell recipient to discard
	 * the partial message.
	 */

	return retval;
}

/**
 *
 */
static int
_rttcp_recv_message(int fd,
		void *hdr,
		size_t hdrlen,
		void *buf,
		size_t buflen,
		int peek)
{
	const int flag = peek ? MSG_PEEK : 0;
	int retval;

	struct iovec iov[2] =
	{
			{ (void*) hdr, hdrlen },
			{ (void*) buf, buflen }
	};

	struct msghdr msgheader;

	/* zero out */
	memset(&msgheader, 0, sizeof(msgheader));

	msgheader.msg_iov = &(iov[0]);
	msgheader.msg_iovlen = 2;

	/* only recv if the iovecs fit into send buffer as one single write */
	/* flag MSG_DONTWAIT is redundant, as the socket is nonblocking anyway */
	retval = recvmsg(fd, &msgheader, flag | MSG_DONTWAIT);

	return retval;
}

/** Partly taken from:
 * http://svn.apache.org/repos/asf/incubator/thrift/trunk/lib/cpp/src/transport/TSocket.cpp
 *
 * Fire and forget. In case of EWOULDBLOCK event, verify queue size and
 * if too large set OOB marker */
ssize_t
_rttcp_send(rttcp_t fd,
		const void *buf,
		size_t buflen,
		struct timeval *usertv,
		rttcp_stat_t *stats)
{
	const char prelude[4] =
		{ MAGIC, VERSION, HIGH_BYTE(buflen), LOW_BYTE(buflen) };
	const size_t preludelen = sizeof(prelude);
	struct timeval zerotv = {0,0};
	struct timeval timeout[2];

	ssize_t result = SOCK_ERROR;
	int retval = SOCK_ERROR;
	char oob[1] = { MARKER }; /* the OOB message may transfer only a single octet as payload */
	int nsent = 0; /* FALSE */
	int iter = 0;

	assert(buf);
	assert(sizeof(oob) == 1);

	/* initialize the timeout values for both iterations */
	timeout[0] = *usertv;
	timeout[1] = zerotv; /* zero timeout for second iteration*/

	if (RTTCP_OOB_FLAG_ISSET(fd)) {
		/* send OOB message */
		// LOG("sending OOB");
		retval = send(fd, &oob[0], sizeof(oob), MSG_DONTWAIT | MSG_OOB);
		if (retval <= 0) {
			/* OOB message failed, return buflen as if a UDP-send has succeeded */
			/* return buflen ; */
			return 0; /* could not write to stream */
		}

		/* OOB succeeded, clear the flags and continue sending users data */
		RTTCP_OOB_FLAG_CLEAR(fd);
	}

	for (iter = 0; iter < 2; ++iter) {
		/* nonblocking send, returning with EWOULDBLOCK in case the sendqueue is full */
		retval = _rttcp_send_message(fd, nsent, &prelude[0], preludelen, buf, buflen);

		nsent += retval>0 ? retval : 0;

		/** If the message is too long to pass atomically  through  the  underlying
		 * protocol, the error EMSGSIZE is returned, and the message is not transmitted.
		 * */
#define RTTCP_SEND_RETRY(_retval,_num) \
	(((_retval) == SOCK_ERROR) && ((_num) == EWOULDBLOCK || (_num) == EMSGSIZE))

#define RTTCP_SEND_PARTIALLY(_retval,_sent,_expected) \
	(((_retval) > 0) && ((_sent) > 0) && ((_sent) < (_expected)))

		if ( RTTCP_SEND_RETRY(retval,errno)
			|| RTTCP_SEND_PARTIALLY(retval,nsent,(preludelen+buflen))) {
			/* the send queue is full. Wait timeout long to resent the missing part otherwise
			 * discard the data in read-queue within peer by sending a OOB message. */
			fd_set wfds;

			FD_ZERO(&wfds);

			FD_SET(fd, &wfds);

			retval = select(fd+1, NULL, &wfds, NULL, &timeout[iter]);

			if (retval == 0 || (iter == 1 && nsent > 0)) {
				/* In both iterations, timeout exceeded but still congestion, or
				 * second iteration (iter==1) and the sent data must be discarded.
				 * So, send OOB message to tell recipient to discard receipt-queue. */
				LOG("\n+ sending OOB");

				RTTCP_OOB_FLAG_SET(fd);

				/* set OOB flag, and unset only after successfull sent now or in next iteration */
				retval = send(fd, &oob[0], sizeof(oob), MSG_DONTWAIT | MSG_OOB);

				if (retval == SOCK_ERROR) {
					/* may happen if sent buffer can not even hold the OOB anymore.
					 * Return buflen as if UDP-send has succeeded. */
					assert(errno == EWOULDBLOCK);
					/* return  buflen; */
					return 0; /* could not write datagram */
				}

				/* OOB message sent successfully  */
				RTTCP_OOB_FLAG_CLEAR(fd);

				/* Note: we sent best effort UDP-like! For better feedback on API level we yield
				 * zero as return value */
				/* return buflen; */
				return 0; /* could not write datagram */
			} else if (retval < 0) {
				/* socket error, user must close the socket */
				LOG("\n+ socket error");

				return SOCK_ERROR;
			}
			/* else iter==0 && retval>0: socket ready to write in second iteration
			 * continue. */

		} else if (retval == SOCK_ERROR || retval == 0) {
			/* socket invalid or connection has been closed (!!TCP)
			 * Callee must invoke rttcp_close(fd) on the socket descriptor.
			 */
			LOG("send failed - socket invalid or has been closed");
			return SOCK_ERROR;
		} else  {
			/* send succeeded, break the for-loop and return the number of bytes written */
			LOG("send succeeded");
			assert(nsent == (preludelen+buflen));
			return buflen;
		}
		/* continue with second iteration */
	} /* for 0..1 */

	assert(!"never reached");

	return  SOCK_ERROR;
}

/** Taken from http://www.gnu.org/s/libc/manual/html_node/Out_002dof_002dBand-Data.html
 */
static int
_rttcp_discard_until_marker(int fd)
{
  while (1)
    {
      /* This is not an arbitrary limit; any size will do.  */
      char buffer[128];
      int atmark, success;

      /* If we have reached the mark, return.  */
      success = ioctl (fd, SIOCATMARK, &atmark);
      if (success < 0) {
    	  LOG("discard failed: ioctl");
      }
      if (atmark) {
    	  LOG("mark found");
    	  return;
      }
      /* Otherwise, read a bunch of ordinary data and discard it.
         This is guaranteed not to read past the mark
         if it starts before the mark.  */
      success = recv (fd, buffer, sizeof(buffer), 0);
      if (success < 0)
    	  LOG ("discard failed: recv");
    }
}

static int
_rttcp_read_marker (int fd)
{
      /* Note: a marker may be of size 1 only */
      int success;
      char buffer[1];

      success = recv(fd, buffer, sizeof(buffer), MSG_OOB );
      if (success == SOCK_ERROR) {
    	  LOG("reading mark failed");
      }
      assert(success == 1 && buffer[0] == MARKER );

      return success;
}

static int
_rttcp_has_marker(int fd)
{
	/* the send queue is full. Wait timeout long and then */
	fd_set efds;
	struct timeval zerotv = {0L, 0L};
	int retval;

	FD_ZERO(&efds);

	FD_SET(fd, &efds);

	retval = select(fd+1, NULL, NULL, &efds, &zerotv);
	if (retval<=0) {
		/* socket error or timeout: no OOB marker present */
		return 0; /* FALSE */
	} else {
		/* OOB marker present */
		return 1; /* TRUE */
	}
}


/** \return -1 on error, 0 if no complete message, otherwise the message-size waiting in receive queue */
static ssize_t
_rttcp_recv_wait(rttcp_t fd,
		void *prelude,
		size_t preludelen,
		void *buf,
		size_t buflen,
		struct timeval *remaintv,
		rttcp_stat_t *stats)
{
#define ITER_MAX (10)
	int iter = 0; /* try at most ITER_MAX times */
	fd_set rfds;
	int retval = 0;
	int error_num;

	if (_rttcp_has_marker(fd)) {
		if (stats) {
			++(stats->oob_count);
			++(stats->discard_count);
		}
		_rttcp_discard_until_marker(fd);
		_rttcp_read_marker(fd);
	}

	FD_ZERO(&rfds);

	FD_SET(fd, &rfds);

	retval = select(fd+1, &rfds, NULL, NULL, remaintv);

	if (retval < 0) {
		return SOCK_ERROR;
	} else if (retval == 0) {
		/* timedout */
		return 0;
	} else {

		do {
			/* we know that data is present in receive queue, so select() would return
			 * immediatly, polling seems more accurate now, otherwise is shall be read by succeeding
			 * invocation from receive queue  */
			memset(buf, 'A', buflen);

			retval = _rttcp_recv_message(fd,
					&prelude[0], preludelen,
					buf, buflen, 1 /* PEEK */ );

			if (retval<0) {
				/* socket error */
				return SOCK_ERROR;
			} else if (retval == 0) {
				/* socket has been closed, mind this is a stream */
				return SOCK_ERROR;
			} else if (retval >= preludelen) {
				unsigned char *header = prelude;
				if ( header[0] != MAGIC || header[1] != VERSION) {
					/* invalid prelude, stream is in undefined state, user must close socket */
					return SOCK_ERROR;
				} else {
					size_t msglen = (header[2] << 8) + header[3];

					if (msglen+preludelen <= retval) {
						return msglen+preludelen;
					}
					/* otherwise continue */
				}
			}
		} while (++iter < ITER_MAX);

		return 0; /* timedout */
	}
#undef ITER_MAX

}

/** */
static ssize_t
_rttcp_recv(rttcp_t fd,
		void *buf,
		size_t buflen,
		struct timeval *usertv,
		rttcp_stat_t *stats)
{
	char prelude[4];
	size_t preludelen = sizeof(prelude);

	ssize_t ret = SOCK_ERROR;
	fd_set rfds;
	int retval;
	int npeeked;

	/* select()  may  update  the timeout argument to indicate how much
	 * time was left.  pselect() does not change this argument. */
	struct timeval remaintv = *usertv;

	if (stats) {
		memset(stats, 0, sizeof(rttcp_stat_t));
	}

	retval = _rttcp_recv_wait (fd, prelude,preludelen, buf, buflen, &remaintv, stats);

	if (retval < 0) {
		/* socket error */
		return SOCK_ERROR;
	} if (retval == 0) {
		/* no message available */
		return 0;
	} else {
		size_t msglen = retval - preludelen;

		/* take the pending data from receive queue */
		retval = _rttcp_recv_message(fd, prelude, preludelen, buf, msglen, 0);
		if (retval < 0) {
			return SOCK_ERROR;
		}
		assert(msglen+preludelen == retval);

		return msglen;
	}
}

/** */
void
rttcp_close(rttcp_t stream)
{
	_rttcp_socket_close((int) stream);
}

/** \return the socket descriptor of underlying TCP connection
 * */
int
rttcp_fd(rttcp_t stream)
{
	return (int) stream;
}

/** */
ssize_t
rttcp_send(rttcp_t dstream,
		const void *buf,
		size_t len,
		struct timeval *tv)
{
	return _rttcp_send(dstream, buf, len, tv, NULL);
}

/** */
ssize_t
rttcp_recv(rttcp_t dstream,
		void *buf,
		size_t len,
		struct timeval *tv)
{
	return _rttcp_recv(dstream, buf, len, tv, NULL);
}

#include "rttcp_debug.h"

/** */
ssize_t
rttcp_debug_send_(rttcp_t dstream,
		const void *buf,
		size_t len,
		struct timeval *tv,
		rttcp_stat_t *stats)
{
	return _rttcp_send(dstream, buf, len, tv, NULL);
}

/** */
ssize_t
rttcp_debug_recv(rttcp_t dstream,
		void *buf,
		size_t len,
		struct timeval *tv,
		rttcp_stat_t *stats)
{
	return _rttcp_recv(dstream, buf, len, tv, NULL);
}
