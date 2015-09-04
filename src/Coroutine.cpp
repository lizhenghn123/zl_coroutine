#include "Coroutine.h"
#include <stdio.h>
#include <assert.h>
#include <ucontext.h>
#include <string.h>   // for memset
#include <stdlib.h>   // for malloc
#include <stddef.h>   // for ptrdiff_t
#ifdef OS_WINDOWS
#else
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#endif
#include <sys/epoll.h>
#include <dlfcn.h>    // for dlsym
#include <fcntl.h>    // for fcntl
#include <algorithm>

class Coro
{
public:
    Coro(const CoroutineFunction& func, CoroId id);
    ~Coro();

    void saveStack(char* top);

    CoroId            id_;
    CoroutineFunction func_;

    CoroStatus        status_;
    ucontext_t        context_;
    char*             stack_;
    ptrdiff_t         capacity_;
    ptrdiff_t         size_;
};

Coro::Coro(const CoroutineFunction& f, CoroId id)
    : id_(id)
    , func_(f)
    , status_(COROUTINE_READY)
    , stack_(NULL)
    , capacity_(0)
    , size_(0)
{

}


Coro::~Coro()
{
    if(stack_)
    {
        ::free(stack_);
    }
}

void Coro::saveStack(char* top)
{
    char dummy = 0;
    assert(top - &dummy <= static_cast<ptrdiff_t>(CORO_STACKSIZE));
    if (capacity_ < top - &dummy)
    {
        ::free(stack_);
        capacity_ = top - &dummy;
        stack_ = static_cast<char*>(::malloc(capacity_));
    }
    size_ = top - &dummy;
    ::memcpy(stack_, &dummy, size_);
}

struct Socket
{
    int    fd_;
    Coro*  coro_;
};

Coroutine gCoroutine;

typedef int(*sysAccept)(int, struct sockaddr *, socklen_t *);
typedef ssize_t(*sysRecv)(int fd, void *buf, size_t len, int flags);
typedef ssize_t(*sysSend)(int fd, const void *buf, size_t len, int flags);
typedef int(*sysClose)(int fd);

static sysAccept gSysAccept;
static sysRecv   gSysRecv;
static sysSend   gSysSend;
static sysClose  gSysClose;

Coroutine::Coroutine()
    : coroSequence_(0)
    , runningId_(0)
{
    getcontext(&mainContext_);
    //mainContext_.uc_stack.ss_size = 16 * 1024;
    //if ((mainContext_.uc_stack.ss_sp = malloc(mainContext_.uc_stack.ss_size)) == NULL)
    //    exit(1);
    //mainContext_.uc_stack.ss_flags = 0;

    socks_.resize(256, NULL);
    //activeCoros_.resize(256, NULL);

    epollfd_ = epoll_create(1024);

    gSysAccept = (sysAccept)dlsym(RTLD_NEXT, "accept");
    gSysRecv = (sysRecv)dlsym(RTLD_NEXT, "recv");
    gSysSend = (sysSend)dlsym(RTLD_NEXT, "send");
    gSysClose = (sysClose)dlsym(RTLD_NEXT, "close");
}

Coroutine::~Coroutine()
{
    for(auto i = coros_.begin(); i != coros_.end(); ++i)
    {
        delete i->second;
    }

    //assert(activeCoros_.size() == socks_.size());
    for (size_t i = 0; i < activeCoros_.size(); ++i)
    {
        if (activeCoros_[i])
        {
            delete activeCoros_[i];
        }
        if (socks_[i])
        {
            delete socks_[i];
        }
    }
}

CoroId Coroutine::create(const CoroutineFunction& func)
{
    Coro* c = new Coro(func, ++coroSequence_);
    coros_.insert(std::make_pair(coroSequence_, c));
    activeCoros_.push_back(c);
    printf("create coro %ld\n", coroSequence_);
    return coroSequence_;
}

void Coroutine::corofunc(uint32_t low32, uint32_t hi32)
{
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    Coroutine* mgr = (Coroutine*)(ptr);
    assert(mgr);

    Coro* c = mgr->coros_[mgr->runningId_];
    assert(c);
    assert(c->id_ == mgr->runningId_);
    printf("coro[%lu] start callback\n", c->id_);
    c->func_();
    c->status_ = COROUTINE_DEAD;
    mgr->runningId_ = 0;
}

