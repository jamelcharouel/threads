// Jamel Charouel jtc2dd
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <semaphore.h>
#include <math.h>
#include <limits.h>

#define arraySize 10000

// Struct to pass into pthread create, holds input and a thread ID
struct threadargs {
	int * maxArray;
	int threadid;	
};

//Define semaphores
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv;
sem_t turnstile;
sem_t turnstile2;
sem_t mutex2;
sem_t programdone;

//Function to calculate log base 2 of a number (taken from Stack Overflow)
static unsigned int mylog2 (unsigned int val) {
    if (val == 0) return UINT_MAX;
    if (val == 1) return 0;
    unsigned int ret = 0;
    while (val > 1) {
        val >>= 1;
        ret++;
    }
    return ret;
}

// Declaration of thread function
void * findMax(void *);

//Global variables
int numlines = 0; //Number of numbers in input file
int numthreads = 0; //Number of threads to create (numlines/2)
int globalindex = 1; // Global index to keep track of maxes
int count = 0; // Global count to keep track of threads in the barrier

/**
 Main method, creates pthreads, waits on programdone until all threads are finished, returns max
**/
int main(int argc,char *argv[] ) {

    char * input = malloc(1000);
    int maxArray[arraySize];
    int FinalMax;

    //Initialize semaphores
    pthread_cond_init(&cv, NULL);
    pthread_mutex_init(&mutex, NULL);
    sem_init(&turnstile, 0, 0);
    sem_init(&turnstile2, 0, 1);
    sem_init(&mutex2, 0, 1);
    sem_init(&programdone, 0, 0);

    //Loop to get input from stdin (cat 'textfile.txt')
    while(fgets(input, 1024, stdin)) {
	int val;
	val = atoi(input);
	maxArray[numlines] = val;
	numlines++;
    }
    numthreads = numlines / 2; // Numthreads required is half the number of numbers
 
    //Array of structs, one struct per thread
    struct threadargs args[numthreads];

    int temparray[sizeof(int)*numlines]; 
    maxArray[numlines] = '\0';
    pthread_t tid[numthreads];

    // Loop to create threads N/2 threads
    for(int i = 0; i < numthreads; i ++) {
	args[i].threadid = i; //Give threads an id (0, 1, 2, 3, 4...)
	args[i].maxArray = maxArray;	
	pthread_create(&tid[i], NULL, findMax,  &args[i]);
    }

   sem_wait(&programdone); //Once final thread is finished, return max
   FinalMax = maxArray[0];
   printf("\n%d\n", FinalMax);

   return 0;
    
}

/**
 Thread function to find the maximum; Overall function runs log2(numlines),   where numlines is the number of number  of numbers in the input textfile.
 Barrier is implemented with 3 binary semaphores
**/

void * findMax(void *input) {
    int activethreshold = numlines/2; //Int to keep track of how many threads should compute
    struct threadargs * threadArgs = input;
    int * maxArray;
    int n = numlines/2;
    int winnercount = n;
    int doublecount = 0;
    int * winners = malloc(numlines);

    maxArray = threadArgs->maxArray;
    if(maxArray == NULL) { return NULL; }
    pthread_mutex_init(&mutex, NULL); //Initialize mutex for locking

    //Loop to check for calculating max, holds barrier
    for(int i = 0; i < mylog2(numlines); i ++) {
	if(threadArgs->threadid < activethreshold) { //If this thread is part of the active threshold do calculations
	/* Check array in pairs for max numbers */
	   pthread_mutex_lock(&mutex);
	   int a = maxArray[globalindex];
	   int b = maxArray[globalindex-1];
           if(a > b) {
	      maxArray[globalindex-1] = maxArray[globalindex];
           } else {
	      maxArray[globalindex] = maxArray[globalindex-1];
           }
	   globalindex += 2;
	   pthread_mutex_unlock(&mutex);
	}	
        /* End Calculate Block */
	
	//Used barrier information from the Little Book Of Semaphores
    
	// Barrier Starts
	//Rendezvous
	sem_wait(&mutex2); //Threads pass through initial turnstile
	pthread_mutex_lock(&mutex);
	count ++;
	if(count == n) { // All threads have reached this point if count = n
	   // Release threads since they've all hit the 'checkpoint'
	   sem_wait(&turnstile2);
	   sem_post(&turnstile);
	}
	pthread_mutex_unlock(&mutex);
	sem_post(&mutex2);
	sem_wait(&turnstile);
	sem_post(&turnstile);

	//Critical Point
	sem_wait(&mutex2);
	pthread_mutex_lock(&mutex);
	count--;
	if (count == 0) { //All threads have arrived at the critical point, so do the singular calculation once the last thread arrives (and release the others afterwards)
	   for(int i = 0; i < winnercount; i ++) {
	     maxArray[i] = maxArray[doublecount];
	     doublecount += 2;
	   } 

	   winnercount /= 2;
	   globalindex = 1;
	   doublecount = 0;
	   sem_wait(&turnstile);
	   sem_post(&turnstile2);
	}
	pthread_mutex_unlock(&mutex);
	sem_post(&mutex2);
	sem_wait(&turnstile2);
	sem_post(&turnstile2);
	// Barrier Ends

	activethreshold /= 2; //Half the amount of threads to use next time
    } //End for loop for calculating maxes

    //Once threads are done computing, let main read the max value
    if(threadArgs->threadid == 0) {
        sem_post(&programdone);
	pthread_mutex_destroy(&mutex);
    }
}
