#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef uint64_t                xthread_t;
typedef struct xthread_attr_t   xthread_attr_t;
typedef struct xthread_mutex_t  xthread_mutex_t;
typedef void                    xthread_mutexattr_t;
typedef struct xthread_cond_t   xthread_cond_t;
typedef void                    xthread_condattr_t;
typedef struct Xthread          Xthread;
typedef struct Cond             Cond;
typedef struct Pointer_array    Pointer_array;
typedef struct List             List;
typedef struct Dq_list          Dq_list;

static inline void Pointer_array_init(Pointer_array*const __this);
static inline void Pointer_array_destroy(Pointer_array*const __this);
static inline bool Pointer_array_emplace_back(Pointer_array*const __this, void*const __new);
static inline void Pointer_array_remove(Pointer_array*const __this, const uint64_t i);
static inline int       xthread_create (xthread_t * __newthread, const xthread_attr_t * __attr, void *(*__start_routine) (void *), void *args);
static inline int       xthread_yield();
static inline void      xthread_exit(void *);
static inline int       xthread_join(xthread_t thread, void **retval);
static inline xthread_t xthread_self();
static inline int       xthread_attr_init(xthread_attr_t* attr);
static inline int       xthread_attr_getstacksize(xthread_attr_t* attr, size_t* size);
static inline int       xthread_attr_setstacksize(xthread_attr_t* attr, size_t size);
static inline int       xthread_attr_getstack(xthread_attr_t* attr, void** addr, size_t* size);
static inline int       xthread_attr_setstack(xthread_attr_t* attr,void* addr, size_t size);
static inline int       xthread_attr_destroy(xthread_attr_t* attr);
static inline int       xthread_mutex_init(xthread_mutex_t *mutex, const xthread_mutexattr_t *__mutexattr);
static inline int       xthread_mutex_lock(xthread_mutex_t *__mutex);
static inline int       xthread_mutex_trylock(xthread_mutex_t *__mutex);
static inline int       xthread_mutex_unlock(xthread_mutex_t *__mutex);
static inline int       xthread_mutex_destroy(xthread_mutex_t *__mutex);
static inline int       xthread_cond_init(xthread_cond_t *__cond, const xthread_condattr_t *__cond_attr);
static inline int       xthread_cond_wait(xthread_cond_t *__cond, xthread_mutex_t *__mutex);
static inline int       xthread_cond_signal(xthread_cond_t *__cond);
static inline int       xthread_cond_destroy(xthread_cond_t *__cond);
#define list_for_least(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_entry(ptr, type, member) \
    (type *)( (char *)(ptr) - offsetof(type,member) )

// list_add(List* pos, List* new)
#define list_add(pos, new) \
{ \
    (new)->next=(pos)->next; \
    (pos)->next=(new); \
}

// dq_list_add(List* pos, List* new)
#define dq_list_add(pos, new) \
{ \
    (pos)->next->prev=(new); \
    (new)->next=(pos)->next; \
    (new)->prev=(pos); \
    (pos)->next=(new); \
}

// dq_list_remove(List* pos)
#define dq_list_remove(pos) \
{ \
    (pos)->prev->next=(pos)->next; \
    (pos)->next->prev=(pos)->prev; \
}

// list_index_init(List ** index, List* new)
#define list_index_init(index, new) \
    *(index)=(new)->next=(new)

// dq_list_index_init(List ** index, List* new)
#define dq_list_index_init(index, new) \
    *(index)=(new)->prev=(new)->next=(new)

struct List {
    List *next;
};

struct Dq_list {
    Dq_list *next;
    Dq_list *prev;
};

struct Xthread
{
    //上下文
    void* rsp;
    //栈空间起始地址(低地址)
    void *stack_addr;
    //每个线程有独一无二的id，即使线程已停止运行。主线程id为0
    xthread_t id;
    //使用xthread_join()等待这个线程结束的线程表
    List* join_list_index;
    List join_list;
    Dq_list running_list;
    List stopped_list;
};

