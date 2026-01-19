#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define ERR(source) (perror(source), fprintf(stderr, "%s:%d\n", __FILE__, __LINE__), exit(EXIT_FAILURE))
#define UNUSED(x) ((void)(x))

#define DECK_SIZE (4 * 13)
#define HAND_SIZE (7)
volatile sig_atomic_t sigusr1_count = 0;
volatile sig_atomic_t sigint_received = 0;

void print_deck(const int *deck, int size);
void shuffle(int *array, size_t n);


typedef struct
{
    int seated;           // how many players are currently at the table
    int table_size;       // n
    int game_active;      // 0 = waiting, 1 = table full
    int shutdown;         // set when SIGINT arrives

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int game_over;          
    int winner_count;       

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    pthread_barrier_t round_barrier; 
} table_t;
typedef struct{
    int id;
    int hand[HAND_SIZE];
    table_t* table;
}player_arg_t;


void sigusr1_handler(int sig)
{
    UNUSED(sig);
    sigusr1_count++;
}

void sigint_handler(int sig)
{
    UNUSED(sig);
    sigint_received=1;
}
void usage(const char *program_name)
{
    fprintf(stderr, "USAGE: %s n\n", program_name);
    exit(EXIT_FAILURE);
}

void shuffle(int *array, size_t n)
{
    if (n > 1)
    {
        size_t i;
        for (i = 0; i < n - 1; i++)
        {
            size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
            int t = array[j];
            array[j] = array[i];
            array[i] = t;
        }
    }
}
void* player_thread(void* arg)
{
    player_arg_t* p = arg;

    printf("Player %d hand: ", p->id);
    print_deck(p->hand, HAND_SIZE);

    pthread_mutex_lock(&p->table->mutex);
    while(!p->table->game_active && !p->table->shutdown)
    {
        pthread_cond_wait(&p->table->cond, &p->table->mutex);
    }
    pthread_mutex_unlock(&p->table->mutex);
    if (p->table->shutdown)
        return NULL;
}
void print_deck(const int *deck, int size)
{
    const char *suits[] = {" of Hearts", " of Diamonds", " of Clubs", " of Spades"};
    const char *values[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King", "Ace"};

    char buffer[1024];
    int offset = 0;

    if (size < 1 || size > DECK_SIZE)
        return;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");
    for (int i = 0; i < size; ++i)
    {
        int card = deck[i];
        if (card < 0 || card > DECK_SIZE)
            return;
        int suit = deck[i] % 4;
        int value = deck[i] / 4;

        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s", values[value]);
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%s", suits[suit]);
        if (i < size - 1)
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
    }
    snprintf(buffer + offset, sizeof(buffer) - offset, "]");

    puts(buffer);
}

int main(int argc, char *argv[])
{
    

    if(argc!=2) {usage(argv[0]);}
    int n = atoi(argv[1]);

    if (n < 4 || n > 7){usage(argv[0]);}
    srand(time(NULL));


    struct sigaction sa = {0};
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);


    int deck[DECK_SIZE];
    for (int i = 0; i < DECK_SIZE; ++i)
        deck[i] = i;
    shuffle(deck, DECK_SIZE);


    table_t table = {
    .seated = 0,
    .table_size = n,
    .game_active = 0,
    .shutdown = 0,
    .mutex = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER
    };

    pthread_t threads[n];
    player_arg_t args[n];

    int next_player_id = 0;

    int deck_index = 0;

    while(!sigint_received)
    {
        pause();
        if(sigusr1_count>0)
        {
            sigusr1_count--;
            pthread_mutex_lock(&table.mutex);
            if(table.seated==table.table_size)
            {
                printf("Table full\n");
                pthread_mutex_unlock(&table.mutex);
                continue;

            }

            int id = next_player_id++;
            table.seated++;
            args[id].id = id;
            args[id].table = &table;
            for(int j=0; j<HAND_SIZE; j++)
            {
                args[id].hand[j] = deck[deck_index++];
            }

            if(pthread_create(&threads[id], NULL, player_thread, &args[id])!=0) {ERR("Error creating thread");}
            if(table.seated==table.table_size)
            {
                table.game_active=1;
                pthread_cond_broadcast(&table.cond);
            }


            pthread_mutex_unlock(&table.mutex);
        }
    }
    for (int i = 0; i < n; i++)
    {
        if (pthread_join(threads[i], NULL) != 0) {ERR("pthread_join");}
    }
    exit(EXIT_SUCCESS);
}
