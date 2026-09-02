/**
 **************************************************************************************************
 * @file    host_slist.h
 * @brief   host slist define.
 * @attention
 *          Copyright (c) 2026 Possumic Technology. all rights reserved.
 **************************************************************************************************
 */

/* Define to prevent recursive inclusion.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef __HOST_SLIST_H__
#define __HOST_SLIST_H__

#ifdef __cplusplus
extern "C" {
#endif


/* Includes.
 * ------------------------------------------------------------------------------------------------
 */
#include "host_std_includes.h"


/* Exported constants.
 * ------------------------------------------------------------------------------------------------
 */
/* Exported macros.
 * ------------------------------------------------------------------------------------------------
 */
#define HOST_SLIST_FOREACH(slist, item, type) \
    for ((item) = (type *)(((host_slist_t *)slist)->next); (item) != NULL; (item) = (type *)(((host_slist_t *)item)->next))


/* Exported types.
 * ------------------------------------------------------------------------------------------------
 */
struct host_slist_s {
    struct host_slist_s *next;
};
typedef struct host_slist_s host_slist_t;


/* Exported functions.
 * ------------------------------------------------------------------------------------------------
 */
static inline void host_slist_init(host_slist_t *slist)
{
    if (slist != NULL) {
        slist->next = NULL;
    }
}

static inline host_slist_t *host_slist_get_first(host_slist_t *slist)
{
    if (slist != NULL) {
        return slist->next;
    }
    return NULL;
}

static inline host_slist_t *host_slist_get_last(host_slist_t *slist)
{
    host_slist_t *item = NULL;
    if (slist != NULL) {
        HOST_SLIST_FOREACH(slist, item, host_slist_t) {
            if (item->next == NULL) {
                return item;
            }
        }
    }
    return NULL;
}

static inline void host_slist_append(host_slist_t *slist, host_slist_t *item, bool insert_head)
{
    host_slist_t *p = NULL;
    if (slist == NULL || item == NULL) {
        return;
    }
    if (insert_head) {
        item->next = slist->next;
        slist->next = item;
    } else {
        item->next = NULL;
        if (slist->next != NULL) {
            HOST_SLIST_FOREACH(slist, p, host_slist_t) {
                if (p->next == NULL) {
                    p->next = item;
                    return;
                }
            }
        } else {
            slist->next = item;
        }
    }
}

static inline void host_slist_remove(host_slist_t *slist, host_slist_t *item)
{
    host_slist_t *p = NULL;
    if (slist == NULL || item == NULL) {
        return;
    }
    if (slist->next == item) {
        slist->next = item->next;
        return;
    }
    HOST_SLIST_FOREACH(slist, p, host_slist_t) {
        if (p->next == item) {
            p->next = item->next;
            return;
        }
    }
}


#ifdef __cplusplus
}
#endif

#endif /* __HOST_SLIST_H__ */

