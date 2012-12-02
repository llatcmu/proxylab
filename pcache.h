/*
 * pcache.h
 *
 * Author: Bin Liu (bliu) & Lei Yan (leiyan)
 * header file for cache module
 */

/* $begin pcache.h */
#ifndef __PCACHE_H__
#define __PCACHE_H__

#include <stdio.h>
#include "string.h"
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define FOUND 1
#define NOT_FOUND NULL

typedef struct {
	int is_valid;
	int timestamp;
    char *uri_key ;		/* use uri as key of the cache */
    char *webobj_buf;   /* pointer to the cached obj */
} linePCache;

static linePCache *centralCache;

int number_of_sets = MAX_CACHE_SIZE / MAX_OBJECT_SIZE;
static int has_empty_line;
static int empty_line_number;
static unsigned int timeline;

void init_cache();

/* Exposed interfaces */
int is_cached(char *uri_in);
char* get_webobj_from(char *uri_in, char *buf1);
char* get_buf_for_webobj(char *uri_in);

/* Internal helpers*/
 

#endif /* __PCACHE_H__ */
/* $end pcache.h */
