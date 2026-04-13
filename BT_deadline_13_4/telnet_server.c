#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 9090
#define BUFFER_SIZE 1024
#define MAX_CLIENTS FD_SETSIZE

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

void remove_client(int fd)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i].fd == fd)
        {
            close(fd);
            clients[i].fd = -1;
            return;
        }
    }
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
        char *err = "Khong doc duoc file\n";
        send(fd, err, strlen(err), 0);
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

    printf("Telnet server running...\n");

    while (1)
    {
        readfds = master;

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        for (int i = 0; i <= max_fd; i++)
        {
            if (FD_ISSET(i, &readfds))
            {

                if (i == listener)
                {
                    // new client
                    addrlen = sizeof(client_addr);
                    newfd = accept(listener, (struct sockaddr *)&client_addr, &addrlen);

                    FD_SET(newfd, &master);
                    if (newfd > max_fd)
                        max_fd = newfd;

                    add_client(newfd);

                    char *msg = "Username: ";
                    send(newfd, msg, strlen(msg), 0);
                }
                else
                {
                    char buf[BUFFER_SIZE];
                    int n = recv(i, buf, sizeof(buf) - 1, 0);

                    if (n <= 0)
                    {
                        remove_client(i);
                        FD_CLR(i, &master);
                    }
                    else
                    {
                        buf[n] = '\0';
                        buf[strcspn(buf, "\r\n")] = 0;

                        Client *c = get_client(i);

                        if (c->state == STATE_USER)
                        {
                            strcpy(c->username, buf);
                            c->state = STATE_PASS;

                            char *msg = "Password: ";
                            send(i, msg, strlen(msg), 0);
                        }
                        else if (c->state == STATE_PASS)
                        {
                            if (check_login(c->username, buf))
                            {
                                c->state = STATE_AUTH;
                                char *ok = "Login success!\n$ ";
                                send(i, ok, strlen(ok), 0);
                            }
                            else
                            {
                                char *fail = "Login failed!\nUsername: ";
                                send(i, fail, strlen(fail), 0);
                                c->state = STATE_USER;
                            }
                        }
                        else if (c->state == STATE_AUTH)
                        {
                            execute_command(i, buf);

                            char *prompt = "\n$ ";
                            send(i, prompt, strlen(prompt), 0);
                        }
                    }
                }
            }
        }
    }
}