#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <arpa/inet.h> //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "Coroutine.h"
using namespace std;


void* echo(void *arg)
{
    int fd = *((int*)arg);
    char buf[1024];

    printf("###### in echo :%d ######\n", fd);
    while (true)
    {
        ssize_t n = recv(fd, buf, sizeof(buf), 0);
        if (n <= 0)
        {
            break;
        }
        buf[n] = '\0';
        printf("echo recv: %s\n", buf);

        n = send(fd, buf, n, 0);
        if (n <= 0)
        {
            break;
        }
    }
    close(fd);
    return NULL;
}

void* listener(void *arg)
{
    printf("server listener .....\n");
    struct sockaddr_in server;
    int srvsock = socket(AF_INET, SOCK_STREAM, 0);
    int optval = 1;
    setsockopt(srvsock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval);

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(9999);

    fcntl(srvsock, F_SETFL, fcntl(srvsock, F_GETFL) | O_NONBLOCK);
    bind(srvsock, (struct sockaddr *)&server, sizeof(server));

    listen(srvsock, 200);
    while (true)
    {
        printf("server listener listening .....\n");
        int fd = accept(srvsock, NULL, NULL);
        printf("server listener accept[%d].....\n", fd);

        if (fd != -1)
        {
            fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
            if (1)
            {
                pthread_t id;
                pthread_create(&id, NULL, echo, &fd);
            }
            else
            {
                echo(&fd);
            }
        }
    }
    return NULL;
}

int main()
{    
    pthread_t listenthread;
    pthread_create(&listenthread, NULL, listener, NULL);

    gCoroutine.run();

    printf("###############\n");
    return 0;
}
