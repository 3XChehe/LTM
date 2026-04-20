#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1000

typedef struct
{
    int fd;
    char id[50];
    int registered;
} Client;

Client clients[MAX_CLIENTS];
struct pollfd fds[MAX_CLIENTS];
int nfds = 0;

void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        clients[i].fd = -1;
}

Client *get_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd == fd)
            return &clients[i];
    return NULL;
}

int is_id_exist(char *id)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && clients[i].registered &&
            strcmp(clients[i].id, id) == 0)
            return 1;
    return 0;
}

void add_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == -1)
        {
            clients[i].fd = fd;
            clients[i].registered = 0;
            return;
        }
    }
}

void remove_client(int index)
{
    int fd = fds[index].fd;

    Client *c = get_client(fd);
    if (c)
        c->fd = -1;

    close(fd);

    // swap-remove
    fds[index] = fds[nfds - 1];
    nfds--;
}

int parse_registration(char *msg, char *id)
{
    char *colon = strchr(msg, ':');
    if (!colon)
        return 0;

    *colon = 0;

    char *left = msg;
    char *right = colon + 1;

    while (*left == ' ')
        left++;

    while (*right == ' ')
        right++;

    right[strcspn(right, "\r\n")] = 0;

    if (strcmp(left, "client_id") != 0)
        return 0;

    if (strlen(right) == 0)
        return 0;

    strcpy(id, right);
    return 1;
}

void broadcast(int sender_fd, char *msg)
{
    for (int i = 0; i < nfds; i++)
    {
        if (fds[i].fd != sender_fd)
        {
            Client *c = get_client(fds[i].fd);
            if (c && c->registered)
            {
                send(fds[i].fd, msg, strlen(msg), 0);
            }
        }
    }
}

void get_time_str(char *buf)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, 64, "%Y/%m/%d %H:%M:%S", t);
}

int main()
{
    int listener;
    struct sockaddr_in addr;

    init_clients();

    listener = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 10);

    fds[0].fd = listener;
    fds[0].events = POLLIN;
    nfds = 1;

    printf("Server running on port %d...\n", PORT);

    while (1)
    {
        int ret = poll(fds, nfds, -1);
        if (ret < 0)
        {
            perror("poll");
            break;
        }

        for (int i = 0; i < nfds; i++)
        {

            if (!(fds[i].revents & POLLIN))
                continue;

            if (fds[i].fd == listener)
            {
                int client = accept(listener, NULL, NULL);

                if (nfds < MAX_CLIENTS)
                {
                    fds[nfds].fd = client;
                    fds[nfds].events = POLLIN;
                    nfds++;

                    add_client(client);

                    send(client, "Nhap: client_id: X\n", strlen("Nhap: client_id: X\n"), 0);
                }
                else
                {
                    close(client);
                }
            }
            else
            {
                char buf[BUFFER_SIZE];
                int len = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                if (len <= 0)
                {
                    remove_client(i);
                    i--;
                    continue;
                }

                buf[len] = 0;

                Client *c = get_client(fds[i].fd);

                if (!c->registered)
                {
                    char temp[BUFFER_SIZE];
                    strcpy(temp, buf);

                    char new_id[50];

                    if (parse_registration(temp, new_id))
                    {

                        if (is_id_exist(new_id))
                        {
                            send(c->fd, "ID da ton tai!\n", strlen("ID da ton tai!\n"), 0);
                        }
                        else
                        {
                            strcpy(c->id, new_id);
                            c->registered = 1;

                            send(c->fd, "Dang ky thanh cong!\n", strlen("Dang ky thanh cong!\n"), 0);
                        }
                    }
                    else
                    {
                        send(c->fd, "Sai format. Dung: client_id: X\n", strlen("Sai format. Dung: client_id: X\n"), 0);
                    }
                }
                else
                {
                    char timebuf[64];
                    get_time_str(timebuf);

                    char msg[BUFFER_SIZE];
                    snprintf(msg, sizeof(msg),
                             "%s %s: %s",
                             timebuf, c->id, buf);

                    broadcast(c->fd, msg);
                }
            }
        }
    }

    close(listener);
}