struct xthread_attr_t
{
    bool   set_stackaddr;  // 是否设置了栈的位置
    void*  stack_addr;     // 线程栈的位置
    size_t stack_size;     // 线程栈的大小
};

struct Pointer_array
{
    void **list;
    uint64_t size;
    uint64_t capacity;
};

struct xthread_mutex_t
{
    Pointer_array wait_list;
    Xthread* owner;
};

struct xthread_cond_t
{
    Dq_list *cond_list_index;
    Pointer_array destroyer_list;
};

struct Cond
{
    Dq_list cond_list;
    Xthread *thread;
};

//运行中线程表(运行态、就绪态、阻塞态)
Dq_list* running_list_index;
//可调度线程表(运行态、就绪态线程)
Pointer_array schedulable_list;
//现在正在运行中的线程(运行态)在可调度线程表中的位置
uint64_t running_thread=0;
//已停止线程表
List* stopped_list_index=NULL;
//xthread_create()时，下一个新线程分配的线程id
uint64_t next_thread_id=1;

static inline __attribute__((constructor)) void before_main_xthread();
static inline __attribute__((destructor)) void after_main_xthread();

// 不稳定
/*
static inline void xthread_start(void * args, void *(*func)(void *))
{
    __asm__ volatile (
            "pushq  %%rdi\n\t"
            "pushq  %%rsi\n"
            ".Lreal_xthread_start:\n\t"
            "popq   %%rsi\n\t"
            "popq   %%rdi"
            :
            :"D"(args), "S"(func)
            :
            );
    xthread_exit(func(args));
}
*/

// 使用汇编实现开始函数
#ifndef __cplusplus
void __xthread_start();
#else
extern "C" void __xthread_start();
#endif

static inline void __xthread_switch_thread(Xthread* const old_thread, Xthread* const new_thread)
{
    {
        void *temp_new_rsp=new_thread->rsp;
        // 根据 gcc 的优化规则，如果输入不变则输出不变，会被优化掉
        // 所以加上 volatile
        __asm__ volatile (
                // gcc可能提供 %rsp 的偏移，如果要阻止gcc这么做需要在损坏列表里加上rsp，这样会带来warning
                // 因此在 %rsp 的值改变之前，先把需要的东西读出来
                "leaq   %[old_rsp], %%rbx\n\t"
                "pushq  %%rbp\n\t"
                "pushq  %%r14\n\t"
                "pushq  %%r15\n\t"
                "leaq   .Lxthread%=(%%rip), %%rcx\n\t"
                "pushq  %%rcx\n\t"
                "movq   %%rsp, (%%rbx)\n\t"
                "movq   %%rax, %%rsp\n\t"
                "retq\n"
                ".Lxthread%=:\n\t"
                "popq   %%r15\n\t"
                "popq   %%r14\n\t"
                "popq   %%rbp"
                :"+a"(temp_new_rsp), "=m"(new_thread->rsp), [old_rsp]"=m"(old_thread->rsp)
                :
                // 省略 %rsp %rax %rbp %r14 %r15 memory
                :"cc","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","st",
                "mm0","mm1","mm2","mm3","mm4","mm5","mm6","mm7",
                "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9","xmm10","xmm11","xmm12","xmm13","xmm14","xmm15",
                "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5","ymm6","ymm7","ymm8","ymm9","ymm10","ymm11","ymm12","ymm13","ymm14","ymm15",
                "zmm0","zmm1","zmm2","zmm3","zmm4","zmm5","zmm6","zmm7","zmm8","zmm9","zmm10","zmm11","zmm12","zmm13","zmm14","zmm15"
                    );
    }
}

