#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 9090
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1000

#define STATE_USER 0
#define STATE_PASS 1
#define STATE_AUTH 2

typedef struct
{
    int fd;
    int state;
    char username[50];
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

void add_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == -1)
        {
            clients[i].fd = fd;
            clients[i].state = STATE_USER;
            return;
        }
    }
}

void remove_client(int index)
{
    int fd = fds[index].fd;
    close(fd);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == fd)
        {
            clients[i].fd = -1;
            break;
        }
    }

    fds[index] = fds[nfds - 1];
    nfds--;
}

int check_login(char *user, char *pass)
{
    FILE *f = fopen("users.txt", "r");
    if (!f)
        return 0;

    char u[50], p[50];
    while (fscanf(f, "%s %s", u, p) != EOF)
    {
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

void execute_command(int fd, char *cmd)
{
    char fullcmd[BUFFER_SIZE];
    snprintf(fullcmd, sizeof(fullcmd), "%s > out.txt", cmd);

    system(fullcmd);

    FILE *f = fopen("out.txt", "r");
    if (!f)
    {
        send(fd, "Khong doc duoc file\n", 21, 0);
        return;
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), f))
    {
        send(fd, line, strlen(line), 0);
    }

    fclose(f);
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

    // poll init
    fds[0].fd = listener;
    fds[0].events = POLLIN;
    nfds = 1;

    printf("Telnet server running...\n");

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
                // new client
                int client = accept(listener, NULL, NULL);

                if (nfds < MAX_CLIENTS)
                {
                    fds[nfds].fd = client;
                    fds[nfds].events = POLLIN;
                    nfds++;

                    add_client(client);

                    send(client, "Username: ", strlen("Username: "), 0);
                }
                else
                {
                    close(client);
                }
            }
            else
            {
                char buf[BUFFER_SIZE];
                int n = recv(fds[i].fd, buf, sizeof(buf) - 1, 0);

                if (n <= 0)
                {
                    remove_client(i);
                    i--;
                    continue;
                }

                buf[n] = 0;
                buf[strcspn(buf, "\r\n")] = 0;

                Client *c = get_client(fds[i].fd);

                if (c->state == STATE_USER)
                {
                    strcpy(c->username, buf);
                    c->state = STATE_PASS;
                    send(c->fd, "Password: ", strlen("Password: "), 0);
                }
                else if (c->state == STATE_PASS)
                {
                    if (check_login(c->username, buf))
                    {
                        c->state = STATE_AUTH;
                        send(c->fd, "Login success!\n$ ", strlen("Login success!\n$ "), 0);
                    }
                    else
                    {
                        send(c->fd, "Login failed!\nUsername: ", strlen("Login failed!\nUsername: "), 0);
                        c->state = STATE_USER;
                    }
                }
                else if (c->state == STATE_AUTH)
                {
                    execute_command(c->fd, buf);
                    send(c->fd, "\n$ ", 3, 0);
                }
            }
        }
    }

    close(listener);
}