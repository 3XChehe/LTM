#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

void signal_handler(int sig)
{
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int check_login(char *user, char *pass)
{
    FILE *f = fopen("users.txt", "r");
    if (f == NULL)
        return 0;

    char line[64], cred[64];
    sprintf(cred, "%s %s", user, pass);

    while (fgets(line, sizeof(line), f))
    {
        if (line[strlen(line) - 1] == '\n')
            line[strlen(line) - 1] = 0;
        if (line[strlen(line) - 1] == '\r')
            line[strlen(line) - 1] = 0;
        if (strcmp(line, cred) == 0)
        {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

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

    signal(SIGCHLD, signal_handler);

    printf("Server listening on port 8080...\n");

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        printf("New client connected: %d\n", client);

        if (fork() == 0)
        {
            close(listener);

            char buf[256];
            int logged_in = 0;

            send(client, "Hay dang nhap theo cu phap 'user pass':\n", 40, 0);

            while (1)
            {
                int len = recv(client, buf, sizeof(buf) - 1, 0);
                if (len <= 0)
                    break;

                buf[len] = 0;
                if (buf[len - 1] == '\n')
                    buf[len - 1] = 0;
                if (buf[strlen(buf) - 1] == '\r')
                    buf[strlen(buf) - 1] = 0;

                printf("Received from client %d: %s\n", client, buf);

                if (!logged_in)
                {
                    char user[32], pass[32];
                    int n = sscanf(buf, "%s %s", user, pass);

                    if (n != 2)
                    {
                        send(client, "Sai cu phap. Hay dang nhap lai.\n", 32, 0);
                        printf("Client %d: Invalid syntax (n=%d)\n", client, n);
                    }
                    else if (check_login(user, pass))
                    {
                        logged_in = 1;
                        send(client, "Login OK. Nhap lenh:\n", 21, 0);
                        printf("Client %d: Logged in successfully\n", client);
                    }
                    else
                    {
                        send(client, "Sai username hoac password.\n", 28, 0);
                        printf("Client %d: Login failed\n", client);
                    }
                }
                else
                {
                    char filename[64];
                    char cmd[512];
                    char out_buf[512];

                    sprintf(filename, "out_%d.txt", getpid());

                    sprintf(cmd, "%s > %s 2>&1", buf, filename);

                    system(cmd);

                    FILE *f = fopen(filename, "r");
                    if (f)
                    {
                        while (fgets(out_buf, sizeof(out_buf), f) != NULL)
                        {
                            send(client, out_buf, strlen(out_buf), 0);
                        }
                        fclose(f);
                    }
                    else
                    {
                        send(client, "Loi thuc thi lenh.\n", 19, 0);
                    }
                }
            }
            printf("Client %d disconnected. Child process %d exiting.\n", client, getpid());
            close(client);
            exit(0);
        }

        close(client);
    }

    close(listener);
    return 0;
}