bool Coroutine::resume(CoroId id)
{
    printf("resume %lu, %lu\n", id, runningId_);
    //assert(runningId_ == 0);

    Coro *c = getCoroutine(id);
    if (c==NULL)
    {
        printf("It is impossible\n");
        return false;
    }

    //assert(c->id_ == id);
    switch(c->status_)
    {
    case COROUTINE_READY:
    {
        ::getcontext(&c->context_);
        c->context_.uc_stack.ss_sp = stack_;
        c->context_.uc_stack.ss_size = CORO_STACKSIZE;
        c->context_.uc_link = &mainContext_;

        c->status_ = COROUTINE_RUNNING;
        runningId_ = id;
        uintptr_t ptr = (uintptr_t)this;
        ::makecontext(&c->context_, (void (*)(void))Coroutine::corofunc, 2, (uint32_t)ptr, (uint32_t)(ptr >> 32));
        ::swapcontext(&mainContext_, &c->context_);
        break;
    }
    case COROUTINE_SUSPEND:
    {
        ::memcpy(stack_ + CORO_STACKSIZE - c->size_, c->stack_, c->size_);
        runningId_ = id;
        c->status_ = COROUTINE_RUNNING;
        ::swapcontext(&mainContext_, &c->context_);
        break;
    }
    default:
        printf("coro[%ld] status [%d]\n", id, c->status_);
        //assert(0);
        return false;
    }

    //printf("%s, %d, [%d][%d][%d]\n", __FILE__, __LINE__, runningId_, c->id_, c->status_);
    if (runningId_ == 0 && c->status_ == COROUTINE_DEAD)
    {
        deleteCoroutine(c);
    }
    return true;
}

bool Coroutine::yield() 
{
    if (runningId_ == 0)
        return false;

    Coro* c = coros_[runningId_];
    assert(c->id_ == runningId_);
    c->saveStack(stack_ + CORO_STACKSIZE);
    c->status_ = COROUTINE_SUSPEND;
    runningId_ = 0;
    ::swapcontext(&c->context_, &mainContext_);

    return true;
}

void Coroutine::run()
{
    printf("coros is %s empty[%ld]\n", coros_.empty() ? "" : "not", coros_.size());
    printf("activeCoros_ is %s empty[%ld]\n", activeCoros_.empty() ? "" : "not", activeCoros_.size());
    std::vector<Coro*>     vecCoros;
    while (true)
    {
        vecCoros.clear();
        ////printf("poll.......\n");
        //int nfds = poll(1000, &activeCoro);   // 这里还是需要唤醒机制的，也就是resume调用中创建了coro，然后必须等到poll之后才能调用；
        //// 可以改为poll传个vector，预先调用，然后再调用active coro， 类似于muduo中pending functor
        //if (nfds > 0)
        //{
        //    for (auto iter = activeCoro.begin(); iter != activeCoro.end(); ++iter)
        //    {
        //        assert(*iter);
        //        printf("resume %ld\n", (*iter)->id_);
        //        resume((*iter)->id_);
        //    }
        //}
        //
        //printf("activeCoros_ is %s empty[%ld]\n", activeCoros_.empty() ? "" : "not", activeCoros_.size());
        //if (!activeCoros_.empty())
        //{
        //    activeCoro.swap(activeCoros_);
        //    for (auto iter = activeCoro.begin(); iter != activeCoro.end(); ++iter)
        //    {
        //        assert(*iter);
        //        printf("resume %ld\n", (*iter)->id_);
        //        resume((*iter)->id_);
        //    }
        //}
        int nfds = poll(1000, NULL);
        (void)nfds;
        if (!activeCoros_.empty())
        {
            vecCoros.swap(activeCoros_);
            for (auto iter = vecCoros.begin(); iter != vecCoros.end(); ++iter)
            {
                assert(*iter);
                printf("resume %ld\n", (*iter)->id_);
                resume((*iter)->id_);
            }
        }
        //activeCoros_.clear();
    }
}

CoroId Coroutine::running() const
{
    return runningId_;
}

CoroStatus Coroutine::status(CoroId id) const
{
    auto it = coros_.find(id);
    if(it == coros_.end())
    {
        return COROUTINE_DEAD; 
    }
    return (*it).second->status_;
}

Coro* Coroutine::getCoroutine(CoroId id) const
{
    auto iter = coros_.find(id);
    if (iter == coros_.end())
        return NULL;
    return iter->second;
}

void Coroutine::deleteCoroutine(Coro* co)
{
    printf("delete coro[%lu]\n", co->id_);
    assert(co->status_ == COROUTINE_DEAD);
    coros_.erase(co->id_);
    delete co;
}

int Coroutine::poll(int timeoutMs, std::vector<Coro*>* activeCoro)
{
    epoll_event events[1024];
    int nfds = epoll_wait(epollfd_, events, 1024, timeoutMs); //此处写为-1会阻塞死，改成1000
    if (nfds > 0)
    {
        printf("epoll happened [%d] events\n", nfds);
        for (int i = 0; i < nfds; ++i)
        {
            if (events[i].events & (EPOLLIN | EPOLLOUT))
            {
                Socket* sock = static_cast<Socket*>(events[i].data.ptr);
                printf("epoll event[%p][%lu]\n", sock->coro_, sock->coro_->id_);
                if (std::find(activeCoros_.begin(), activeCoros_.end(), sock->coro_) == activeCoros_.end())
                    activeCoros_.push_back(sock->coro_);
                else
                    printf("$$$$   alreay push [%lu]\n", sock->coro_->id_);
            }
        }
    }
    else if (nfds == 0)
    {
        printf("epoll happened nothing\n");
    }
    else
    {
        printf("epoll error: %s:%d\n", strerror(errno), errno);
    }
    return nfds;
}

