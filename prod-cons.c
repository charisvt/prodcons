#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <math.h>

#define QUEUESIZE 10
#define LOOP 2000
#define p 3 // number of producer threads
#define qt 2 // number of consumer threads
#define CONSUMER_TIMEOUT 20 // seconds

void *producer (void *args);
void *consumer (void *args);

// 
typedef struct workFunction {
  void * (*work)(void *);
  void * arg;
  struct timeval timestamp;
} workFunction;

typedef struct {
  float angle;
  float *result;
} SinArgs;

typedef struct {
  workFunction buf[QUEUESIZE];
  long head, tail;
  int full, empty;
  pthread_mutex_t *mut;
  pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction in);
void queueDel (queue *q, workFunction *out);
void *sin_wrapper (void *arg);

int main ()
{
  int i;
  queue *fifo;
  // Allocate memory for producer and consumer thread arrays
  pthread_t *producers = (pthread_t *) malloc(p * sizeof(pthread_t));
  pthread_t *consumers = (pthread_t *) malloc(qt * sizeof(pthread_t));
  if (producers == NULL || consumers == NULL) {
    fprintf (stderr, "main: Thread creation failed.\n");
    exit (1);
  }

  fifo = queueInit ();
  if (fifo ==  NULL) {
    fprintf (stderr, "main: Queue Init failed.\n");
    exit (1);
  }

  // Create p producer threads
  for (i = 0; i < p; i++) {
    pthread_create (&producers[i], NULL, producer, fifo);
  }
  
  // Create q consumer threads
  for (i = 0; i < qt; i++) {
    pthread_create (&consumers[i], NULL, consumer, fifo);
  }

  // Join p producer threads
  for (i = 0; i < p; i++) {
    pthread_join (producers[i], NULL);
  }

  struct timeval total_waiting_time;
  timerclear(&total_waiting_time);
  
  // Join q consumer threads
  for (i = 0; i < qt; i++) {
    void *consumer_result;
    pthread_join (consumers[i], &consumer_result);

    // Merge the waiting times
    struct timeval *consumer_waiting_time = (struct timeval *)consumer_result;
    timeradd(&total_waiting_time, consumer_waiting_time, &total_waiting_time);

    free(consumer_waiting_time);
  }
  
  printf("Average waiting time: %f ms\n", ((total_waiting_time.tv_sec * 1e3) + (total_waiting_time.tv_usec / 1e3)) / (LOOP * p));

  queueDelete (fifo);

  // Free the memory allocated for producer and consumer thread arrays
  free(producers);
  free(consumers);

  // Print results

  return 0;
}

void *producer(void *q) {
  queue *fifo;
  int i;

  fifo = (queue *)q;

  for (i = 0; i < LOOP; i++) {
    // lock the mutex
    pthread_mutex_lock(fifo->mut);
    while (fifo->full) {
      printf("producer: queue FULL.\n");
      pthread_cond_wait(fifo->notFull, fifo->mut);
    }
    // wait till the queue is not full
    // add the item to the queue
    // DEBUG: create actual simple function (ie. calc cosine of an angle) and add it to the queue
    workFunction wf;
    SinArgs *sin_args = (SinArgs *)malloc(sizeof(SinArgs)); // DEBUG - free this memory IN THE CONSUMER THREAD !!!
    float *result = (float *)malloc(sizeof(float)); // DEBUG - free this memory IN THE CONSUMER THREAD !!!

    sin_args->angle = i * M_PI / 180.0f;  // Convert i to radians
    sin_args->result = result;

    wf.work = sin_wrapper;
    wf.arg = sin_args;
    gettimeofday(&(wf.timestamp), NULL);

    queueAdd(fifo, wf);
    // unlock the mutex
    pthread_mutex_unlock(fifo->mut);
    // broadcast signal
    pthread_cond_broadcast(fifo->notEmpty);
  }
  return (NULL);
}


