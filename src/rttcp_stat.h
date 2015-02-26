/*
 * rttcp_stat.h
 *
 *  Created on: Jan 9, 2010
 *      Author: frehberg
 */

#ifndef RTTCP_STAT_H_
#define RTTCP_STAT_H_

#include <sys/time.h>
#include "rttcp_types.h"

#ifdef  __cplusplus
extern "C" {
#endif

typedef struct {
	int oob_count;
	int discard_count;
	struct timeval elapsed_time;
} rttcp_stat_t;

#ifdef  __cplusplus
}
#endif

#endif /* RTTCP_STAT_H_ */
