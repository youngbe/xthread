#include <stdio.h>
#include <stdint.h>

#include "xthread.h"

void *test1(void* args)
{
    printf("thread1_get:%p\n", args);
    for (uint64_t i=0; i<10; ++i)
    {
        printf("thread1_%llu\n", i);
        xthread_yield();
    }
    return (void *)0x10;
}
void *test2(void* args)
{
    printf("thread2_get:%p\n", args);
    for (uint64_t i=0; i<20; ++i)
    {
        printf("thread2_%llu\n", i);
        xthread_yield();
    }
    return (void *)0x20;
}
void *test3(void* args)
{
    printf("thread3_get:%p\n", args);
    for (uint64_t i=0; i<20; ++i)
    {
        printf("thread3_%llu\n", i);
        xthread_yield();
    }
    return (void *)0x30;
}
void *test4(void* args)
{
    printf("thread4_get:%p\n", args);
    for (uint64_t i=0; i<20; ++i)
    {
        printf("thread4_%llu\n", i);
        xthread_yield();
    }
    return (void *)0x40;
}

int main()
{
    xthread_t xt1, xt2, xt3, xt4;
    xthread_create(&xt1, NULL, test1, (void *)0x110);
    xthread_create(&xt2, NULL, test2, (void *)0x120);
    xthread_create(&xt3, NULL, test3, (void *)0x130);
    xthread_create(&xt4, NULL, test4, (void *)0x140);
    for (uint64_t i=0; i<10; ++i)
    {
        printf("main1_%llu\n", i);
        xthread_yield();
    }
    void * ret;
    xthread_join( xt1, &ret );
    printf("xt1 exit with:%p\n", ret);
    for (uint64_t i=0; i<10; ++i)
    {
        printf("main2_%llu\n", i);
        xthread_yield();
    }
    xthread_join( xt2, &ret );
    printf("xt2 exit with:%p\n", ret);
    xthread_join( xt3, &ret );
    printf("xt3 exit with:%p\n", ret);
    xthread_join( xt4, &ret );
    printf("xt4 exit with:%p\n", ret);
    return 0;
}
