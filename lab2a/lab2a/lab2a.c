#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>

int numThreads = 1, numIterations = 1, opt_yield = 0;
char sync = '\0';

void parseArgs(int argc, char** argv){
	int c;
	while(1){
		static struct option long_options[] =
		{
			{"threads", required_argument, 0, 't'},
			{"iterations", required_argument, 0, 'i'},
			{"yield", no_argument, 0, 'y'},
			{"sync", required_argument, 0, 's'},
		};
		int option_index = 0;
		c = getopt_long(argc, argv, "t:i:ys:", long_options, &option_index);
		if(c == -1){
			break;
		}
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
				opt_yield = 1;
				break;
			// Check if the "sync" option is specified
			case 's':
				if(strlen(optarg) == 1 && optarg[0] == 'm'){
					sync = 'm';
				}
				else if(strlen(optarg) == 1 && optarg[0] == 's'){
					sync = 's';
				}
				else if(strlen(optarg) == 1 && optarg[0] == 'c'){
					sync = 'c';
				}
				else{
					fprintf(stderr, "Error: Please enter a \"sync\" value that is 'm', 's', or 'c'\n");
					exit(EXIT_FAILURE);
				}
				break;
			default:
				exit(EXIT_FAILURE);
		}
	}
}

void compareAdd(long long *pointer, long long value) {
	int prev, sum;
	do{
		prev = *pointer;
		sum = prev + value;
		if(opt_yield){
			pthread_yield();
		}
	} while(__sync_val_compare_and_swap(pointer, prev, sum) != prev);
}

void add(long long *pointer, long long value) {
	long long sum = *pointer + value;
	if (opt_yield)
		pthread_yield();
	*pointer = sum;
}

pthread_mutex_t mutex;
int lock = 0;

void *threadFunction(void *arg){
	long long *counter = (long long *)arg;
	// Add and substract for the specified number of iterations
	int i;
	for(i = 0; i < numIterations; i++){
		if(sync == 'm'){
			// Lock the mutex
			int status = pthread_mutex_lock(&mutex);
			if(status){
				fprintf(stderr, "Error: An error occurred while locking\n");
				exit(EXIT_FAILURE);
			}
			add(counter, 1);
			// Unlock the mutex
			status = pthread_mutex_unlock(&mutex);
			if(status){
				fprintf(stderr, "Error, An error occurred while unlocking\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(sync == 's'){
			// Test the lock. Spin while waiting for the lock
		        while(__sync_lock_test_and_set(&lock, 1) == 1);
			add(counter, 1);
			// Release the lock
			__sync_lock_release(&lock);	
		}
		else if(sync == 'c'){
			compareAdd(counter, 1);
		}
		else{
			add(counter, 1);
		}
	}
	for(i = 0; i < numIterations; i++){	
		if(sync == 'm'){
			// Lock the mutex
			int status = pthread_mutex_lock(&mutex);
			if(status){
				fprintf(stderr, "Error: An error occurred while locking\n");
				exit(EXIT_FAILURE);
			}
			add(counter, -1);
			// Unlock the mutex
			status = pthread_mutex_unlock(&mutex);
			if(status){
				fprintf(stderr, "Error, An error occurred while unlocking\n");
				exit(EXIT_FAILURE);
			}
		}
		else if(sync == 's'){
			// Test the lock. Spin while waiting for the lock
		        while(__sync_lock_test_and_set(&lock, 1) == 1);
			add(counter, -1);
			// Release the lock
			__sync_lock_release(&lock);	
		}
		else if(sync == 'c'){
			compareAdd(counter, -1);
		}
		else{
			add(counter, -1);
		}
	}
	return NULL;
}

void runThreads(){
	long long counter = 0;
	struct timespec startTime, endTime;
	// Initialize mutex if applicable
	if(sync == 'm'){
		pthread_mutex_init(&mutex, NULL);
	}
	// Create threads
	pthread_t *threads = malloc(numThreads * sizeof(pthread_t));
	// Note the starting clock time
	int status = clock_gettime(CLOCK_MONOTONIC, &startTime);
	if(status == -1){
		perror("Clock Error");
		exit(EXIT_FAILURE);
	}
	int i;
	for(i = 0; i < numThreads; i++){
		status = pthread_create(&threads[i], NULL, threadFunction, (void *)&counter);
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
		status = pthread_mutex_destroy(&mutex);
		if(status){
			fprintf(stderr, "Error: Could not destroy mutex\n");
			exit(EXIT_FAILURE);
		}
	}
	// Print summary
	int numOps = numThreads * numIterations * 2;
	printf("%d threads x %d iterations x (add + subtract) = %d operations\n", numThreads, numIterations, numOps);
	if(counter){
		fprintf(stderr, "ERROR: final count = %lld\n", counter);
	}
	long long elapsedTime = (endTime.tv_sec - startTime.tv_sec) * 1000000000 + (endTime.tv_nsec - startTime.tv_nsec);
	printf("elapsed time: %lldns\n", elapsedTime);
	long long avgTime = elapsedTime / numOps;
	printf("per operation: %lldns\n", avgTime);
	if(counter){
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char** argv){
	parseArgs(argc, argv);
	runThreads();
	exit(EXIT_SUCCESS);
}
