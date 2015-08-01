#include <iostream>
#include <stdio.h>
#include <assert.h>
#include "Coroutine.h"
using namespace std;

// see http://blog.csdn.net/siddontang/article/details/20544437

// see https://github.com/cloudwu/coroutine

extern Coroutine gCoroutine;

namespace test_coro1
{
    void foo(int num)
    {
        for (int i = 0; i < 5;++i)
        {
            printf("coroutine %d : %d \n", gCoroutine.running(), num + i);
            gCoroutine.yield();
        }
    }

    void test()
    {
        int co1 = gCoroutine.create(std::bind(foo, 5));
        int co2 = gCoroutine.create(std::bind(foo, 100));
        printf("main start\n");
        while (gCoroutine.status(co1) != COROUTINE_DEAD && gCoroutine.status(co2) != COROUTINE_DEAD)
        {
            gCoroutine.resume(co1);
            gCoroutine.resume(co2);
        }
        printf("main end\n");
    }
}

namespace test_coro2
{
    CoroId co1;
    CoroId co2;

    void func1()
    {
        int a = 10;
        printf("hello func1\n");

        CoroId co = gCoroutine.running();
        if (co != co1)
        {
            printf("error, co1 must run\n");
            exit(1);
        }

        gCoroutine.yield();               // 此线程 yield

        co = gCoroutine.running();        // 继续run
        if (co != co1)
        {
            printf("error, co1 must run\n");
            exit(1);
        }

        printf("hello end func1\t %d", a);
    }

    void func2()
    {
        int a = 100;
        printf("hello func2\n");

        CoroId co = gCoroutine.running();
        if (co != co2)
        {
            printf("error, co2 must run\n");
            exit(1);
        }

        CoroStatus s = gCoroutine.status(co1);
        if (s != COROUTINE_SUSPEND)
        {
            printf("error, co1 must suspeneded\n");
            exit(1);
        }

        cout << __LINE__ << "\n";
        gCoroutine.resume(co1);
        cout << __LINE__ << "\n";

        s = gCoroutine.status(co1);
        if (s != COROUTINE_DEAD)
        {
            printf("error, co1 must daed\n");
            exit(1);
        }

        co = gCoroutine.running();
        printf("%s, %d, [%d][%d]\n", __FILE__, __LINE__, co, co2);
        if (co != co2)      // todo 这里会有问题，co2 resume co1， co1 运行完成后， 并没有再设置当前co为co2
        {
            printf("error, co2 must run\n");
            exit(1);
        }

        gCoroutine.yield();

        printf("hello end func2\t %d", a);
    }
    void test()
    {
        printf("hello func\n");

        co1 = gCoroutine.create(std::bind(&func1));

        gCoroutine.resume(co1);

        co2 = gCoroutine.create(std::bind(&func2));

        printf("%s, %d, [%d][%d]\n", __FILE__, __LINE__, co1, co2);

        gCoroutine.resume(co2);

        gCoroutine.resume(co2);

        CoroStatus s = gCoroutine.status(co2);
        if (s != COROUTINE_DEAD)
        {
            printf("error, co2 must dead\n");
            exit(1);
        }

        printf("hello end fun\n");
    }
}
int main()
{    
    test_coro1::test();
    printf("###############\n");
    //test_coro2::test();   // core
    return 0;
}
