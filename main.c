#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

// --- Data Structures ---

// A node for the Linked List to store lines
typedef struct Node {
    char *line;
    struct Node *next;
} Node;

// Defines a specific chunk of the file to process
typedef struct {
    long start_offset;
    long size;
    int id;
} Chunk;

// Shared data accessible by all threads
typedef struct {
    char *filepath;
    Chunk *chunks;          // Array of chunks
    int total_chunks;       // m
    
    // Thread synchronization
    int current_chunk_idx;  // Index of the next chunk to be picked up
    pthread_mutex_t mutex;  // Protects current_chunk_idx
    
    // Error handling flags
    volatile int error_flag; // 0 = OK, 1 = Error encountered
    
} SharedContext;

// Argument struct to pass to each worker thread
typedef struct {
    int thread_id;
    SharedContext *ctx;
    Node *head; // The head of the linked list for this specific thread
} ThreadArgs;


// --- Helper Functions ---

// Function to add a line to a local linked list
void add_line_to_list(Node **head, char *text) {
    // TODO: Implement logic to append 'text' to the linked list
    // Hint: Malloc a new Node, strdup the text, link it.
}

// Function to check comma consistency
int validate_line(char *line, int expected_commas) {
    // TODO: Return 1 if valid, 0 if invalid
    return 1; 
}


// --- Worker Thread Routine ---

void* worker_routine(void *arg) {
    ThreadArgs *args = (ThreadArgs*)arg;
    SharedContext *ctx = args->ctx;

    printf("Thread %d started.\n", args->thread_id); // Stage 1 requirement

    while (1) {
        Chunk current_task;
        int task_found = 0;

        // --- Critical Section: Get Task ---
        pthread_mutex_lock(&ctx->mutex);
        
        // Check if we should stop due to error in another thread
        if (ctx->error_flag) {
            pthread_mutex_unlock(&ctx->mutex);
            break; 
        }

        // Check if there are tasks left
        if (ctx->current_chunk_idx < ctx->total_chunks) {
            current_task = ctx->chunks[ctx->current_chunk_idx];
            ctx->current_chunk_idx++;
            task_found = 1;
        }
        
        pthread_mutex_unlock(&ctx->mutex);
        // --- End Critical Section ---

        if (!task_found) {
            break; // No more chunks, exit loop
        }

        // --- Process Chunk (Stage 2 & 3) ---
        // printf("Thread %d processing chunk starting at %ld, size %ld\n", args->thread_id, current_task.start_offset, current_task.size);

        // TODO: Open file (use fopen or open), fseek/lseek to start_offset.
        // TODO: Read lines until size is consumed.
        // TODO: Handle edge cases (lines spanning across chunks).
        
        
        // --- Error Handling (Stage 4) ---
        // If error detected:
        // 1. Lock mutex.
        // 2. Set ctx->error_flag = 1.
        // 3. Unlock.
        // 4. Wait for other threads (barrier or join logic needed here later).
        // 5. Print to stderr.
        // 6. exit(1).
    }

    // printf("Thread %d finished.\n", args->thread_id);
    return NULL; // Return the head of the list if needed, or store in args
}


// --- Main ---

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <n threads> <m chunks> <path>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    int m = atoi(argv[2]);
    char *path = argv[3];

    // 1. Read Header and File Size
    FILE *fp = fopen(path, "r");
    if (!fp) {
        perror("Error opening file");
        return 1;
    }

    // Read the first line (header) to determine context/commas (Stage 3)
    char header_buffer[1024];
    if (fgets(header_buffer, sizeof(header_buffer), fp) == NULL) {
        fprintf(stderr, "File is empty\n");
        return 1;
    }
    
    // Get file size
    struct stat st;
    stat(path, &st);
    long total_size = st.st_size;
    long data_start_pos = ftell(fp); // Where data actually starts after header
    long data_size = total_size - data_start_pos;

    fclose(fp); // Close here, threads will open their own handles or use pread

    // 2. Divide file into m chunks (Stage 1/2)
    SharedContext ctx;
    ctx.filepath = path;
    ctx.total_chunks = m;
    ctx.current_chunk_idx = 0;
    ctx.error_flag = 0;
    ctx.chunks = malloc(sizeof(Chunk) * m);
    pthread_mutex_init(&ctx.mutex, NULL);

    long chunk_size = data_size / m;
    for (int i = 0; i < m; i++) {
        ctx.chunks[i].id = i;
        ctx.chunks[i].start_offset = data_start_pos + (i * chunk_size);
        // Handle the last chunk taking the remainder
        if (i == m - 1) {
             ctx.chunks[i].size = (data_start_pos + data_size) - ctx.chunks[i].start_offset;
        } else {
             ctx.chunks[i].size = chunk_size;
        }
    }

    // 3. Create Thread Pool (Stage 1)
    pthread_t *threads = malloc(sizeof(pthread_t) * n);
    ThreadArgs *thread_args = malloc(sizeof(ThreadArgs) * n);

    for (int i = 0; i < n; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].ctx = &ctx;
        thread_args[i].head = NULL;
        
        if (pthread_create(&threads[i], NULL, worker_routine, &thread_args[i]) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // 4. Wait for threads to finish
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    // 5. Concatenate and Print Lists (Stage 5)
    // TODO: Iterate through thread_args[i].head and link them together
    // TODO: Print the final list

    // Cleanup
    pthread_mutex_destroy(&ctx.mutex);
    free(ctx.chunks);
    free(threads);
    free(thread_args);

    return 0;
}