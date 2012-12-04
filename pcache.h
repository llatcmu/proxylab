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

#define MAX_CACHE_SIZE  1049000
#define MAX_OBJECT_SIZE 102400

#define FOUND 1
#define NOT_FOUND NULL

typedef struct pcacheLine{
	struct pcacheLine *prev_line;
	struct pcacheLine *next_line;
    char *uri_key ;		/* use uri as key of the cache */
    char *webobj_buf;   /* pointer to the cached obj */
    int obj_length;
} linePCache;


/* Exposed interfaces */
linePCache* get_webobj_from(char *uri_in);
void update_cache(linePCache* visited_line);
linePCache* set_webobj_to(char *uri_in, char *webobj_in, int obj_length_in);
void free_cache();

/* Internal helpers*/
 
int test_cache();
void add_new_line(linePCache *new_line);
void put_line_to_head(linePCache *new_head);

void evict_lines_for_size(int needed_size);

/* Line Operations */
void remove_line(linePCache *line);
void free_line(linePCache *line);
#endif /* __PCACHE_H__ */
/* $end pcache.h */
