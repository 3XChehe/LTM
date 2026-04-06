#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

void build_email(char *dest, const char *fullname, const char *mssv)
{
    char name[256];
    strcpy(name, fullname);
    char *words[20];
    int count = 0;

    char *token = strtok(name, " ");
    while (token != NULL && count < 20)
    {
        words[count++] = token;
        token = strtok(NULL, " ");
    }

    if (count == 0)
    {
        strcpy(dest, "invalid@sis.hust.edu.vn");
        return;
    }

    char result[512] = {0};
    strcat(result, words[count - 1]); // tên

    strcat(result, ".");
    for (int i = 0; i < count - 1; i++)
    {
        char c = words[i][0];
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';

        strncat(result, &c, 1);
    }
    if (strlen(mssv) > 2)
    {
        strcat(result, mssv + 2);
    }
    strcat(result, "@sis.hust.edu.vn");
    strcpy(dest, result);
}

int main()
{
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1)
    {
        perror("socket() failed");
        return 1;
    }

    // Chuyen socket listener sang non-blocking
    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)))
    {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

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

    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");

    int clients[64];
    int nclients = 0;

    char buf[64][256];
    int len[64];

    int sendstage[64];
    memset(sendstage, 0, sizeof(sendstage));
    char email[64][256];
    char temp_email[64][256];
    memset(email, 0, sizeof(email));
    memset(temp_email, 0, sizeof(temp_email));

    while (1)
    {
        // Chap nhan ket noi
        int client = accept(listener, NULL, NULL);
        if (client == -1)
        {
            if (errno == EWOULDBLOCK)
            {
                // Loi do dang cho ket noi
                // Bo qua
            }
            else
            {
                // Loi khac
            }
        }
        else
        {
            printf("New client accepted: %d\n", client);
            clients[nclients] = client;
            nclients++;
            ul = 1;
            ioctl(client, FIONBIO, &ul);
        }
        for (int i = 0; i < nclients; i++)
        {
            if (sendstage[i] == 0)
            {
                strcat(email[i], "email:");
                send(clients[i], "Tên: ", strlen("Tên:"), 0);
                sendstage[i] = 1;
            }
            else if (sendstage[i] == 1 && len[i] > 0)
            {
                strcpy(temp_email[i], buf[i]);
                send(clients[i], "MSSV: ", strlen("MSSV: "), 0);
                memset(buf[i], 0, sizeof(buf[i]));
                sendstage[i] = 2;
            }
            else if (sendstage[i] == 2 && len[i] > 0)
            {
                build_email(email[i], temp_email[i], buf[i]);
                send(clients[i], email[i], strlen(email[i]), 0);
                send(clients[i], "\n", 1, 0);
                memset(buf[i], 0, sizeof(buf[i]));
                sendstage[i] = 0;
            }

            len[i] = recv(clients[i], buf[i], sizeof(buf[i]), 0);
            if (len[i] == -1)
            {
                if (errno == EWOULDBLOCK)
                {
                    // Loi do cho du lieu
                    // Bo qua
                }
                else
                {
                    continue;
                }
            }
            else
            {
                if (len[i] == 0)
                    continue;
                buf[i][strcspn(buf[i], "\r\n")] = 0; // Loai bo newline
                buf[i][len[i]] = 0;
                printf("Received from %d: %s\n", clients[i], buf[i]);
            }
        }
    }

    close(listener);
    return 0;
}