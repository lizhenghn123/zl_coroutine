#include <stdio.h>
#include <unistd.h>
#include <functional>
#include "Coroutine.h"

extern "C"
{
#undef pthread_create
int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine) (void *), void *arg)
{
    *thread = gCoroutine.create(std::bind(start_routine, arg));
    return 0;
}

#undef accept
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    return gCoroutine.accept(sockfd, addr, addrlen);
}

#undef recv
ssize_t recv(int fd, void *buf, size_t len, int flags)
{
    return gCoroutine.recv(fd, buf, len, flags);
}

#undef send
ssize_t send(int fd, const void *buf, size_t len, int flags)
{
    return gCoroutine.send(fd, buf, len, flags);
}

#undef close
int close(int fd)
{
    return gCoroutine.close(fd);
}

}
