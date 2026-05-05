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

struct pollfd fds[MAX_CLIENTS];
int nfds = 0;

void transform(char *s)
{
    for (int i = 0; s[i] != '\0'; i++)
    {
        if (s[i] >= 'a' && s[i] <= 'z')
        {
            if (s[i] == 'z')
                s[i] = 'a';
            else
                s[i]++;
        }
        else if (s[i] >= 'A' && s[i] <= 'Z')
        {
            if (s[i] == 'Z')
                s[i] = 'A';
            else
                s[i]++;
        }
        else if (s[i] >= '0' && s[i] <= '9')
        {
            s[i] = '9' - (s[i] - '0');
        }
    }
}

int main()
{
    int listener;
    struct sockaddr_in addr;

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

                    char msg[BUFFER_SIZE];
                    int len = snprintf(msg, BUFFER_SIZE, "Xin chao, hien dang co %d client dang ket noi\n", nfds - 1);
                    send(client, msg, len, 0);
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
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    continue;
                }

                buf[len] = 0;
                if (strcmp(buf, "exit\n") == 0)
                {
                    send(fds[i].fd, "Goodbye!\n", 9, 0);
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                }
                else
                {
                    transform(buf);
                    send(fds[i].fd, buf, strlen(buf), 0);
                }
            }
        }
    }

    close(listener);
}