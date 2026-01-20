#include "header.h"

/**
 * Thread pool
 */

#define MAX_POOL_SIZE 16

typedef struct thread_pool
{
    int size;
    pthread_t threads[MAX_POOL_SIZE];

    pthread_mutex_t mtx;
    pthread_cond_t  cv;
    void (*work_function)(void *);
    void *work_arg;

    int job_available;     // 1 if work is waiting
    int active_workers;    // number of threads currently working

    int shutdown;
} thread_pool_t;


void *worker_thread(void *args)
{
    thread_pool_t *pool = (thread_pool_t *)args;

    printf("Worker started, TID=%lu\n",
           (unsigned long)pthread_self());

    while (1)
    {
        pthread_mutex_lock(&pool->mtx);

        /* wait for work or shutdown */
        while (!pool->job_available && !pool->shutdown)
            pthread_cond_wait(&pool->cv, &pool->mtx);

        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mtx);
            break;
        }

        /* take the job */
        void (*job)(void *) = pool->work_function;
        void *arg = pool->work_arg;

        pool->job_available = 0;
        pool->active_workers++;

        /* notify dispatch that job was taken */
        pthread_cond_broadcast(&pool->cv);
        pthread_mutex_unlock(&pool->mtx);

        /* execute work OUTSIDE the lock */
        job(arg);

        pthread_mutex_lock(&pool->mtx);
        pool->active_workers--;
        pthread_cond_broadcast(&pool->cv);
        pthread_mutex_unlock(&pool->mtx);
    }

    return NULL;
}


thread_pool_t *initialize(int N)
{
    if (N > MAX_POOL_SIZE)
        ERR("thread pool is too big!");
    if (N <= 0)
        ERR("invalid pool size");

    thread_pool_t *pool = calloc(1, sizeof(thread_pool_t));
    if (!pool)
        ERR("calloc");

    pool->size = N;
    pool->shutdown = 0;

    if (pthread_mutex_init(&pool->mtx, NULL) != 0)
        ERR("pthread_mutex_init");
    if (pthread_cond_init(&pool->cv, NULL) != 0)
        ERR("pthread_cond_init");

    for (int i = 0; i < N; i++)
    {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0)
            ERR("pthread_create");
    }

    return pool;
}

void dispatch(thread_pool_t *pool,
              void (*work)(void *),
              void *arg)
{
    pthread_mutex_lock(&pool->mtx);

    /* wait until there is a free worker and no pending job */
    while (pool->job_available ||
           pool->active_workers == pool->size)
    {
        pthread_cond_wait(&pool->cv, &pool->mtx);
    }

    /* assign job */
    pool->work_function = work;
    pool->work_arg = arg;
    pool->job_available = 1;

    /* wake up workers */
    pthread_cond_broadcast(&pool->cv);

    /* wait until worker takes the job */
    while (pool->job_available)
        pthread_cond_wait(&pool->cv, &pool->mtx);

    pthread_mutex_unlock(&pool->mtx);
}


void cleanup(thread_pool_t *pool) 
{
    // TODO STAGE-4: Just do it
    UNUSED(pool);

    printf("cleanup\n"); 
}

/**
 * Worker functions
 */
typedef struct monte_carlo_args
{
    float radius;
    unsigned int sample_count;
    unsigned int hit_count;
    unsigned int seed;

} monte_carlo_args_t;

typedef struct monte_carlo_args_array
{
    monte_carlo_args_t *args;
    int thread_count;
    int task_idx;
    float radius;
} monte_carlo_args_array_t;

void circle_monte_carlo(void *args)
{
    monte_carlo_args_t *mc_args = (monte_carlo_args_t *)args;

    for (unsigned int i = 0; i < (*mc_args).sample_count; ++i)
    {
        double rand_x = (double)rand_r(&(mc_args->seed)) / RAND_MAX;
        double rand_y = (double)rand_r(&(mc_args->seed)) / RAND_MAX;

        if (rand_x * rand_x + rand_y * rand_y <= 1.0)
            (*mc_args).hit_count++;
        sleep_ms();
    }
    // DO NOT MODIFY CODE ABOVE - IT CALCULATES CIRCLE AREA
    // TODO STAGE-3: ADD THREADS SYNCHRONIZATION BELOW
}

