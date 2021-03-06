#include <stdio.h>
// #include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <limits.h>
#include <pthread.h>
#include "myqueue.hpp"
#include "mystack.hpp"
#include "mymemory.hpp"
#include <signal.h>
#define MAX_LIMIT 1024

#define SERVERPORT 5008
#define BUFSIZE 1024
#define SOCKETERROR (-1)
#define SERVER_BACKLOG 100
#define THREAD_POOL_SIZE 20

int counter = 4000;

pthread_t thread_pool[THREAD_POOL_SIZE];

typedef struct sockaddr_in SA_IN;
typedef struct sockaddr SA;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t stack_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition_var = PTHREAD_COND_INITIALIZER;

char size_message[1024];
int server_socket;
void *handle_connection(void *p_client_socket);
int check(int exp, const char *msg);
void *thread_function(void *arg);

void ctrlc_handler(int num)
{

    close(server_socket);
    printf("DEBUG:server socket finish  %d", server_socket);
}

int main(int argc, char **argv)
{
    signal(SIGINT, ctrlc_handler);

    int client_socket, addr_size;
    SA_IN server_addr, client_addr;

    // first off we create a bunch of threads
    for (int i = 0; i < THREAD_POOL_SIZE; i++)
    {
        pthread_create(&thread_pool[i], NULL, thread_function, NULL);
    }

    check((server_socket = socket(AF_INET, SOCK_STREAM, 0)), "Failed to craete socket");

    // initialize the adress struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVERPORT);

    check(bind(server_socket, (SA *)&server_addr, sizeof(server_addr)), "Bind Failed!");
    check(listen(server_socket, SERVER_BACKLOG), "Listen Failed!");

    while (true)
    {
        printf("DEBUG:Waiting for connections... \n");
        // wait for and eventually accept an incomming connection

        addr_size = sizeof(SA_IN);
        check(client_socket = accept(server_socket, (SA *)&client_addr, (socklen_t *)&addr_size), "accept failed");
        printf("DEBUG:connected! on socket %d \n", client_socket);

        // do whatever we do with connections.
        int *pclient = (int *)malloc(sizeof(int));
        *pclient = client_socket;
        pthread_mutex_lock(&mutex);
        enqueue(pclient);
        pthread_cond_signal(&condition_var);
        pthread_mutex_unlock(&mutex);
    }
    close(server_socket);
    printf("DEBUG:server socket finish  %d", server_socket);
}

void *thread_function(void *arg)
{
    while (true)
    {
        // mutex and cond are designed to work with each other so it is writen like that so that a mutex will not block
        // if contition doesnt met
        int *pclient;
        pthread_mutex_lock(&mutex);
        if ((pclient = dequeue()) == NULL)
        {
            pthread_cond_wait(&condition_var, &mutex);
            // try again
            pclient = dequeue();
        }
        pthread_mutex_unlock(&mutex);
        if (pclient != NULL)
        {
            // we have a connection

            handle_connection(pclient);
        }
    }
}

int check(int exp, const char *msg)
{
    if (exp == SOCKETERROR)
    {
        perror(msg);
        return 1;
    }
    return exp;
}

void *handle_connection(void *p_client_socket)
{
    int client_socket = *((int *)p_client_socket);
    free(p_client_socket);
    char client_message[1024];

    while (true)
    {

        bzero(client_message, sizeof(client_message));
        recv(client_socket, client_message, sizeof(client_message), 0);
        pthread_mutex_lock(&stack_mutex);

        if (strncmp(client_message, "PUSH", 4) == 0)
        {
            // printf("DEBUG:from client : %s\n", client_message);
            memcpy(client_message, client_message + 5, MAX_LIMIT - 5);
            push(client_message);
            // printf("DEBUG: push good! socket %d\n", client_socket);
        }
        else if (strncmp(client_message, "POP", 3) == 0)
        {

            // printf("DEBUG: from client : %s \n", client_message);
            if (get_size() == 0)
            {
                // strcpy(client_message, "EMPTY");
                // send(client_socket, client_message, 1024, 0);
            }
            else
            {
                pop();
            }

            // printf("DEBUG: pop good!\n");
        }
        else if (strncmp(client_message, "TOP", 3) == 0)
        {
            // printf("DEBUG: from client : %s \n", client_message);
            if (get_size() == 0)
            {
                strcpy(client_message, "OUTPUT:EMPTY");
                send(client_socket, client_message, 1024, 0);
            }
            else
            {
                char *msg = top();
                send(client_socket, msg, 1024, 0);
                free(msg);
            }
        }
        else if (strncmp(client_message, "size", 4) == 0)
        {
            // printf("DEBUG: IN SIZE: %s \n", client_message);
            printf("DEBUG: size call \n");
            int output = get_size();
            bzero(client_message, sizeof(client_message));
            strcat(client_message, "DEBUG:");
            sprintf(size_message, "%d", output);
            strncat(client_message, size_message, sizeof(size_message));
            send(client_socket, client_message, sizeof(client_message), 0);
        }
        else if (strncmp(client_message, "exit", 4) == 0)
        {
            pthread_mutex_unlock(&stack_mutex);
            return NULL;
        }

        if (strncmp(client_message, "hello from ruby \n", 17) == 0) /* hello from ruby \n */
        {
            char buffer_ruby_test[BUFSIZE] = "hi from server ";
            printf("%s", client_message);
            send(client_socket, buffer_ruby_test, 1024, 0);
            bzero(buffer_ruby_test, sizeof(buffer_ruby_test));
            close(client_socket);
            pthread_mutex_unlock(&stack_mutex);
            return NULL;
        }
        pthread_mutex_unlock(&stack_mutex);
    }
    return NULL;
}
