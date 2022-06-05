/*

Lola Robins
June 2022

"Loomis-HTTP"
Simple http server to handle get request for files within directory

Created for educational purposes

*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <dirent.h>

bool ipv6, lock, verbose;
volatile bool active = true;
char address[40] = "127.0.0.1", directory[512] = "web";
uint16_t port = 80;
int sid;

static void *client_handle(void *arg)
{
    // take the id
    int cid = *((int *)arg);
    free(arg);

    // receive info
    char buffer[65535];
    int bytes = read(cid, buffer, 65535);

    if (bytes < 1)
        return NULL;

    // parse query
    char *request = strtok(buffer, "\n");
    char *type = strtok(request, " ");
    char *path = strtok(NULL, " ");
    path = strtok(path, "?");

    if (!strcasecmp(type, "GET"))
    {
        char file_path[strlen(directory) + strlen(path) + 1];
        strcpy(file_path, directory);
        strcat(file_path, path);

        DIR *dir = opendir(file_path);
        if (dir)
        {
            strcat(file_path, "/index.html");
            closedir(dir);
        }

        FILE *file = fopen(file_path, "r");
        if (file)
        {
            // show existing file in webserver directory
            struct stat file_stat;
            stat(file_path, &file_stat);
            int size = file_stat.st_size;

            char buffer[512 + size];
            char file_buffer[size];

            fread(file_buffer, size, 1, file);
            fclose(file);

            // support rendering of common file formats in browser
            char type[48];
            char *ext = strrchr(file_path, '.') + 1;
            type[0] = '\0';

            if (!strcasecmp(ext, "css"))
                strcpy(type, "text/css; charset=utf-8");
            else if (!strcasecmp(ext, "js"))
                strcpy(type, "text/javascript; charset=utf-8");
            else if (!strcasecmp(ext, "ico"))
                strcpy(type, "image/x-icon");
            else if (!strcasecmp(ext, "png"))
                strcpy(type, "image/png");
            else if (!strcasecmp(ext, "html") || !strcasecmp(ext, "htm"))
                strcpy(type, "text/html; charset=utf-8");
            else if (!strcasecmp(ext, "pdf"))
                strcpy(type, "application/pdf");
            
            // prepare & send
            if (type[0] == '\0')
                sprintf(buffer, "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", size);
            else
                sprintf(buffer, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n", type, size);

            int header_size = strlen(buffer);
            memcpy(buffer + header_size, file_buffer, size);

            send(cid, buffer, header_size + size, 0);
            close(cid);

            if (verbose)
                printf("%sINFO:%s Sending client content of '%s'.\n", "\033[0;36m", "\033[0m", file_path);
        }
        else
        {
            // send 404
            char path_404[strlen(directory) + strlen("/404.html") + 1];
            strcpy(path_404, directory);
            strcat(path_404, "/404.html");

            file = fopen(path_404, "r");

            struct stat file_stat;
            stat(path_404, &file_stat);
            int size = file_stat.st_size;

            char buffer[256 + size];
            char file_buffer[size];

            fread(file_buffer, size, 1, file);
            fclose(file);
            sprintf(buffer, "HTTP/1.1 404 Not Found\r\nContent-Type: %s; charset=utf-8\r\nContent-Length: %d\r\n\r\n%s", "text/html", size, file_buffer);

            send(cid, buffer, strlen(buffer), 0);
            close(cid);

            if (verbose)
                printf("%sINFO:%s Sending client 404 Not Found for trying to access '%s'.\n", "\033[0;36m", "\033[0m", file_path);
        }
    }
    else
    {
        // unknown request, close socket
        close(cid);
    }

    return NULL;
}

static void *command(void *arg)
{
    char buffer[16];

    // allows quit at any point with :q
    while (active)
    {
        scanf("%s", buffer);

        if (strlen(buffer) <= 1)
            continue;

        char *arg0 = strtok(buffer, " ");
        arg0 = strtok(arg0, "\n");

        if (!strcasecmp(arg0, ":q") || !strcasecmp(arg0, "quit"))
        {
            printf("%sINFO:%s Shutting down server.\n", "\033[0;36m", "\033[0m");
            active = false;
            close(sid);
        }
        else
            printf("%sERROR:%s Unrecognized command.\n", "\033[0;31m", "\033[0m");
    }

    return NULL;
}

int main(int argc, char *argv[])
{
    // read arguments
    for (int i = 0; i < argc; i++)
    {
        if (argv[i][0] != '-')
            continue;

        switch ((uint8_t)argv[i][1])
        {
        // use ipv6
        case '6':
            ipv6 = true;
            break;
        // port
        case 'p':
            sscanf(argv[i + 1], "%d", (int *)&port);
            i++;
            break;
        // address
        case 'a':
            sscanf(argv[i + 1], "%s", address);
            i++;
            break;
        // directory
        case '-':
            sscanf(argv[i + 1], "%s", directory);
            i++;
            break;
        case 'v':
            verbose = true;
            break;
        }
    }

    // check directory existence
    DIR *dir = opendir(directory);
    if (!dir)
    {
        printf("%sERROR:%s Unable to open directory '%s'.\n", "\033[0;31m", "\033[0m", directory);
        exit(1);
    }
    closedir(dir);

    printf("%sINFO:%s Hosting directory '%s'.\n", "\033[0;36m", "\033[0m", directory);

    // start socket
    struct sockaddr_in address_ipv4;
    struct sockaddr_in6 address_ipv6;

    if (ipv6)
    {
        inet_pton(AF_INET6, address, &(address_ipv6.sin6_addr));
        address_ipv6.sin6_port = htons(port);
        address_ipv6.sin6_family = AF_INET6;

        sid = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
    }
    else
    {
        inet_pton(AF_INET, address, &(address_ipv4.sin_addr));
        address_ipv4.sin_port = htons(port);
        address_ipv4.sin_family = AF_INET;

        sid = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    }

    setsockopt(sid, SOL_SOCKET, SO_REUSEADDR, ((const void *)true), sizeof(int));

    if (bind(sid, (ipv6 ? ((struct sockaddr *)&address_ipv6) : ((struct sockaddr *)&address_ipv4)), sizeof(struct sockaddr)) != 0)
    {
        printf("%sERROR:%s Socket unable to bind to address '%s:%d'.\n", "\033[0;31m", "\033[0m", address, port);
        exit(2);
    }
    else
        printf("%sINFO:%s Socket bound to address '%s:%d'.\n", "\033[0;36m", "\033[0m", address, port);

    listen(sid, 10);

    // listen for quit command
    pthread_t quit_id;
    pthread_create(&quit_id, NULL, &command, NULL);
    pthread_detach(quit_id);

    // accept connections
    int cid = 0;
    while (active)
    {
        struct sockaddr client_address;
        socklen_t size = sizeof(client_address);
        cid = accept(sid, &client_address, &size);

        if (cid == -1)
            continue;

        // pass new id
        int *cid_arg = malloc(sizeof(int));
        if (!cid_arg)
        {
            printf("%sERROR:%s Memory allocation failed\n", "\033[0;31m", "\033[0m");
            exit(3);
        }

        *cid_arg = cid;

        pthread_t tid;
        pthread_create(&tid, NULL, &client_handle, cid_arg);
        pthread_detach(tid);
    }

    close(sid);
}
