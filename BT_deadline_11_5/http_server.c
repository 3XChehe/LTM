#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define NUM_PROCESSES 4
#define PORT 8080

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
    addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 10))
    {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server listening on port %d with %d processes...\n", PORT, NUM_PROCESSES);

    for (int i = 0; i < NUM_PROCESSES; i++)
    {
        if (fork() == 0)
        {
            char buf[1024];
            while (1)
            {
                int client = accept(listener, NULL, NULL);
                if (client == -1)
                    continue;

                printf("New client accepted in process %d (socket: %d)\n", getpid(), client);

                int ret = recv(client, buf, sizeof(buf) - 1, 0);
                if (ret > 0)
                {
                    buf[ret] = 0;
                    printf("--- HTTP Request (PID %d) ---\n%s\n--------------------------\n", getpid(), buf);

                    char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Hello, World!</h1><p>Processed by PID %d</p></body></html>";
                    char response[2048];
                    sprintf(response, msg, getpid());

                    send(client, response, strlen(response), 0);
                }

                close(client);
            }
            exit(0);
        }
    }

    getchar();

    killpg(0, SIGKILL);

    close(listener);
    return 0;
}