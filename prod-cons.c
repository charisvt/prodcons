#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define QUEUESIZE 10
#define LOOP 2000
#define p 3 // number of producer threads
#define qt 2 // number of consumer threads

void *producer (void *args);
void *consumer (void *args);

// 
typedef struct workFunction {
  void * (*work)(void *);
  void * arg;
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

  // Join q consumer threads
  for (i = 0; i < qt; i++) {
    pthread_join (consumers[i], NULL);
  }

  queueDelete (fifo);

  // Free the memory allocated for producer and consumer thread arrays
  free(producers);
  free(consumers);

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

    queueAdd(fifo, wf);
    // unlock the mutex
    pthread_mutex_unlock(fifo->mut);
    // broadcast signal
    pthread_cond_broadcast(fifo->notEmpty);
  }
  return (NULL);
}


void *consumer (void *q)
{
  queue *fifo;
  workFunction wf;

  fifo = (queue *)q;

  while (1) {
    // lock the mutex
    pthread_mutex_lock (fifo->mut);
    while (fifo->empty) {
      printf ("consumer: queue EMPTY.\n");
      pthread_cond_wait (fifo->notEmpty, fifo->mut);
    }
    // wait till the queue is not empty
    // remove the item from the queue
    queueDel (fifo, &wf);
    // unlock the mutex
    pthread_mutex_unlock (fifo->mut);
    // broadcast signal
    pthread_cond_broadcast (fifo->notFull);

    // call the function
    wf.work(wf.arg);


    // Print the result
    printf("consumer: sin(%f) = %f\n", ((SinArgs *)wf.arg)->angle, *(((SinArgs *)wf.arg)->result));


    // Free the memory
    free(((SinArgs *)wf.arg)->result); // DEBUG - freeing this memory as we promised in producer thread
    free(wf.arg); // DEBUG - freeing this memory as we promised in producer thread

  }
  return (NULL);
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
  
  return;
}