void accumulate_monte_carlo(void *args)
{
    monte_carlo_args_array_t *mc_args = (monte_carlo_args_array_t *)args;

    // TODO STAGE-3: ADD SYNCHRONIZATION WITH WORKING THREAD AND AVERAGE RESULT FROM THEM
    int hit_total = 0, samples_total = 0;
    for (int i = 0; i < mc_args->thread_count; i++)
    {
        samples_total += mc_args->args[i].sample_count;
    }

    double res = 4 * mc_args->radius * mc_args->radius * (double)hit_total / samples_total;
    printf("TASK %d, Circle area of radius %f result %lf\n", mc_args->task_idx, mc_args->args[0].radius, res);
}

void start_monte_carlo(thread_pool_t *pool, int sampling_worker_count, float circle_radius, unsigned int sample_count,
                       int task_idx)
{
    printf("Starting TASK %d: calculating area of circle with radius %.2f\n", task_idx, circle_radius);

    monte_carlo_args_array_t *args = (monte_carlo_args_array_t *)malloc(sizeof(monte_carlo_args_array_t));
    if (!args)
        ERR("malloc");
    args->args = (monte_carlo_args_t *)malloc(sizeof(monte_carlo_args_t));
    if (!args->args)
        ERR("malloc");
    args->thread_count = sampling_worker_count;
    args->radius = circle_radius;
    args->task_idx = task_idx;

    // TODO STAGE-3: PASS CORRECT ARGUMENTS TO WORKERS
    UNUSED(sample_count);

    // Every thread will sample sample_count/sampling_worker_count points
    for (int i = 0; i < sampling_worker_count; ++i)
    {
        dispatch(pool, circle_monte_carlo, &(args->args[i]));
    }

    dispatch(pool, accumulate_monte_carlo, args);
}

void start_hello_work(thread_pool_t *pool, int sampling_worker_count)
{
    for (int i = 0; i < sampling_worker_count; ++i)
    {
        int* number = (int*)malloc(sizeof(int));
        *number = i;

        dispatch(pool, hello_world_test, number);
    }
}

void parse_monte_carlo(thread_pool_t *pool, int worker_count, int task_idx)
{
    float radius = read_float_cli();
    if (radius < 0)
    {
        fprintf(stderr, "Invalid radius\n");
        return;
    }

    int sample_count = read_int_cli();
    if (sample_count < worker_count)
    {
        fprintf(stderr, "Invalid sample count\n");
        return;
    }

    start_monte_carlo(pool, worker_count, radius, sample_count, task_idx);
}

int parse_cli(thread_pool_t *pool)
{
    int number = read_int_cli();
    if (number < 1 || number > 3)
    {
        fprintf(stderr, "Invalid command\n");
        return 1;
    }
    else if (number == 3)
    {
        return 0;
    }

    int worker_count = read_int_cli();
    if (worker_count < 1 || worker_count > MAX_POOL_SIZE)
    {
        fprintf(stderr, "Invalid worker_count\n");
        return 1;
    }

    static int task_idx = 0;
    task_idx++;

    switch (number)
    {
        case 1:
            parse_monte_carlo(pool, worker_count, task_idx);
            break;
        case 2:
            start_hello_work(pool, worker_count);
            break;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    UNUSED(argc);
    UNUSED(argv);

    srand(4);

    if (argc != 2)
    {
        printf("%s <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int pool_size = atoi(argv[1]);
    if (pool_size <= 0)
    {
        printf("Invalid thread count");
        exit(EXIT_FAILURE);
    }

    thread_pool_t *pool = initialize(pool_size);

    do
    {
        printf("\nenter command\n");
        printf("1. circle <n> <r> <s>\n");
        printf("2. hello <n>\n");
        printf("3. exit\n\n");
    } while (parse_cli(pool));


    cleanup(pool);

    return EXIT_SUCCESS;
}
