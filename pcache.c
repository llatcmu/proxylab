/*
 * pcache.c
 *
 * Author: Bin Liu (bliu) & Lei Yan (leiyan)
 * implementation file for cache module
 */

/* $begin pcache.c */
#include "pcache.h"

void init_cache(){
	centralCache = Malloc(sizeof(linePCache *) * number_of_sets);
}

/* Exposed interfaces */
int is_cached(char *uri_in){
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
        	return NOT_FOUND;
        }
    }
}

char* get_webobj_from(char *uri, char *buf1){
	if (centralCache == NULL)
	{
		init_cache();
		return 0;
	}

	timeline ++;
	for (i = 0; i < number_of_sets; i++) {
       //Iterate through all the lines
		if(centralCache[i].is_valid){
			if (strcmp(centralCache[i].uri_key, uri_in) == 0) {
				//Found the matching uri
				centralCache[i].timestamp = timeline;
				buf1 = centralCache[i].webobj_buf;
				return centralCache[i].webobj_buf;
            }
        }
        else {
        	//When we encounter an invalid line
        	//We've iterated through all cached lines
        	has_empty_line = 1;
            empty_line_number = i;
            buf1 = NULL;

        	return NOT_FOUND;
        }
    }
}

char* get_buf_for_webobj(char *uri_in){

	int smallest_timestamp;
	int ret_line_num = 0;

	if (centralCache == NULL)
	{
		init_cache();
	}

	timeline ++;
	smallest_timestamp = centralCache[0].timestamp;


	if (has_empty_line)
	{
		/* Initializing a cache line */
		centralCache[empty_line_number].is_valid = 1;
		centralCache[empty_line_number].timestamp = timeline;
		centralCache[empty_line_number].uri_key = Malloc(MAXLINE);
		strcpy(centralCache[empty_line_number].uri_key, uri_in);

		centralCache[empty_line_number].webobj_buf = Malloc(MAX_OBJECT_SIZE);

		return centralCache[empty_line_number].webobj_buf;
	}
	else {
		for (i = 0; i < number_of_sets; i++) {
       		//Iterate through all the lines
			if(centralCache[i].is_valid){
				if (centralCache[i].timestamp < smallest_timestamp) {
					//Found the matching uri
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

   		centralCache[ret_line_num].timestamp = timeline;
		centralCache[ret_line_num].uri_key = Malloc(MAXLINE);
		strcpy(centralCache[ret_line_num].uri_key, uri_in);

		centralCache[ret_line_num].webobj_buf = Malloc(MAX_OBJECT_SIZE);

		return centralCache[ret_line_num].webobj_buf;
	}

	return NULL;
}

/* Internal helpers*/
 
/* $end pcache.c */