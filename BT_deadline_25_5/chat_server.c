#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>

#define MAX_CLIENTS 1000

typedef struct
{
    int fd;
    char *id;
} ClientInfo;

ClientInfo clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void *client_thread(void *);

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        return 1;
    }
    listen(listener, 5);

    memset(clients, 0, sizeof(clients));
    printf("Chat Server listening on port 8080...\n");

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        printf("New client connected: %d\n", client);

        int *pclient = malloc(sizeof(int));
        *pclient = client;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_thread, pclient);
        pthread_detach(thread_id);
    }

    close(listener);
    return 0;
}

void *client_thread(void *params)
{
    int client_fd = *(int *)params;
    free(params);

    int my_index = -1;
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == 0)
        {
            clients[i].fd = client_fd;
            clients[i].id = NULL;
            my_index = i;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    if (my_index == -1)
    {
        char *full_msg = "Server full.\n";
        send(client_fd, full_msg, strlen(full_msg), 0);
        close(client_fd);
        return NULL;
    }

    char *msg = "Format: 'client_id: name'!\n";
    send(client_fd, msg, strlen(msg), 0);

    char buf[256];

    while (1)
    {
        int len = recv(client_fd, buf, sizeof(buf) - 1, 0);
        if (len <= 0)
            break;

        buf[len] = 0;
        if (buf[len - 1] == '\n')
            buf[len - 1] = 0;
        if (buf[strlen(buf) - 1] == '\r')
            buf[strlen(buf) - 1] = 0;
        printf("Log from %d: %s\n", client_fd, buf);

        pthread_mutex_lock(&clients_mutex);
        char *my_id = clients[my_index].id;
        pthread_mutex_unlock(&clients_mutex);

        if (my_id == NULL)
        {
            char cmd[32], id[32], tmp[32];
            int n = sscanf(buf, "%s %s %s", cmd, id, tmp);
            if (n == 2 && strcmp(cmd, "client_id:") == 0)
            {
                send(client_fd, "de nhan tin nhap theo format 'target msg'!\n", 49, 0);
                pthread_mutex_lock(&clients_mutex);
                clients[my_index].id = malloc(strlen(id) + 1);
                strcpy(clients[my_index].id, id);
                pthread_mutex_unlock(&clients_mutex);
            }
            else
            {
                send(client_fd, "Error. Sai cu phap dang nhap!\n", 30, 0);
            }
        }
        else
        {
            char target[32];
            int n = sscanf(buf, "%s", target);
            if (n == 0)
                continue;

            pthread_mutex_lock(&clients_mutex);
            char out_buf[512];
            char *pos = buf + strlen(target) + 1;
            sprintf(out_buf, "%s: %s\n", clients[my_index].id, pos);

            if (strcmp(target, "all") == 0)
            {
                for (int j = 0; j < MAX_CLIENTS; j++)
                {
                    if (clients[j].fd != 0 && clients[j].id != NULL && j != my_index)
                    {
                        send(clients[j].fd, out_buf, strlen(out_buf), 0);
                    }
                }
            }
            else
            {
                int j = 0;
                for (; j < MAX_CLIENTS; j++)
                {
                    if (clients[j].fd != 0 && clients[j].id != NULL && strcmp(target, clients[j].id) == 0)
                        break;
                }
                if (j < MAX_CLIENTS)
                {
                    send(clients[j].fd, out_buf, strlen(out_buf), 0);
                }
                else
                {
                    send(client_fd, "User khong truc tuyen.\n", 23, 0);
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    }

    pthread_mutex_lock(&clients_mutex);
    if (clients[my_index].id != NULL)
        free(clients[my_index].id);
    clients[my_index].fd = 0;
    clients[my_index].id = NULL;
    pthread_mutex_unlock(&clients_mutex);

    printf("Client %d disconnected\n", client_fd);
    close(client_fd);
    return NULL;
}