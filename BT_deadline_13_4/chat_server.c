#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS FD_SETSIZE

typedef struct
{
    int fd;
    char id[50];
    char name[100];
    int registered;
} Client;

Client clients[MAX_CLIENTS];

void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].registered = 0;
    }
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

void remove_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == fd)
        {
            close(fd);
            clients[i].fd = -1;
            clients[i].registered = 0;
            return;
        }
    }
}

Client *get_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == fd)
            return &clients[i];
    }
    return NULL;
}

int parse_registration(char *msg, char *id, char *name)
{
    // format: id: name
    char *colon = strchr(msg, ':');
    if (!colon)
        return 0;

    *colon = '\0';
    strcpy(id, msg);
    strcpy(name, colon + 1);

    // remove newline
    name[strcspn(name, "\r\n")] = 0;

    return 1;
}

void broadcast(int sender_fd, char *message)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd != -1 && clients[i].fd != sender_fd && clients[i].registered)
        {
            send(clients[i].fd, message, strlen(message), 0);
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
    int listener, newfd, max_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addrlen;

    fd_set master, readfds;

    init_clients();

    listener = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(listener, 10);

    FD_ZERO(&master);
    FD_SET(listener, &master);
    max_fd = listener;

    printf("Server running on port %d...\n", PORT);

    while (1)
    {
        readfds = master;

        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(1);
        }

        for (int i = 0; i <= max_fd; i++)
        {
            if (FD_ISSET(i, &readfds))
            {

                if (i == listener)
                {
                    // new connection
                    addrlen = sizeof(client_addr);
                    newfd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);

                    FD_SET(newfd, &master);
                    if (newfd > max_fd)
                        max_fd = newfd;

                    add_client(newfd);

                    char *msg = "Nhap theo format: client_id: client_name\n";
                    send(newfd, msg, strlen(msg), 0);
                }
                else
                {
                    // receive data
                    char buf[BUFFER_SIZE];
                    int nbytes = recv(i, buf, sizeof(buf) - 1, 0);

                    if (nbytes <= 0)
                    {
                        remove_client(i);
                        FD_CLR(i, &master);
                    }
                    else
                    {
                        buf[nbytes] = '\0';

                        Client *c = get_client(i);

                        if (!c->registered)
                        {
                            if (parse_registration(buf, c->id, c->name))
                            {
                                c->registered = 1;
                                char *ok = "Dang ky thanh cong!\n";
                                send(i, ok, strlen(ok), 0);
                            }
                            else
                            {
                                char *err = "Sai format. Nhap lai: id: name\n";
                                send(i, err, strlen(err), 0);
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

                            broadcast(i, msg);
                        }
                    }
                }
            }
        }
    }
}