inline int xthread_yield()
{
    if (schedulable_list.size==1)
    {
        return 1;
    }
    uint64_t temp;
    while ( true )
    {
        temp=rand()%schedulable_list.size;
        if ( temp!=running_thread )
        {
            break;
        }
    }
    Xthread* const old_running=(Xthread*)schedulable_list.list[running_thread];
    running_thread=temp;
    __xthread_switch_thread( old_running, (Xthread*)schedulable_list.list[running_thread] );
    return 0;
}

inline int xthread_create (xthread_t * __newthread,
        const xthread_attr_t * __attr,
        void *(*__start_routine) (void *),
        void *args)
{
    Xthread* new_thread;
    if (__attr==NULL)
    {
label1:
        void *addr=malloc(0x800000);
        if (addr==NULL)
        {
            return 11;
        }
        new_thread=(Xthread*)malloc(sizeof(Xthread));
        if (new_thread==NULL)
        {
            free(addr);
            return 11;
        }
        new_thread->rsp=(char *)(new_thread->stack_addr=addr)+0x800000;
    }
    else if(__attr->set_stackaddr)
    {
        new_thread=(Xthread*)malloc(sizeof(Xthread));
        if (new_thread==NULL)
        {
            return 11;
        }
        new_thread->stack_addr=NULL;
        new_thread->rsp=(char *)(__attr->stack_addr)+__attr->stack_size;
    }
    else if (__attr->stack_size==0)
    {
        goto label1;
    }
    else
    {
        void *addr=malloc(__attr->stack_size);
        if (addr==NULL)
        {
            return 11;
        }
        new_thread=(Xthread*)malloc(sizeof(Xthread));
        if (new_thread==NULL)
        {
            free(addr);
            return 11;
        }
        new_thread->rsp=(char *)(new_thread->stack_addr=addr)+__attr->stack_size;
    }

    //增加线程表表项
    if (!Pointer_array_emplace_back(&schedulable_list, new_thread))
    {
        free(new_thread->stack_addr);
        free(new_thread);
        return 11;
    }
    //内存对齐16的倍数，否则call printf直接爆炸，具体原因我也不清楚
    new_thread->rsp= (void *)((uint64_t)new_thread->rsp&0xFFFFFFFFFFFFFFF0);
    new_thread->rsp=(char *)new_thread->rsp-4*sizeof(void *);

    ( (void (**)() )new_thread->rsp)[0]=__xthread_start;
    ( (void *(**)(void *))new_thread->rsp )[1]=__start_routine;
    ((void **)new_thread->rsp)[2]=args;
    ( (void (**)(void *))new_thread->rsp )[3]=xthread_exit;
    dq_list_add(running_list_index, &new_thread->running_list);

    new_thread->join_list_index=NULL;

    new_thread->join_list.next=(List *)args;
    *__newthread=new_thread->id=next_thread_id++;
    return 0;
}

__attribute__((noreturn))
inline void xthread_exit(void * retval)
{
    Xthread*const exit_thread=(Xthread*)schedulable_list.list[running_thread];
    //只有一个线程了，直接退出程序
    if (running_list_index->next==running_list_index)
    {
        exit((int)(uint64_t)retval);
    }
    exit_thread->rsp=retval;
    //将此线程从可调度线程表中移除
    Pointer_array_remove(&schedulable_list, running_thread);
    //将此线程从运行中线程表中移除
    {
        Dq_list* to_remove=&exit_thread->running_list;
        if (to_remove==running_list_index)
        {
            running_list_index=to_remove->next;
        }
        dq_list_remove(to_remove);
    }
    //将此线程的join表中的线程移入可调度线程表
    if (exit_thread->join_list_index!=NULL)
    {
        List *head=exit_thread->join_list_index;
        List *pos;
        Xthread *temp;
        list_for_least(pos, head)
        {
            temp=list_entry(pos, Xthread, join_list);
            Pointer_array_emplace_back(&schedulable_list, temp);
        }
        temp=list_entry(pos, Xthread, join_list);
        Pointer_array_emplace_back(&schedulable_list, temp);
    }
    //将此线程放入已结束线程表
    if (stopped_list_index==NULL)
    {
        list_index_init(&stopped_list_index, &exit_thread->stopped_list);
    }
    else
    {
        list_add(stopped_list_index, &exit_thread->stopped_list);
    }
    //随机从可调度表中选取一个线程运行
    if (schedulable_list.size==0)
    {
        //死锁发生
        while (1)
        {
            sleep(60);
        }
    }
    running_thread=rand()%schedulable_list.size;
    //释放线程栈内存并切换线程
    __asm__ volatile (
            "movq   %[new_rsp], %%rsp\n\t"
            "movq   %%rsp, %%rbp\n\t"
            "andq   $0xFFFFFFFFFFFFFFF0, %%rsp\n\t"
            "callq  free@PLT\n\t"
            "movq   %%rbp, %%rsp\n\t"
            "retq"
            :
            :[new_rsp]"g"( ((Xthread*) schedulable_list.list[running_thread] )->rsp), "D"(exit_thread->stack_addr)
            // 标注 memory 使得gcc将所有变量写入内存
            :"memory"
            );
    // 标注 noreturn+__builtin_unreachable(); 让gcc删除函数末尾pop的代码
    // 但是gcc仍然会在函数开头push
    __builtin_unreachable();
}

