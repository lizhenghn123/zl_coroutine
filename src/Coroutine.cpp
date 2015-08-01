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

Coroutine gCoroutine;

Coroutine::Coroutine()
    : coroSequence_(0)
    , runningId_(0)
{
    getcontext(&mainContext_);
    //mainContext_.uc_stack.ss_size = 16 * 1024;
    //if ((mainContext_.uc_stack.ss_sp = malloc(mainContext_.uc_stack.ss_size)) == NULL)
    //    exit(1);
    //mainContext_.uc_stack.ss_flags = 0;
}

Coroutine::~Coroutine()
{
    for(auto i = coros_.begin(); i != coros_.end(); ++i)
    {
        delete i->second;
    }
}

CoroId Coroutine::create(const CoroutineFunction& func)
{
    Coro* c = new Coro(func, ++coroSequence_);
    coros_.insert(std::make_pair(coroSequence_, c));
    printf("create coro %ld\n", coroSequence_);
    return coroSequence_;
}

void Coroutine::corofunc(uint32_t low32, uint32_t hi32)
{
    uintptr_t ptr = (uintptr_t)low32 | ((uintptr_t)hi32 << 32);
    Coroutine* mgr = (Coroutine*)(ptr);

    Coro* c = mgr->coros_[mgr->runningId_];
    assert(c);
    assert(c->id_ == mgr->runningId_);
    c->func_();
    mgr->runningId_ = 0;
    c->status_ = COROUTINE_DEAD;
    mgr->deleteCoroutine(c);
}

bool Coroutine::resume(CoroId id)
{
    assert(runningId_ == 0);

    Coro *c = getCoroutine(id);
    if (c==NULL)
    {
        return false;
    }

    assert(c->id_ == id);
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
    //if (runningId_ == 0 && c->status_ == COROUTINE_DEAD)
    //{
    //    printf("delete this\n");
    //    deleteCoroutine(c);
    //}
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
    printf("coros is %s empty\n", coros_.empty() ? "" : "not");
    while (!coros_.empty())
    {
        for (auto iter = coros_.begin(); iter != coros_.end(); ++iter)
        {
            //printf("resume %ld\n", (*iter).second->id_);
            resume((*iter).second->id_);
        }
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
    printf("deleteCoroutine %ld\n", co->id_);
    assert(co->status_ == COROUTINE_DEAD);
    coros_.erase(co->id_);
    delete co;
}
