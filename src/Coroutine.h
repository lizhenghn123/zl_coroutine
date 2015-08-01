#ifndef ZL_COROUTINE_H
#define ZL_COROUTINE_H
#include <stdint.h>
#include <ucontext.h>
#include <unistd.h>
#include <map>
#include <functional>    // for std::function
#include "OsDefine.h"
#include <arpa/inet.h>   //inet_addr
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/socket.h>

//default coroutine stack size
#define CORO_STACKSIZE (1024 * 1024)

enum CoroStatus
{
    COROUTINE_RUNNING,       /// coroutine is running
    COROUTINE_READY,         /// coroutine is created but has not started
    COROUTINE_SUSPEND,       /// coroutine is yield
    COROUTINE_DEAD           /// coroutine has finished
};

typedef size_t CoroId;
typedef std::function<void ()> CoroutineFunction;

class Coro;

// FIXME: noncopyable
class Coroutine
{
public:
    Coroutine();
    ~Coroutine();

public:
    /// create a new coroutine with func
    CoroId create(const CoroutineFunction& func);

    /// start or continue the execution of coroutine coro
    /// return true iff success
    bool resume(CoroId id);

    /// suspend the execution of calling coroutine
    /// return true iff success
    bool yield();

    /// run forever
    void run();

    /// return status of coro
    CoroStatus status(CoroId id) const;

    /// return current running coroutin or 0 when called by main thread
    CoroId running() const;

public:
    int        accept(int sockfd, struct sockaddr *addr, socklen_t* addrlen);
    ssize_t    recv(int fd, void* buf, size_t len, int flags);
    ssize_t    send(int fd, const void* buf, size_t len, int flags);
    int        close(int fd);

private:
    static void corofunc(uint32_t low32, uint32_t hi32);

    Coro*   getCoroutine(CoroId id) const;
    void    deleteCoroutine(Coro* co);

private:
    char        stack_[CORO_STACKSIZE];
    CoroId      coroSequence_;              /// coro id
    CoroId      runningId_;                 /// current running coro id
    ucontext_t  mainContext_;
    
    std::map<CoroId, Coro*> coros_;
};

extern Coroutine gCoroutine;

#endif  /* ZL_COROUTINE_H */