inline int xthread_join(xthread_t thread, void **retval)
{
    Xthread *target=list_entry(running_list_index, Xthread, running_list);
    if (target->id==thread)
    {
        goto labelin;
    }
    {
        Dq_list *pos;
        list_for_least(pos, running_list_index)
        {
            target=list_entry(pos, Xthread, running_list);
            if (target->id==thread)
            {
                goto labelin;
            }
        }
    }
    goto labelout;
    {
labelin:
        Xthread *now=(Xthread*)schedulable_list.list[running_thread];
        //将当前线程放入目标等待线程的join表
        if (target->join_list_index==NULL)
        {
            list_index_init(&target->join_list_index, &now->join_list);
        }
        else
        {
            list_add(target->join_list_index, &now->join_list);
        }
        //将当前线程移出可调度线程表
        Pointer_array_remove(&schedulable_list, running_thread);
        //随机从可调度表中选取一个线程运行
        if (schedulable_list.size==0)
        {
            //死锁发生
            while (1)
            {
                sleep(60);
            }
        }
        running_thread=rand()%schedulable_list.size;
        __xthread_switch_thread(now, (Xthread*)schedulable_list.list[running_thread]);
    }
labelout:
    //表空
    if (stopped_list_index==NULL)
    {
        return 3;
    }
    //表不空
    {
        List *pos;
        list_for_least(pos, stopped_list_index)
        {
            target=list_entry(pos->next, Xthread, stopped_list);
            if (target->id==thread)
            {
                if (pos->next==stopped_list_index)
                {
                    stopped_list_index=pos->next->next;
                }
                pos->next=pos->next->next;
                *retval=target->rsp;
                free(target);
                return 0;
            }
        }
        target=list_entry(pos->next, Xthread, stopped_list);
        if (target->id==thread)
        {
            if (pos->next==pos)
            {
                stopped_list_index=NULL;
            }
            else
            {
                pos->next=pos->next->next;
            }
            *retval=target->rsp;
            free(target);
            return 0;
        }
    }
    return 3;
}

inline xthread_t xthread_self()
{
    return ((Xthread*)schedulable_list.list[running_thread])->id;
}

inline int xthread_mutex_init(xthread_mutex_t *mutex, const xthread_mutexattr_t *__mutexattr)
{
    if (__mutexattr!=NULL)
    {
        return -1;
    }
    Pointer_array_init(&mutex->wait_list);
    mutex->owner=NULL;
    return 0;
}

