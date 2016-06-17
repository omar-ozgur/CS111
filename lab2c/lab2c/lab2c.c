#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>
#include "SortedList.h"

SortedList_t *lists;
SortedListElement_t *elements;
int *buckets;
int numThreads = 1, numIterations = 1, opt_yield = 0, numLists = 1;
char *randChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
char sync = '\0';

void parseArgs(int argc, char** argv){
	int c;
	while(1){
		static struct option long_options[] =
		{
			{"threads", required_argument, 0, 't'},
			{"iterations", required_argument, 0, 'i'},
			{"yield", required_argument, 0, 'y'},
			{"sync", required_argument, 0, 's'},
			{"lists", required_argument, 0, 'l'}
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "t:i:y:s:l:", long_options, &option_index);
		if(c == -1){
			break;
		}
		int i;
		switch(c){
			// Check if the "threads" option is specified
			case 't':
				numThreads = atoi(optarg);
				if(numThreads < 1){
					fprintf(stderr, "Error: Please enter a valid number of threads\n");
					exit(EXIT_FAILURE);
				}
				break;
			// Check if the "iterations" option is specified
			case 'i':
				numIterations = atoi(optarg);
				if(numIterations < 1){
					fprintf(stderr, "Error: Please enter a valid number of iterations\n");
					exit(EXIT_FAILURE);
				}
				break;
			// Check if the "yield" option is specified
			case 'y':
				for(i = 0; optarg[i] != '\0'; i++){
					if(optarg[i] == 'i'){
						opt_yield |= INSERT_YIELD;
					}
					else if(optarg[i] == 'd'){
						opt_yield |= DELETE_YIELD;
					}
					else if(optarg[i] == 's'){
						opt_yield |= SEARCH_YIELD;
					}
					else{
						fprintf(stderr, "Error: Invalid yield option\n");
						exit(EXIT_FAILURE);
					}
				}
				break;
			// Check if the "sync" option is specified
			case 's':
				if(strlen(optarg) == 1 && optarg[0] == 'm'){
					sync = 'm';
				}
				else if(strlen(optarg) == 1 && optarg[0] == 's'){
					sync = 's';
				}
				else{
					fprintf(stderr, "Error: Please enter a \"sync\" value that is 'm', or 's'\n");
					exit(EXIT_FAILURE);
				}
				break;
			// Check if the "lists" option is specified
			case 'l':
				if(atoi(optarg) > 0){
					numLists = atoi(optarg);
				}
				else{
					fprintf(stderr, "Error: Invalid lists option\n");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}
}

unsigned int hashFunction(const char *key){
	unsigned int hash = 7;
	int i;
	for(i = 0; i < strlen(key); i++){
		hash = hash*101 + key[i];
	}
	return hash;
}

void listInit(){
	// Allocate space for a new list, as well as elements for the list
	lists = malloc(numLists * sizeof(SortedList_t));
	int i;
	for(i = 0; i < numLists; i++){
		lists[i].next = &lists[i];
		lists[i].prev = &lists[i];
		lists[i].key = NULL;
	} 
	buckets = malloc(numThreads * numIterations * sizeof(int));
	elements = malloc(numThreads * numIterations * sizeof(SortedListElement_t));
	srand(time(NULL));
	for(i = 0; i < numThreads * numIterations; i++){
		// Generate random keys
		int keyLen = rand() % 20 + 5;
		char *key = malloc((keyLen + 1) * sizeof(char));
		int j;
		for(j = 0; j < keyLen; j++){
			key[j] = randChars[rand() % strlen(randChars)];
		}
		key[j] = '\0';
		// Set the corresponding element's key
		elements[i].key = key;
		buckets[i] = hashFunction(key) % numLists;
	}
}

pthread_mutex_t *mutexes;
int *locks;

void *threadFunction(void *arg){
	int tid = *(int *)arg;
	int i;
	// Insert elements into the list
	for(i = tid; i < numIterations * numThreads; i += numThreads){
		if(sync == 'm'){
			pthread_mutex_lock(&mutexes[buckets[i]]);
			SortedList_insert(&lists[buckets[i]], &elements[i]);
			pthread_mutex_unlock(&mutexes[buckets[i]]);
		}
		else if(sync == 's'){
			while(__sync_lock_test_and_set(&locks[buckets[i]], 1) == 1);
			SortedList_insert(&lists[buckets[i]], &elements[i]);
			__sync_lock_release(&locks[buckets[i]]);
		}
		else{
			SortedList_insert(&lists[buckets[i]], &elements[i]);
		}
	}
	// Get the length of the list
	int length = 0;
	if(sync == 'm'){
		for(i = 0; i < numLists; i++){
			pthread_mutex_lock(&mutexes[i]);
			length += SortedList_length(&lists[i]);
			pthread_mutex_unlock(&mutexes[i]);
		}
	}
	else if(sync == 's'){
		for(i = 0; i < numLists; i++){
			while(__sync_lock_test_and_set(&locks[i], 1) == 1);
			length += SortedList_length(&lists[i]);
			__sync_lock_release(&locks[i]);
		}
	}
	else{
		for(i = 0; i < numLists; i++){
			length += SortedList_length(&lists[i]);
		}
	}
	// Find and delete elements from the list
	for(i = tid; i < numIterations * numThreads; i += numThreads){
		if(sync == 'm'){
			pthread_mutex_lock(&mutexes[buckets[i]]);
			SortedListElement_t *found = SortedList_lookup(&lists[buckets[i]], elements[i].key);
			SortedList_delete(found);
			pthread_mutex_unlock(&mutexes[buckets[i]]);
		}
		else if(sync == 's'){
			while(__sync_lock_test_and_set(&locks[buckets[i]], 1) == 1);
			SortedListElement_t *found = SortedList_lookup(&lists[buckets[i]], elements[i].key);
			SortedList_delete(found);
			__sync_lock_release(&locks[buckets[i]]);
		}
		else{
			SortedListElement_t *found = SortedList_lookup(&lists[buckets[i]], elements[i].key);
			SortedList_delete(found);
		}
	}
	return NULL;
}

void runThreads(){
	struct timespec startTime, endTime;
	int i;
	// Initialize mutex if applicable
	if(sync == 'm'){
		mutexes = malloc(numLists * sizeof(pthread_mutex_t));	
		for(i = 0; i < numLists; i++){
			pthread_mutex_init(&mutexes[i], NULL);
		}
	}
	else if(sync == 's'){
		locks = calloc(numLists, sizeof(int));
	}
	// Create threads
	pthread_t *threads = malloc(numThreads * sizeof(pthread_t));
	int *tid = malloc(numThreads * sizeof(int));
	// Note the starting clock time
	int status = clock_gettime(CLOCK_MONOTONIC, &startTime);
	if(status == -1){
		perror("Clock Error");
		exit(EXIT_FAILURE);
	}
	for(i = 0; i < numThreads; i++){
		tid[i] = i;
		status = pthread_create(&threads[i], NULL, threadFunction, &tid[i]);
		if(status){
			fprintf(stderr, "Error: Could not create thread\n");
			exit(EXIT_FAILURE);
		}
	}
	// Join threads
	for(i = 0; i < numThreads; i++){
		status = pthread_join(threads[i], NULL);
		if(status){
			fprintf(stderr, "Error: Could not join thread\n");
			exit(EXIT_FAILURE);
		}
	}
	// Note the ending clock time
	status = clock_gettime(CLOCK_MONOTONIC, &endTime);
	if(status == -1){
		perror("Clock Error");
		exit(EXIT_FAILURE);
	}
	// Destroy mutex if applicable
	if(sync == 'm'){
		for(i = 0; i < numLists; i++){
			status = pthread_mutex_destroy(&mutexes[i]);
			if(status){
				fprintf(stderr, "Error: Could not destroy mutex\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	// Print summary
	int finalLength = 0;
	for(i = 0; i < numLists; i++){
		finalLength += SortedList_length(&lists[i]);
	}
	int numOps = numThreads * numIterations * 2;
	printf("%d threads x %d iterations x (insert + lookup/delete) = %d operations\n", numThreads, numIterations, numOps);
	long long elapsedTime = (endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
	if(finalLength != 0){
		fprintf(stderr, "ERROR: List length = %d\n", finalLength);
	}
	printf("elapsed time: %lldns\n", elapsedTime);
	long long avgTime = elapsedTime / numOps;
	printf("per operation: %lldns\n", avgTime);
	if(finalLength != 0){
		exit(1);
	}
	exit(0);
}

int main(int argc, char** argv){
	parseArgs(argc, argv);
	listInit();
	runThreads();
	exit(EXIT_SUCCESS);
}
