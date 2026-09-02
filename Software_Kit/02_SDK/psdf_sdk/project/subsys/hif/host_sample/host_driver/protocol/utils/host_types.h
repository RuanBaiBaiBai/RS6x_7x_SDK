/**
 **************************************************************************************************
 * @file    host_types.h
 * @brief   host error types file.
 * @attention
 *          Copyright (c) 2025 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_TYPES_H__
#define __HOST_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_std_includes.h"

/* slist formats */
#include "host_slist.h"


#ifndef true
#define true            1
#define false           0
#endif

/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_ERRCODE_SUCCESS                  (0)  /* 0 should not change */
#define HOST_ERRCODE_NOT_READY                (-1)
#define HOST_ERRCODE_INVALID_PARAM            (-2)
#define HOST_ERRCODE_NO_BUFFER                (-3)
#define HOST_ERRCODE_IO_ERROR                 (-4)
#define HOST_ERRCODE_TIMEOUT                  (-5)
#define HOST_ERRCODE_ABORTED                  (-6)
#define HOST_ERRCODE_UNSUPPORT                (-7)
#define HOST_ERRCODE_BUSY                     (-8)
#define HOST_ERRCODE_STATE                    (-9)
#define HOST_ERRCODE_SYSERR                   (-10)
#define HOST_ERRCODE_NEED_RETRY               (-11)
#define HOST_ERRCODE_NOT_FOUND                (-12)
#define HOST_ERRCODE_INVALID_HANDLE           (-13)
#define HOST_ERRCODE_NOT_ALLOW                (-14)

typedef enum {
    HOST_STATE_DISABLE      = 0,
    HOST_STATE_ENABLE       = 1,
} host_state_t;

/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef BIT
#define BIT(n)                       (1U<<(n))
#endif

#ifndef ABS
#define ABS(v)                       ((v)<0 ? -(v) : (v))
#endif

#ifndef ABS_INT32
#define ABS_INT32(v)                 ((((v)>>31)^(v)) + (((v)>>31) & 0x1))
#endif

#ifndef ABS_INT16
#define ABS_INT16(v)                 ((((v)>>16)^(v)) + (((v)>>16) & 0x1))
#endif

#ifndef ABS_INT8
#define ABS_INT8(v)                  ((((v)>>8)^(v)) + (((v)>>8) & 0x1))
#endif

#ifndef IS_POW2
#define IS_POW2(n) 			         (!((n) & ((n) - 1)))
#endif

#ifndef ROUND_UP4
#define ROUND_UP4(v)		         (((v) + 3) & ~0x3)
#endif

#ifndef ROUND_DOWN4
#define ROUND_DOWN4(v)		         ((v) & ~0x3)
#endif

#ifndef ROUND_Qn
#define ROUND_Qn(v, nbit)            (((v) + (1<<(nbit-1))) & (~(1<<(nbit))))
#endif

#ifndef ROUND_INT_Qn
#define ROUND_INT_Qn(v, nbit)        (((v) + (1<<(nbit-1)))>>(nbit))
#endif

#ifndef MIN
#define MIN(x,y)                     ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y)                     ((x) > (y) ? (x) : (y))
#endif

//#ifndef ARRAY_SIZE
//#define ARRAY_SIZE(arr)              (sizeof(arr) / sizeof((arr)[0]))
//#endif

#ifndef contain_off
#define contain_off(t, m)            ((uint32_t)(&((t *)0)->m))
#endif

#ifndef contain_mb
#define contain_mb(t, m)             ((uint32_t)((&((t *)0)->m) + 1))
#endif

#define wrap32_before(a, b)          ((int)((a) - (b)) < 0)
#define wrap32_after(a, b)            wrap32_before(b, a)
#define wrap32_diff(a, b)            ((int)((a) - (b)))

#ifdef __cplusplus
}
#endif

#endif /* __HOST_TYPES_H__ */

