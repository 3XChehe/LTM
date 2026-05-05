#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 1000
#define MAX_TOPICS 100
#define MAX_SUBS 100

typedef struct
{
    char name[50];
    int fds[MAX_SUBS];
    int count;
} Topic;

Topic topics[MAX_TOPICS];
int topic_count = 0;

int find_topic(const char *name)
{
    for (int i = 0; i < topic_count; i++)
    {
        if (strcmp(topics[i].name, name) == 0)
            return i;
    }
    return -1;
}

int get_or_create_topic(const char *name)
{
    int idx = find_topic(name);
    if (idx != -1)
        return idx;

    strcpy(topics[topic_count].name, name);
    topics[topic_count].count = 0;
    return topic_count++;
}
void subscribe(int fd, const char *topic_name)
{
    int idx = get_or_create_topic(topic_name);

    for (int i = 0; i < topics[idx].count; i++)
    {
        if (topics[idx].fds[i] == fd)
            return;
    }

    topics[idx].fds[topics[idx].count++] = fd;
}
void publish(const char *topic_name, const char *message)
{
    int idx = find_topic(topic_name);
    if (idx == -1)
        return;

    for (int i = 0; i < topics[idx].count; i++)
    {
        int fd = topics[idx].fds[i];
        send(fd, message, strlen(message), 0);
        send(fd, "\n", 1, 0);
    }
}

struct pollfd fds[MAX_CLIENTS];
int nfds = 0;

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
                buf[len] = 0;
                buf[strcspn(buf, "\r\n")] = 0;
                if (len <= 0)
                {
                    close(fds[i].fd);
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    continue;
                }

                buf[len] = 0;
                if (strncmp(buf, "SUB", 3) == 0)
                {
                    char *topic_name = buf + 4;
                    subscribe(fds[i].fd, topic_name);
                    printf("Client %d subscribed to topic '%s'\n", fds[i].fd, topic_name);
                }
                else if (strncmp(buf, "PUB", 3) == 0)
                {
                    char *topic_name = buf + 4;
                    char *message = strchr(topic_name, ' ');
                    if (message)
                    {
                        *message = 0;
                        message++;
                        publish(topic_name, message);
                        printf("Client %d published to topic '%s': %s\n", fds[i].fd, topic_name, message);
                    }
                }
                else if (strncmp(buf, "UNSUB", 5) == 0)
                {
                    char *topic_name = buf + 6;
                    int idx = find_topic(topic_name);
                    if (idx != -1)
                    {
                        for (int j = 0; j < topics[idx].count; j++)
                        {
                            if (topics[idx].fds[j] == fds[i].fd)
                            {
                                for (int k = j; k < topics[idx].count - 1; k++)
                                {
                                    topics[idx].fds[k] = topics[idx].fds[k + 1];
                                }
                                topics[idx].count--;
                                printf("Client %d unsubscribed from topic '%s'\n", fds[i].fd, topic_name);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    close(listener);
}