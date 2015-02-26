/*
 * rttcp.h
 *
 *  Created on: Jan 6, 2010
 *      Author: frehberg
 */

#ifndef RTTCP_H_
#define RTTCP_H_

#include <sys/time.h>
#include <netinet/in.h>

#include "rttcp_types.h"

#ifdef  __cplusplus
extern "C" {
#endif

/** */
rttcp_t
rttcp_server(const in_addr_t addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv);

rttcp_t
rttcp_server6(const struct in6_addr *addr,
		unsigned short port,
		unsigned int bufsize,
		struct timeval *tv);

/** */
rttcp_t
rttcp_accept(rttcp_t fd,
		struct timeval *tv);

/** */
rttcp_t
rttcp_client(const in_addr_t addr,
		unsigned short port,
		unsigned bufsize,
		struct timeval *tv);

/** */
rttcp_t
rttcp_client6(const struct in6_addr *addr,
		unsigned short port,
		unsigned bufsize,
		struct timeval *tv);

/** */
ssize_t
rttcp_send(rttcp_t dstream,
		const void *buf,
		size_t len,
		struct timeval *tv);

/** */
ssize_t
rttcp_recv(rttcp_t dstream,
		void *buf,
		size_t len,
		struct timeval *tv);

/** */
void
rttcp_close(rttcp_t stream);

/** \return the socket descriptor of underlying TCP connection */
int
rttcp_fd(rttcp_t stream);

#ifdef  __cplusplus
}
#endif


#endif /* BETCP_H_ */
