#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "SortedList.h"

void SortedList_insert(SortedList_t *list, SortedListElement_t *element){
	if(list == NULL || element == NULL){
		fprintf(stderr, "Error: List and/or element cannot be null\n");
		exit(EXIT_FAILURE);
	}
	SortedListElement_t *current = list;
	while(current->next != list){
		if(strcmp(element->key, current->next->key) <= 0){
			break;
		}
		current = current->next;
	}
	if (opt_yield & INSERT_YIELD)
		pthread_yield();
	current->next->prev = element;
	element->next = current->next;
	current->next = element;
	element->prev = current;
}

int SortedList_delete(SortedListElement_t *element){
	if(element != NULL && element->next->prev == element && element->prev->next == element){
		if(opt_yield & DELETE_YIELD)
			pthread_yield();
		element->prev->next = element->next;
		element->next->prev = element->prev;
		return 0;
	}
	return 1;
}

SortedListElement_t *SortedList_lookup(SortedList_t *list, const char *key){
	if(list == NULL){
		return NULL;
	}
	SortedListElement_t *current = list;
	do{
		if(current->key != NULL && !strcmp(current->key, key)){
			return current;
		}
		if(opt_yield & SEARCH_YIELD)
			pthread_yield();
		current = current->next;
	} while(current != list);
	return NULL;
}

int SortedList_length(SortedList_t *list){
	if(list == NULL){
		return -1;
	}
	int len = 0;
	SortedListElement_t *current = list;
	while(current->next != list){
		len++;
		if(opt_yield & SEARCH_YIELD)
			pthread_yield();
		current = current->next;
	}
	return len;
}
