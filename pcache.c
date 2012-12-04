/*
 * pcache.c
 *
 * Author: Bin Liu (bliu) & Lei Yan (leiyan)
 * implementation file for cache module
 * 
 * General Design Explanation:
 * 	- The cache is organized in a double-linked list
 *  - The head and tail pointers are global variables
 *  - remain_cache_size is used to keep track of the size of our cache
 *  - When adding a new entry, simply create a cacheline and
 		insert it to head, and also decrement remain_cache_size
 *	- When accessing an entry, move that entry to head
 *	- So that the sequence of the linked list represent a LRU pattern
 */

/* $begin pcache.c */
#include "pcache.h"

/* Macro printf for debugging*/
#define DEBUGx
#ifdef DEBUG
# define dbg_printf(...) printf(__VA_ARGS__)
#else
# define dbg_printf(...)
#endif

// Global variables
static linePCache *cache_head = NULL;
static linePCache *cache_tail = NULL;
static unsigned remain_cache_size = MAX_CACHE_SIZE;

/* Exposed interfaces */

/*
 * get_webobj_from
 *
 * provided interface for the proxy to find/retrieve data                      
 * Using the uri of the request
 * 
 * INPUT:
 * 	- uri_in: pointer to the uri, key for the search
 *
 * Returns pointer to the cache line if found
 * Returns NULL if not found
 */
linePCache* get_webobj_from(char *uri_in) {
	
	linePCache *current_line;
	current_line = cache_head;

	//Traverse the cache from the head
	while (current_line != NULL) {
		if (strcmp(current_line->uri_key, uri_in) == 0)
		{
			//If found the matching key (uri)
			//Move the founded line to the head
			put_line_to_head(current_line);
			//Return the founded line
			return current_line;
		}
		current_line = current_line->next_line;
	}

	//Not found, so return NULL
	return NULL;
}

/*
 * update_cache
 *
 * provided interface to update cache to achieve LRU
 * 
 * NOTE: Not used in current scheme, kept it for portability
 */
void update_cache(linePCache* visited_line){
	put_line_to_head(visited_line);
}


/*
 * get_webobj_from
 *
 * provided interface for the proxy to write new contents                      
 * into the cache
 * 
 * INPUT:
 * 	- uri_in: pointer to the uri, used as key for the new line
 * 	- webobj_in: pointer to the content buffer
 *				used to copy content to cache
 *  - obj_length_in: integer, the size of the content
 *
 * Returns pointer to the newly created cache line
 *
 */
linePCache* set_webobj_to(char *uri_in, char *webobj_in, int obj_length_in) {

	linePCache *new_line;
	int obj_size = obj_length_in;
	
	//Create and initialize a new line
	new_line = Malloc(sizeof(linePCache));
	new_line->obj_length = obj_size;
	new_line->prev_line = NULL;
	new_line->next_line = NULL;

	//Copy the uri into our cacheline as key
	new_line->uri_key = Malloc(MAXLINE);
	strcpy(new_line->uri_key, uri_in);

	//Copy the content into our buffer
	new_line->webobj_buf = Malloc(obj_size);
	memcpy(new_line->webobj_buf, webobj_in, obj_size);

	if (remain_cache_size < obj_size)
	{
		//Not enough room, need to perform eviction
		evict_lines_for_size(obj_size);
	}
	
	remain_cache_size -= obj_size;
	add_new_line(new_line);

	return new_line;
}

/*
 * free_cache
 *
 * provided interface to clean the entire cache
 */
void free_cache(){
	linePCache *current_line = cache_head;

	while (current_line != NULL)
	{
		cache_head = current_line->next_line;
		free_line(current_line);
		current_line = cache_head;
	}
}

/* Internal helpers*/
void put_line_to_head(linePCache *new_head) {
	dbg_printf("Updated Cache for LRU");
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


/*
 * evict_lines_for_size
 *
 * [Internal Helper]
 * Evict the lease recently used cachelines 
 * Until we have enough space to accommandate the new line
 *
 * INPUT:
 * 	- needed_size: integer, required buffer size for the new line
 */
void evict_lines_for_size(int needed_size)
{
	linePCache *current_line;
	int tmp_size;

	//Traverse our cache from tail(Least Recent Used ones)
	//Search for a line that's big enough until reached the head
	current_line = cache_tail;

	while (current_line != NULL)
	{
		tmp_size = current_line->obj_length + remain_cache_size;
		if (tmp_size > needed_size)
		{
			remove_line(current_line);
			free_line(current_line);
			remain_cache_size = tmp_size;
			return;
		}
		else current_line = current_line->prev_line;
	}

	//Reached the head
	//Means no line is big enough
	//So we evict cachelines from the tail one
	//by one until we have enough space
	current_line = cache_tail;

	while ((remain_cache_size < needed_size) 
		&& (current_line != NULL))
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

/*
 * add_new_line
 *
 * [Internal Helper]
 * Add a new cacheline to the head of the cache
 * 
 * INPUT:
 * 	- new_line: pointer to the newline
 */
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

/*
 * remove_line
 *
 * [Internal Helper]
 * Remove a cacheline from the linked list
 *  - Connects the previous and the next line
 *  - isolate the current line
 * 
 * INPUT:
 * 	- line: pointer to the line that need to be isolated
 */
void remove_line(linePCache *line)
{
	linePCache *old_prev_line = line->prev_line;
	linePCache *old_next_line = line->next_line;

	if (old_prev_line == NULL)
	{
		// The line is the head
		// So we move the head to the next line
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

/*
 * free_line
 *
 * [Internal Helper]
 * free the memory of a line
 * 	- including the content buffer and they uri key buffer
 * 
 * INPUT:
 * 	- line: pointer to the line to be freed
 */
void free_line(linePCache *line)
{
	Free(line->uri_key);
	Free(line->webobj_buf);
	Free(line);
}

/* Internal Test cases */
// Please skip this part :-)
int test_cache() 
{	
	char *uri_arr[6] = {"abcabc","aaaaaa","abcabc","eeeeee","dddddd","abcabc"};
	char *webobj_arr[6] = {"111111","123123","123123","222222","123123","123"};

	int n = 6;
	int i;
	linePCache *obj_from_cache;

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
