#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in local_addr, dest_addr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0)
    {
        perror("socket");
        exit(1);
    }

    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(port_s);

    if (bind(sockfd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d);
    inet_pton(AF_INET, ip_d, &dest_addr.sin_addr);

    printf("UDP Chat started on port %d\n", port_s);

    struct pollfd fds[2];

    fds[0].fd = sockfd;
    fds[0].events = POLLIN;

    fds[1].fd = STDIN_FILENO;
    fds[1].events = POLLIN;

    while (1)
    {
        int ret = poll(fds, 2, -1);
        if (ret < 0)
        {
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            int len = recvfrom(sockfd, buffer, BUFFER_SIZE - 1, 0, NULL, NULL);
            if (len > 0)
            {
                buffer[len] = '\0';
                printf("\n[RECV]: %s\n", buffer);
            }
        }
        if (fds[1].revents & POLLIN)
        {
            if (fgets(buffer, BUFFER_SIZE, stdin) != NULL)
            {
                sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            }
        }
    }

    close(sockfd);
    return 0;
}