inline int xthread_mutex_lock(xthread_mutex_t *__mutex)
{
    if (__mutex->owner==NULL)
    {
        __mutex->owner=(Xthread*)schedulable_list.list[running_thread];
        return 0;
    }
    Xthread* now=(Xthread*)schedulable_list.list[running_thread];
    if (!Pointer_array_emplace_back(&__mutex->wait_list, now))
    {
        return -1;
    }
    Pointer_array_remove(&schedulable_list, running_thread);
    if (schedulable_list.size==0)
    {
        while (1)
        {
            sleep(60);
        }
    }
    running_thread=rand()%schedulable_list.size;
    __xthread_switch_thread(now, (Xthread*)schedulable_list.list[running_thread]);
    return 0;
}

inline int xthread_mutex_trylock(xthread_mutex_t *__mutex)
{
    if (__mutex->owner==NULL)
    {
        __mutex->owner=(Xthread*)schedulable_list.list[running_thread];
        return 0;
    }
    return 16;
}

inline int xthread_mutex_unlock(xthread_mutex_t *__mutex)
{
    if (__mutex->owner==NULL)
    {
        return 0;
    }
    if (__mutex->wait_list.size==0)
    {
        __mutex->owner=NULL;
        return 0;
    }
    uint64_t temp=rand()%__mutex->wait_list.size;
    __mutex->owner=(Xthread*)__mutex->wait_list.list[temp];
    Pointer_array_remove(&__mutex->wait_list, temp);
    Pointer_array_emplace_back(&schedulable_list, __mutex->owner);
    return 0;
}

inline int xthread_mutex_destroy(xthread_mutex_t *__mutex)
{
    if (__mutex->owner!=NULL)
    {
        return 16;
    }
    Pointer_array_destroy(&__mutex->wait_list);
    return 0;
}

inline int xthread_cond_init(xthread_cond_t *__cond, const xthread_condattr_t *__cond_attr)
{
    if (__cond_attr!=NULL)
    {
        return -1;
    }
    __cond->cond_list_index=NULL;
    Pointer_array_init(&__cond->destroyer_list);
    return 0;
}

inline int xthread_cond_wait(xthread_cond_t *__cond, xthread_mutex_t *__mutex)
{
    Cond *new_cond=(Cond *)malloc(sizeof(Cond));
    if (new_cond==NULL)
    {
        return -1;
    }
    if (xthread_mutex_unlock(__mutex)!=0)
    {
        return -1;
    }
    new_cond->thread=(Xthread *)schedulable_list.list[running_thread];
    Pointer_array_remove(&schedulable_list, running_thread);
    if (schedulable_list.size==0)
    {
        //死锁发生
        while (1)
        {
            sleep(60);
        }
    }
    if (__cond->cond_list_index==NULL)
    {
        dq_list_index_init(&__cond->cond_list_index, &new_cond->cond_list);
    }
    else
    {
        dq_list_add(__cond->cond_list_index, &new_cond->cond_list);
    }
    uint64_t temp=rand()%schedulable_list.size;
    __xthread_switch_thread(new_cond->thread, (Xthread *)schedulable_list.list[temp]);
    xthread_mutex_lock(__mutex);
    return 0;
}

inline int xthread_cond_signal(xthread_cond_t *__cond)
{
    if (__cond->cond_list_index==NULL)
    {
        return 0;
    }
    Cond *out=list_entry(__cond->cond_list_index, Cond, cond_list);
    Pointer_array_emplace_back(&schedulable_list, out->thread);
    if (__cond->cond_list_index->prev==__cond->cond_list_index)
    {
        __cond->cond_list_index=NULL;
        if (__cond->destroyer_list.size!=0)
        {
            uint64_t i=__cond->destroyer_list.size;
            do
            {
                i--;
                Pointer_array_emplace_back(&schedulable_list, __cond->destroyer_list.list[i]);
            }
            while (i!=0);
            Pointer_array_destroy(&__cond->destroyer_list);
        }
    }
    else
    {
        __cond->cond_list_index=__cond->cond_list_index->prev;
    }
    free(out);
    return 0;
}

