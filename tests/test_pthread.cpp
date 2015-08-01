#include <iostream>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include "Coroutine.h"
using namespace std;


namespace test_pthread
{
    void* foo(void *arg)
    {
        int num = *((int*)arg);

        for (int i = 0; i < 5; ++i)
        {
            printf("coroutine %d : %d \n", gCoroutine.running(), num + i);
            gCoroutine.yield();
        }

        return NULL;
    }

    void test()
    {
        int num1 = 5, num2 = 100;

        pthread_t id1;
        pthread_create(&id1, NULL, foo, &num1);

        pthread_t id2;
        pthread_create(&id2, NULL, foo, &num2);

        sleep(1);
        printf("main end\n");
        gCoroutine.run();
    }
}

int main()
{    
    test_pthread::test();
    

    printf("###############\n");
    return 0;
}
