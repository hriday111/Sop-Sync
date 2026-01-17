#define _GNU_SOURCE
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))


typedef struct Node{
    char* line;
    struct Node* next;
} Node;
typedef struct{
    long start;
    long size;
    int id;
} chunk_t;
typedef struct{
    chunk_t* chunks;
    int total_chunks;
    int current_chunk_idx;
    pthread_mutex_t mutex;
    char* filepath;
} shared_t;

typedef struct{
    shared_t *shared;
    Node *head;
    Node* tail;
} thread_arg_t;

void add_line(thread_arg_t *arg, char* line_content)
{
    Node *new_node = malloc(sizeof(Node));
    new_node->line = line_content;
    new_node->next = NULL;
    if (arg->head == NULL) {
        arg->head = new_node;
        arg->tail = new_node;
    } else {
        arg->tail->next = new_node;
        arg->tail = new_node;
    }
}
void* thread_work(void* args)
{   
    thread_arg_t* t_arg = (thread_arg_t*)args;
    shared_t* shared = t_arg->shared;
    while(1)
    {
        int my_task_id = -1;
        pthread_mutex_lock(&shared->mutex);
        if(shared->current_chunk_idx<shared->total_chunks)
        {
            my_task_id = shared->current_chunk_idx;
            shared->current_chunk_idx++;

        }
        pthread_mutex_unlock(&shared->mutex);
        if(my_task_id==-1)
        {
            break; // queue is empty??? THE FUCK DOES THIS MEAN TODO
        }
        chunk_t task = shared->chunks[my_task_id];

        FILE *fp = fopen(shared->filepath, "r");
        if (!fp) ERR("Thread failed to open file");
        if (task.id == 0) {
            // First chunk starts immediately
            fseek(fp, task.start, SEEK_SET);
        } else {
            // Check byte BEFORE our start to see if we are in middle of line
            fseek(fp, task.start - 1, SEEK_SET);
            int prev_char = fgetc(fp);
            
            if (prev_char != '\n') {
                char c;
                while ((c = fgetc(fp)) != '\n' && c != EOF);
            }   
        }

        char *buffer = NULL;
        size_t len = 0;
        long end_limit = task.start + task.size;
        while(ftell(fp)<end_limit)
        {
            buffer = NULL; 
            len = 0;
            
            ssize_t read = getline(&buffer, &len, fp);
            if (read == -1) {
                free(buffer); // EOF or error
                break;
            }
            add_line(t_arg, buffer);
        }
        fclose(fp);
        
    }
    return NULL;
    
}
int main(int argc, char **argv)
{
    if(argc!=4){
        fprintf(stderr, "Usage: %s <n threads> <m chunks> <path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    char *path = argv[3];

    FILE* fp = fopen(path, "r");
    if(!fp){ERR("Error reading file");}

    char header_buffer[1024];
    if(fgets(header_buffer, sizeof(header_buffer), fp)==NULL) {ERR("Error reading buffer");}
    struct stat st;
    stat(path, &st);
    long total_size = st.st_size;
    long data_start_pos= ftell(fp);
    long data_size = total_size-data_start_pos;
    chunk_t * chunks  = malloc(sizeof(chunk_t)*m);
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);
    long chunk_size = data_size/m;
    for(int i=0; i<m; i++)
    {
        chunks[i].id = i;
        chunks[i].start = data_start_pos + (i*chunk_size);
        if (i == m - 1) {
            chunks[i].size = (data_start_pos + data_size) - chunks[i].start;
        } else {
            chunks[i].size = chunk_size;
        }
    }

    shared_t shared={
        .chunks = chunks,
        .total_chunks = m,
        .current_chunk_idx = 0,
        .mutex = mutex,
        .filepath = path
    };

    pthread_t * workers = malloc(sizeof(pthread_t)*n);
    thread_arg_t *thread_args = malloc(sizeof(thread_arg_t)*n);

    for(int i = 0; i < n; i++) {
        thread_args[i].shared = &shared;
        thread_args[i].head = NULL;
        thread_args[i].tail = NULL;
        pthread_create(&workers[i], NULL, thread_work, &thread_args[i]);
    }
    for(int j=0; j<n; j++)
    {
        pthread_join(workers[j], NULL);
    }
    pthread_mutex_destroy(&mutex);
    free(thread_args);
    free(workers);
    free(chunks);
    fclose(fp);

    return EXIT_SUCCESS;
}
