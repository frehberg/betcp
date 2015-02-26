/*
 * rttcp_debug.h
 *
 *  Created on: Jan 9, 2010
 *      Author: frehberg
 */

#ifndef RTTCP_DEBUG_H_
#define RTTCP_DEBUG_H_

#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "rttcp_stat.h"

#ifdef  __cplusplus
extern "C" {
#endif

/** */
ssize_t
rttcp_debug_send_(rttcp_t dstream,
		const void *buf,
		size_t len,
		struct timeval *tv,
		rttcp_stat_t *stats);

/** */
ssize_t
rttcp_debug_recv(rttcp_t dstream,
		void *buf,
		size_t len,
		struct timeval *tv,
		rttcp_stat_t *stats);

#ifdef  __cplusplus
}
#endif

#endif /* RTTCP_DEBUG_H_ */