int Coroutine::accept(int fd, struct sockaddr *addr, socklen_t* addrlen)
{
    Coro* coro = coros_[runningId_];
    Socket* sock = getSocket(fd);
    assert(coro);
    assert(sock);

    epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.ptr = (void*)sock;
    ev.events = EPOLLIN;

    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        printf("epoll_ctl error:%s\n", strerror(errno));
        return -1;
    }

swap:
    coro->status_ = COROUTINE_SUSPEND;
    swapcontext(&coro->context_, &mainContext_);    // 暂停本协程的执行(刚加入到epoll)

    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
    int s;
    do
    {
        s = gSysAccept(fd, addr, addrlen);
        if (s < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)         // again
            {
                printf("eagain\n");
                goto swap;
            }
            else if (errno == EINTR)                              // 这种属于打断， 应该continue
            {
                continue;
            }
            else  
            {
                printf("accept errno: %s\n", strerror(errno));
                return -1;
            }
        }
        fcntl(s, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);   // 设置非阻塞
        break;
    } while (true);

    return s;
}

ssize_t    Coroutine::recv(int fd, void* buf, size_t len, int flags)
{
    Coro* coro = coros_[runningId_];
    Socket* sock = getSocket(fd);
    assert(coro);
    assert(sock);

    epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.ptr = (void*)sock;
    ev.events = EPOLLIN;
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        printf("recv add epoll error: %s\n", strerror(errno));
        return -1;
    }

    ssize_t ret = 0;
swap:
    coro->status_ = COROUTINE_SUSPEND;
    swapcontext(&coro->context_, &mainContext_);    // 暂停本协程的执行(刚加入到epoll)

    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
    while (ret < (ssize_t)len)
    {
        ssize_t nbytes = gSysRecv(fd, (char*)buf + ret, len - ret, flags);
        if (nbytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                goto swap;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                printf("recv error [%d], [%s]\n", fd, strerror(errno));
                return -1;
            }
        }

        if (nbytes == 0)
        {
            return 0;   // 表示对方已关闭
        }

        ret += nbytes;
        if (nbytes < (ssize_t)len - ret)
        {
            break;
        }
    }
    return ret;
}

ssize_t    Coroutine::send(int fd, const void* buf, size_t len, int flags)
{
    printf("send [%d] [%s]\n", fd, (const char*)buf);
    Coro* coro = coros_[runningId_];
    Socket* sock = getSocket(fd);
    assert(coro);
    assert(sock);
    printf("send $$$$$$$$$$$$$  1\n");
    epoll_event ev;
    memset(&ev, 0, sizeof(struct epoll_event));
    ev.data.ptr = (void*)sock;
    ev.events = EPOLLOUT;
    if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, fd, &ev) != 0)
    {
        printf("send add epoll error: %s\n", strerror(errno));
        return -1;
    }
    printf("send $$$$$$$$$$$$$  2\n");
    ssize_t ret = 0;
swap:
    coro->status_ = COROUTINE_SUSPEND;
    swapcontext(&coro->context_, &mainContext_);    // 暂停本协程的执行(刚加入到epoll)
    printf("send $$$$$$$$$$$$$  3\n");
    epoll_ctl(epollfd_, EPOLL_CTL_DEL, fd, &ev);
    while (ret < (ssize_t)len)
    {
        ssize_t nbytes = gSysSend(fd, (char*)buf + ret, len - ret, flags | MSG_NOSIGNAL);
        printf("send [%d] [%ld]\n", fd, nbytes);
        if (nbytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                goto swap;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            else
            {
                printf("recv error [%d], [%s]\n", fd, strerror(errno));
                return -1;
            }
        }

        if (nbytes == 0)
        {
            return 0;
        }

        ret += nbytes;
        if (ret == (ssize_t)len)
        {
            break;
        }
    }

    return ret;
}

int        Coroutine::close(int fd)
{
    if (socks_[fd])   // 也应该清理coro
    {
        delete socks_[fd];
        socks_[fd] = NULL;
    }
    return gSysClose(fd);
}

Socket*    Coroutine::getSocket(int fd)
{
    if (fd >= static_cast<int>(socks_.size()))
    {
        printf("socks_.resize [%ld] [%d]\n", socks_.size(), fd);
        socks_.resize(socks_.size() * 2);
    }

    Socket* sock = socks_[fd];
    if (sock == NULL)
    {
        sock = new Socket();
        sock->fd_ = fd;
        sock->coro_ = coros_[runningId_];

        socks_[fd] = sock;
    }

    return sock;
}