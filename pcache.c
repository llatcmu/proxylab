/*
 * pcache.c
 *
 * Author: Bin Liu (bliu) & Lei Yan (leiyan)
 * implementation file for cache module
 */

/* $begin pcache.c */
#include "pcache.h"

/* If you want debugging output, use the following macro.  When you hand
 * in, remove the #define DEBUG line. */
#define DEBUG
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

static linePCache *cache_head = NULL;
static linePCache *cache_tail = NULL;
static unsigned remain_cache_size = MAX_CACHE_SIZE;

void init_cache(){
	cache_head = NULL;
	cache_tail = NULL;
}

/* Exposed interfaces */

linePCache* get_webobj_from(char *uri_in) {

	linePCache *current_line;
	
	dbg_printf("[in get_webobj_from]**************\n");
	
	current_line = cache_head;

	while (current_line != NULL) {
		if (strcmp(current_line->uri_key, uri_in) == 0)
		{
			dbg_printf("[in get_webobj_from] Found in cache\n");
			put_line_to_head(current_line);
			return current_line;
		}
		current_line = current_line->next_line;
	}

	dbg_printf("[out get_webobj_from] NOT_FOUND \n");
	return NULL;
}

linePCache* set_webobj_to(char *uri_in, char *webobj_in, int obj_length_in) {

	linePCache *new_line;

	dbg_printf("[in set_webobj_to]uri: %s, webobj: %s\n", uri_in, webobj_in);
	
	//Create a new line
	new_line = Malloc(sizeof(linePCache));
	new_line->obj_length = obj_length_in;
	new_line->prev_line = NULL;
	new_line->next_line = NULL;

	new_line->uri_key = Malloc(strlen(uri_in));
	strcpy(new_line->uri_key, uri_in);
	
	new_line->webobj_buf = webobj_in;

	if (remain_cache_size < obj_length_in)
	{
		//Not enough room, eviction
		evict_lines_for_size(obj_length_in);
	}
	
	remain_cache_size -= obj_length_in;
	add_new_line(new_line);

	return new_line;
}

/* Internal helpers*/
 
void put_line_to_head(linePCache *new_head) {
	// Remove line from list
	linePCache *old_prev_line = new_head->prev_line;
	linePCache *old_next_line = new_head->next_line;

	if (old_prev_line == NULL)
	{
		// The line is already the head
		// So we don't need to do anything
		return;
	}
	else 
	{
		old_prev_line->next_line = old_next_line;
	}

	if (old_next_line == NULL)
	{
		/* The line is the tail */
		// renew the tail pointer
		cache_tail = old_prev_line;
	}
	else
	{
		old_next_line->prev_line = old_prev_line;
	}

	// Insert the new head
	new_head->next_line = cache_head;
	new_head->prev_line = NULL;
	cache_head->prev_line = new_head;
	cache_head = new_head;

	return;
}

void add_new_line(linePCache *new_line)
{
	if (cache_head == NULL)
	{
		cache_head = new_line;
		if (cache_tail == NULL)
		{
			cache_tail = new_line;
		}
	}
	else
	{
		new_line->next_line = cache_head;
		cache_head->prev_line = new_line;
		cache_head = new_line;
	}

	return;
}


void evict_lines_for_size(int needed_size)
{
	linePCache *current_line;
	int tmp_size;

	current_line = cache_tail;

	while (current_line != NULL)
	{
		tmp_size = current_line->obj_length + remain_cache_size;
		if (tmp_size > needed_size)
		{
			remove_line(current_line);
			free_line(current_line);
			return;
		}
		else current_line = current_line->prev_line;
	}

	//Means no line is big enough
	//So we evict cachelines from the tail one
	//by one until we have enough space
	current_line = cache_tail;

	while (remain_cache_size < needed_size)
	{
		remain_cache_size += current_line->obj_length;
		remove_line(current_line);
		free_line(current_line);
		current_line = cache_tail;
	}

	//Now the cache would have enough space
	return;
}

/* Line Operations */
void remove_line(linePCache *line)
{
	// Remove line from list
	linePCache *old_prev_line = line->prev_line;
	linePCache *old_next_line = line->next_line;

	if (old_prev_line == NULL)
	{
		// The line is the head
		// So we move the head
		cache_head = old_next_line;
	}
	else 
	{
		old_prev_line->next_line = old_next_line;
	}

	if (old_next_line == NULL)
	{
		/* The line is the tail */
		// renew the tail pointer
		cache_tail = old_prev_line;
	}
	else
	{
		old_next_line->prev_line = old_prev_line;
	}
}

void free_line(linePCache *line)
{
	Free(line->uri_key);
	Free(line->webobj_buf);
	Free(line);
}

/* Internal Test cases */
int test_cache() 
{	
	char *uri_arr[6] = {"abcabc","aaaaaa","abcabc","eeeeee","dddddd","abcabc"};
	char *webobj_arr[6] = {"111111","123123","123123","222222","123123","123123"};

	int n = 6;
	int i;
	linePCache *obj_from_cache;

	init_cache();
	for (i = 0; i < n; ++i)
	{
		obj_from_cache = get_webobj_from(uri_arr[i]);
		if (obj_from_cache == NULL)
		{
			//NOT Found
			set_webobj_to(uri_arr[i],webobj_arr[i],6);
		}
		else{
			dbg_printf("Found: %s\n", obj_from_cache->webobj_buf);
		}
	}

	return 1;

}




/* $end pcache.c */