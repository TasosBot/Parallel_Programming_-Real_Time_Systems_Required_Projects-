#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>

#define P 8
#define Q 8
#define QUEUESIZE 10

void *producer (void *args);
void *consumer (void *args);

typedef struct {
    double angles[10];
} math_args;

void *do_math(void *arg) {
    math_args *data = (math_args *)arg;
    for(int j=0; j<10; j++) {
        double r = sin(data->angles[j]);
    }
    free(data); 
    return NULL;
}

typedef struct {
    void * (*work)(void *); 
    void * arg;             
    struct timeval start_time; 
} workFunction;

typedef struct {
    workFunction *buf[QUEUESIZE];
    long head, tail;
    int full, empty;
    pthread_mutex_t *mut;
    pthread_cond_t *notFull, *notEmpty;
} queue;

queue *queueInit (void);
void queueDelete (queue *q);
void queueAdd (queue *q, workFunction *workin);
void queueDel (queue *q, workFunction **workout);

int main ()
{
    queue *fifo;
    fifo = queueInit ();
    if (fifo == NULL) {
        fprintf (stderr, "main: Queue Init failed.\n");
        exit (1);
    }

    pthread_t pro[P], con[Q];

    for (int i = 0; i < P; i++) {
        pthread_create(&pro[i], NULL, producer, fifo);
    }

    for (int i = 0; i < Q; i++) {
        pthread_create(&con[i], NULL, consumer, fifo);
    }

    for (int i = 0; i < P; i++) {
        pthread_join(pro[i], NULL);
    }

    workFunction *stopTask = malloc(sizeof(workFunction));
    stopTask->work = NULL; 
    stopTask->arg = NULL;
    
    pthread_mutex_lock(fifo->mut);
    while (fifo->full) {
        pthread_cond_wait(fifo->notFull, fifo->mut);
    }
    queueAdd(fifo, stopTask);
    pthread_cond_broadcast(fifo->notEmpty); 
    pthread_mutex_unlock(fifo->mut);

    long int grand_total = 0;
    for (int i = 0; i < Q; i++) {
        long int *res;
        pthread_join(con[i], (void **)&res);
        if (res) {
            grand_total += *res;
            free(res);
        }
    }
    
    printf("Average waiting time: %.2f us\n", (double)grand_total / 1000000.0);

    workFunction *finalCleanup;
    while (!fifo->empty) {
        queueDel(fifo, &finalCleanup);
        free(finalCleanup);
    }
    queueDelete(fifo);

    return 0;
}

void *producer (void *q)
{
    queue *fifo = (queue *)q;
    int count = 0;

    while(count < 1000000/P) {
        math_args *m = malloc(sizeof(math_args));
        for(int j=0; j<10; j++) {
            m->angles[j] = rand() % 360;
        }

        workFunction *task = malloc(sizeof(workFunction));
        task->work = do_math;
        task->arg = m;

        gettimeofday(&task->start_time, NULL);
        
        pthread_mutex_lock (fifo->mut);
        while (fifo->full) {
            pthread_cond_wait (fifo->notFull, fifo->mut);
        }
        queueAdd (fifo, task);
        pthread_cond_signal (fifo->notEmpty);
        pthread_mutex_unlock (fifo->mut);
        count++;
    }
    
    return NULL;
}

void *consumer (void *q)
{
    queue *fifo = (queue *)q;
    long int total = 0;

    while(1) {
        pthread_mutex_lock(fifo->mut);

        while (fifo->empty) {
            pthread_cond_wait(fifo->notEmpty, fifo->mut);
        }

        workFunction *task;
        queueDel(fifo, &task);

        if (task->work == NULL) {
            queueAdd(fifo, task); 
            pthread_cond_signal(fifo->notEmpty);
            pthread_mutex_unlock(fifo->mut);

            long int *result = malloc(sizeof(long int));
            *result = total; 
            pthread_exit(result); 
        }

        struct timeval end_time;
        gettimeofday(&end_time, NULL);
        long int diff = (end_time.tv_sec - task->start_time.tv_sec) * 1000000L + 
                        (end_time.tv_usec - task->start_time.tv_usec);
        total += diff;

        pthread_cond_signal(fifo->notFull);
        pthread_mutex_unlock(fifo->mut);

        task->work(task->arg);
        free(task);
    }
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

void queueAdd (queue *q, workFunction *workin)
{
    q->buf[q->tail] = workin;
    q->tail++;

    if (q->tail == QUEUESIZE)
        q->tail = 0;

    if (q->tail == q->head)
        q->full = 1;

    q->empty = 0;

    return;
}

void queueDel (queue *q, workFunction **workout)
{
    *workout = q->buf[q->head];
    q->head++;

    if (q->head == QUEUESIZE)
        q->head = 0;

    if (q->head == q->tail)
        q->empty = 1;

    q->full = 0;

    return;
}