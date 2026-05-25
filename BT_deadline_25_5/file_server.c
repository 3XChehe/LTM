#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 1050
#define RES_DIR "/mnt/d/Study/LTM/BT_deadline_25_5/"

void signal_handler(int sig)
{
    int pid = wait(NULL);
    printf("Child process terminated: %d\n", pid);
}

int send_file_list(int client)
{
    DIR *dir = opendir(RES_DIR);
    if (dir == NULL)
    {
        send(client, "ERROR No files to download \r\n", 30, 0);
        return -1;
    }

    struct dirent *entry;
    struct stat st;
    char filepath[512];
    char *filenames[256];
    int count = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        sprintf(filepath, "%s/%s", RES_DIR, entry->d_name);
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode))
        {
            filenames[count] = strdup(entry->d_name);
            count++;
        }
    }

    if (count == 0)
    {
        send(client, "ERROR No files to download \r\n", 30, 0);
        closedir(dir);
        return -1;
    }

    char header[64];
    sprintf(header, "OK %d\r\n", count);
    send(client, header, strlen(header), 0);

    for (int i = 0; i < count; i++)
    {
        send(client, filenames[i], strlen(filenames[i]), 0);
        send(client, "\r\n", 2, 0);
        free(filenames[i]);
    }

    send(client, "\r\n", 2, 0);

    printf(">> List of %d files sent to client\n", count);
    closedir(dir);
    return 0;
}

void receive_and_send_file(int client)
{
    char filename[256];
    char filepath[512];
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(filename, 0, sizeof(filename));
        int ret = recv(client, filename, sizeof(filename) - 1, 0);

        if (ret <= 0)
            break;

        filename[strcspn(filename, "\r\n")] = 0;

        if (strlen(filename) == 0)
            continue;

        sprintf(filepath, "%s/%s", RES_DIR, filename);
        FILE *f = fopen(filepath, "rb");

        if (f == NULL)
        {
            char *err_msg = "ERROR File not found. Please send another name\r\n";
            send(client, err_msg, strlen(err_msg), 0);
            printf("!! File not found: %s\n", filename);
            continue;
        }

        struct stat st;
        stat(filepath, &st);
        long filesize = st.st_size;

        char header[64];
        sprintf(header, "OK %ld\r\n", filesize);
        send(client, header, strlen(header), 0);

        printf(">> Sending file: %s (%ld bytes)\n", filename, filesize);
        int n;
        while ((n = fread(buffer, 1, BUFFER_SIZE, f)) > 0)
        {
            send(client, buffer, n, 0);
        }

        fclose(f);
        printf(">> Done. Closing connection with client\n");
        break;
    }
}

int main()
{
    mkdir(RES_DIR, 0777);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)))
    {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5))
    {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("File Server listening on port %d...\n", SERVER_PORT);
    signal(SIGCHLD, signal_handler);

    while (1)
    {
        int client = accept(listener, NULL, NULL);
        if (client < 0)
            continue;

        printf("New client connected: %d\n", client);

        if (fork() == 0)
        {
            close(listener);
            if (send_file_list(client) == 0)
            {
                send(client, "Enter filename you want to download\r\n", 40, 0);
                receive_and_send_file(client);
            }

            close(client);
            exit(0);
        }
        close(client);
    }

    close(listener);
    return 0;
}