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

static linePCache *centralCache;

int number_of_sets = MAX_CACHE_SIZE / MAX_OBJECT_SIZE;
static int has_empty_line;
static int empty_line_number;
static unsigned int timeline;

void init_cache(){
	centralCache = Malloc(sizeof(linePCache *) * number_of_sets);
}

/* Exposed interfaces */
int is_cached(char *uri_in){

	int i;

	if (centralCache == NULL)
	{
		init_cache();
		return 0;
	}

	for (i = 0; i < number_of_sets; i++) {
       //Iterate through all the lines
		if(centralCache[i].is_valid){
			if (strcmp(centralCache[i].uri_key, uri_in) == 0) {
				//Found the matching uri
				return FOUND;
            }
        }
        else {
        	//When we encounter an invalid line
        	//We've iterated through all cached lines
        	return 0;
        }
    }

    return 0;
}

int get_webobj_from(char *uri_in, char *cached_obj_out) {

	int i;

	dbg_printf("[in get_webobj_from]**************\n");
	if (centralCache == NULL)
	{
		init_cache();
		cached_obj_out = NULL;
		return 0;
	}

	timeline ++;
	for (i = 0; i < number_of_sets; i++) {
       //Iterate through all the lines
		if(centralCache[i].is_valid){
			if (strcmp(centralCache[i].uri_key, uri_in) == 0) {
				//Found the matching uri
				centralCache[i].timestamp = timeline;
				cached_obj_out = centralCache[i].webobj_buf;

				dbg_printf("********Found in cache!**********\n");
				return centralCache[i].obj_length;
            }
        }
        else {
        	//When we encounter an invalid line
        	//We've iterated through all cached lines
        	has_empty_line = 1;
            empty_line_number = i;
            cached_obj_out = NULL;
            dbg_printf("********Still have empty lines!!**********\n");

        	return 0;
        }
    }
    dbg_printf("[out get_webobj_from]*********NOT_FOUND******\n");
    cached_obj_out = NULL;
    return 0;
}

int set_webobj_to(char *uri_in, char *webobj_in, int obj_length_in) {

	int smallest_timestamp;
	int ret_line_num = 0;
	int i;

	dbg_printf("[in get_buf_for_webobj]*************\n");
	if (centralCache == NULL)
	{
		init_cache();
	}

	timeline ++;
	smallest_timestamp = timeline;


	if (has_empty_line)
	{
		/* Initializing a cache line */
		centralCache[empty_line_number].is_valid = 1;
		centralCache[empty_line_number].timestamp = timeline;
		centralCache[empty_line_number].obj_length = obj_length_in;
		centralCache[empty_line_number].uri_key = Malloc(MAXLINE);
		strcpy(centralCache[empty_line_number].uri_key, uri_in);

		centralCache[empty_line_number].webobj_buf = webobj_in;

		dbg_printf("[in get_buf_for_webobj]*******insert into cache******\n");
		has_empty_line = 0;
		empty_line_number = 0;
		return 1;
	}
	else {
		for (i = 0; i < number_of_sets; i++) {
       		//Iterate through all the lines
			if(centralCache[i].is_valid){
				if (centralCache[i].timestamp < smallest_timestamp) {
					//Found smaller timestamp
					smallest_timestamp = centralCache[i].timestamp;
					ret_line_num = i;
            	}
        	}
        	else {
        		//When we encounter an invalid line
        		//We've iterated through all cached lines
        		has_empty_line = 1;
            	empty_line_number = i;
        	}
    	}

    	dbg_printf("[in get_buf_for_webobj]*******start replace******\n");
   		centralCache[ret_line_num].timestamp = timeline;
   		centralCache[ret_line_num].obj_length = obj_length_in;
		strcpy(centralCache[ret_line_num].uri_key, uri_in);

		centralCache[ret_line_num].webobj_buf = webobj_in;
		dbg_printf("[in get_buf_for_webobj]*******replaced cacheline******\n");
		return 1;
	}

	dbg_printf("[in get_buf_for_webobj]*************\n");
	return 0;
}

/* Internal helpers*/
 
/* $end pcache.c */