void *consumer(void *q) {
  queue *fifo;
  workFunction wf;
  struct timeval total_waiting_time;

  // Initialize the total_waiting_time to zero
  timerclear(&total_waiting_time);

  fifo = (queue *)q;

  while (1) {
    // Lock the mutex
    pthread_mutex_lock(fifo->mut);

    int timed_out = 0;

    while (fifo->empty) {
      printf("consumer: queue EMPTY.\n");

      // Calculate the timeout point
      struct timespec timeout;
      clock_gettime(CLOCK_MONOTONIC, &timeout);
      timeout.tv_sec += CONSUMER_TIMEOUT;

      // Wait with a timeout
      int ret = pthread_cond_timedwait(fifo->notEmpty, fifo->mut, &timeout);

      // Check for a timeout
      if (ret == ETIMEDOUT) {
        printf("consumer: TIMEOUT. Terminating...\n");
        timed_out = 1;
        break;
      }
    }

    // If not timed_out, dequeue the item from the queue
    if (!timed_out) {
      queueDel(fifo, &wf);
    }

    // Unlock the mutex
    pthread_mutex_unlock(fifo->mut);

    // Exit the loop if timed_out
    if (timed_out) {
      break;
    }

    // Broadcast signal
    pthread_cond_broadcast(fifo->notFull);

    // Calculate the waiting time
    struct timeval now, waiting_time;
    gettimeofday(&now, NULL);
    timersub(&now, &(wf.timestamp), &waiting_time);

    // Accumulate the waiting time
    timeradd(&total_waiting_time, &waiting_time, &total_waiting_time);

    // Call the function
    wf.work(wf.arg);

    // Print the result
    printf("consumer: sin(%f) = %f\n", ((SinArgs *)wf.arg)->angle, *(((SinArgs *)wf.arg)->result));

    // Free the memory
    free(((SinArgs *)wf.arg)->result); // DEBUG - freeing this memory as we promised in producer thread
    free(wf.arg);                       // DEBUG - freeing this memory as we promised in producer thread
  }

  // Return the accumulated waiting time as a pointer to a dynamically allocated timeval struct
  struct timeval *result = (struct timeval *)malloc(sizeof(struct timeval));
  *result = total_waiting_time;
  return (void *)result;
}




queue *queueInit (void)
{
  queue *q;

  q = (queue *)malloc (sizeof (queue));
  if (q == NULL) return (NULL);

  q->empty = 1;
  q->full = 0;
  q->head = 0;
  q->tail = 0;
  q->mut = (pthread_mutex_t *) malloc (sizeof (pthread_mutex_t));
  pthread_mutex_init (q->mut, NULL);
  q->notFull = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notFull, NULL);
  q->notEmpty = (pthread_cond_t *) malloc (sizeof (pthread_cond_t));
  pthread_cond_init (q->notEmpty, NULL);
	
  return (q);
}

void queueDelete (queue *q)
{
  pthread_mutex_destroy (q->mut);
  free (q->mut);	
  pthread_cond_destroy (q->notFull);
  free (q->notFull);
  pthread_cond_destroy (q->notEmpty);
  free (q->notEmpty);
  free (q);
}

void queueAdd (queue *q, workFunction in)
{
  q->buf[q->tail] = in;
  q->tail++;
  if (q->tail == QUEUESIZE)
    q->tail = 0;
  if (q->tail == q->head)
    q->full = 1;
  q->empty = 0;

  return;
}

void queueDel (queue *q, workFunction *out)
{
  *out = q->buf[q->head];

  q->head++;
  if (q->head == QUEUESIZE)
    q->head = 0;
  if (q->head == q->tail)
    q->empty = 1;
  q->full = 0;

  return;
}

// void *(*start_routine) (void *); function signaturue to maintain Pthread API compatibility 
void *sin_wrapper (void *arg){
  SinArgs *sin_args = (SinArgs *) arg; // cast the void pointer to a SinArgs pointer
  
  *(sin_args->result) = sin(sin_args->angle);
  
  return NULL;
}