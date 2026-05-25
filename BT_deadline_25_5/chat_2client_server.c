#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

typedef struct
{
    int my_fd;
    int peer_fd;
} PairArgs;

int waiting_client = -1;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

void *handle_pair_chat(void *);

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)))
    {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Chat 2 nguoi Server is listening on port 8080...\n");

    while (1)
    {
        int new_client = accept(listener, NULL, NULL);
        printf("Client %d connected. Checking waiting queue...\n", new_client);

        pthread_mutex_lock(&queue_mutex);

        if (waiting_client == -1)
        {
            waiting_client = new_client;
            pthread_mutex_unlock(&queue_mutex);

            char *wait_msg = "Dang cho nguoi thu hai vao phong de ghep cap...\n";
            send(new_client, wait_msg, strlen(wait_msg), 0);
        }
        else
        {
            int peer_client = waiting_client;
            waiting_client = -1;
            pthread_mutex_unlock(&queue_mutex);

            printf("Pairing success: Client %d <==> Client %d\n", peer_client, new_client);

            char *start_msg = "Ghep cap thanh cong!\n";
            send(peer_client, start_msg, strlen(start_msg), 0);
            send(new_client, start_msg, strlen(start_msg), 0);

            PairArgs *args1 = malloc(sizeof(PairArgs));
            args1->my_fd = peer_client;
            args1->peer_fd = new_client;

            pthread_t thread1;
            pthread_create(&thread1, NULL, handle_pair_chat, args1);
            pthread_detach(thread1);

            PairArgs *args2 = malloc(sizeof(PairArgs));
            args2->my_fd = new_client;
            args2->peer_fd = peer_client;

            pthread_t thread2;
            pthread_create(&thread2, NULL, handle_pair_chat, args2);
            pthread_detach(thread2);
        }
    }

    close(listener);
    return 0;
}

void *handle_pair_chat(void *params)
{
    PairArgs *args = (PairArgs *)params;
    int my_fd = args->my_fd;
    int peer_fd = args->peer_fd;

    free(args);

    char buf[256];
    while (1)
    {
        int len = recv(my_fd, buf, sizeof(buf) - 1, 0);

        if (len <= 0)
        {
            printf("Client %d disconnected. Ending pair with Client %d.\n", my_fd, peer_fd);
            break;
        }

        buf[len] = 0;
        char forward_buf[512];
        snprintf(forward_buf, sizeof(forward_buf), "Friend: %s", buf);
        send(peer_fd, forward_buf, strlen(forward_buf), 0);
    }

    close(my_fd);
    close(peer_fd);

    return NULL;
}