inline int xthread_cond_destroy(xthread_cond_t *__cond)
{
    if (__cond->cond_list_index==NULL)
    {
        return 0;
    }
    Xthread *now=(Xthread *)schedulable_list.list[running_thread];
    if (!Pointer_array_emplace_back(&__cond->destroyer_list, now))
    {
        return -1;
    }
    Pointer_array_remove(&schedulable_list, running_thread);
    if (schedulable_list.size==0)
    {
        //死锁发生
        while (1)
        {
            sleep(60);
        }
    }
    uint64_t temp=rand()%schedulable_list.size;
    __xthread_switch_thread(now, (Xthread *)schedulable_list.list[temp]);
    return 0;
}

inline __attribute__((constructor)) void before_main_xthread()
{
    Pointer_array_init(&schedulable_list);
    Xthread *const main_thread=(Xthread *)malloc(sizeof(Xthread));
    Pointer_array_emplace_back(&schedulable_list, main_thread);
    main_thread->id=0;
    main_thread->stack_addr=NULL;
    main_thread->join_list_index=NULL;
    main_thread->stopped_list.next=NULL;
    dq_list_index_init(&running_list_index, &main_thread->running_list);
}

inline __attribute__((destructor)) void after_main_xthread()
{
    Pointer_array_destroy(&schedulable_list);
    free(list_entry(running_list_index, Xthread, running_list));
}

inline void Pointer_array_init(Pointer_array*const __this)
{
    __this->list=NULL;
    __this->capacity=__this->size=0;
}

inline void Pointer_array_destroy(Pointer_array*const __this)
{
    free(__this->list);
}

inline bool Pointer_array_emplace_back(Pointer_array*const __this, void*const __new)
{
    if (__this->size!=__this->capacity)
    {
        goto labelend;
    }
    else if (__this->list==NULL)
    {
        __this->list=(void **)malloc(16*sizeof(void *));
        if (__this->list==NULL)
        {
            return false;
        }
        __this->capacity=16;
    }
    else
    {
        void **temp=(void **)malloc((__this->capacity<<1)*sizeof(void *));
        if (temp==NULL)
        {
            temp=(void **)malloc((__this->capacity+1)*sizeof(void *));
            if (temp==NULL)
            {
                return false;
            }
            __this->capacity+=1;
        }
        else
        {
            __this->capacity<<=1;
        }
        memcpy(temp, __this->list, __this->size);
        free(__this->list);
        __this->list=temp;
    }
labelend:
    __this->list[__this->size]=__new;
    __this->size++;
    return true;
}

inline void Pointer_array_remove(Pointer_array*const __this, const uint64_t i)
{
    __this->list[i]=__this->list[__this->size-1];
    __this->size--;
}

inline int xthread_attr_init(xthread_attr_t* attr)
{
    attr->set_stackaddr=false;
    attr->stack_addr=NULL;
    attr->stack_size=0;
    return 0;
}

inline int xthread_attr_getstacksize(xthread_attr_t * attr, size_t* size)
{
    if (attr->set_stackaddr==false && attr->stack_size==0)
    {
        *size=0x4000;
    }
    else
    {
        *size=attr->stack_size;
    }
    return 0;
}

inline int xthread_attr_setstacksize(xthread_attr_t * attr, size_t size)
{
    if (size<0x4000)
    {
        return 22;
    }
    *(char **)(attr->stack_addr)-=size-attr->stack_size;
    attr->stack_size=size;
    return 0;
}

inline int xthread_attr_getstack(xthread_attr_t * attr, void** addr , size_t* size)
{
    *addr=attr->stack_addr;
    *size=attr->stack_size;
    return 0;
}

inline int xthread_attr_setstack(xthread_attr_t * attr, void* addr , size_t size)
{
    if (size<0x4000)
    {
        return 22;
    }
    attr->set_stackaddr=true;
    attr->stack_addr=addr;
    attr->stack_size=size;
    return 0;
}

inline int xthread_attr_destroy(xthread_attr_t*)
{
    return 0;
}
