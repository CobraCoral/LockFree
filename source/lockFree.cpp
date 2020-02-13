#include <queue>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <pthread.h>
#define CAS(a,b,c) __sync_bool_compare_and_swap(a, b, c)

volatile bool start=false;
std::size_t LOOPS(0);
const std::size_t CACHE_LINE(64);
int *values(NULL);

struct LFNode
{
    LFNode() : value(0), next(NULL) { }
    LFNode( int* val ) : value(val), next(NULL) { }
    int* value;
    volatile LFNode* next;
};

LFNode *nodes(NULL);

class LockFreeQueue
{
public:
    //char cachePad0[CACHE_LINE];
    volatile LFNode *first;      // for producer only
    char cachePad1[CACHE_LINE - sizeof(LFNode*)];
    volatile LFNode *last;
    char cachePad2[CACHE_LINE - sizeof(LFNode*)];
    volatile LFNode *divider;
    //char cachePad3[CACHE_LINE - sizeof(LFNode*)];

    LockFreeQueue()
    { 
        // add dummy separator
        first = divider = last = new LFNode( NULL );
    }

    ~LockFreeQueue() {}

    void Produce( int* t )
    {
        //std::cout << "producing [" << *t << "] last[" << (void*)last << "] divider[" << (void*)divider << "] first[" << (void*)first << "]" << std::endl;
        static std::size_t idx(0);
        last->next = const_cast<volatile LFNode*>(&nodes[idx++]);
        last->next->value = t; // add the new item
        #ifdef LOCKFREE
        while(!CAS(&last, last, last->next)) {} // publish it
        #else
        last = last->next; // publish it
        #endif
    }

    int* Consume()
    {
        if( divider != last )
        {         
            //std::cout << "consuming last[" << (void*)last << "] divider[" << (void*)divider << "] first[" << (void*)first << "]" << std::endl;
            // if queue is nonempty
            int* result = divider->next->value; // C: copy it back
            #ifdef LOCKFREE
            while (!CAS(&divider, divider, divider->next)) {} // D: publish that we took it
            #else
            divider = divider->next; // D: publish that we took it
            #endif
            return result;                  // and report success
        }
        return NULL;
    }
};

void *Producer( void *ptr )
{
    while (!start) {}
    LockFreeQueue* pQ((LockFreeQueue*)ptr);
    for ( int i=0; i<LOOPS; ++i )
    {
        //std::cout << "producing [" << *pi << "]" << std::endl;
    	pQ->Produce(&values[i]);
    }
}


void *Consumer( void *ptr )
{
    while (!start) {}
    LockFreeQueue* pQ((LockFreeQueue*)ptr);
    volatile int* a=(NULL);
    while(true)
    {
        volatile int* a = pQ->Consume();
        if (a && *a == LOOPS) break;
    }
}
int main(int argc, char* argv[])
{
    LOOPS=atoi(argv[1]);
    nodes = new LFNode[LOOPS];
    values = new int[LOOPS];
    for ( int i=0; i<LOOPS; ++i )
    {
        values[i] = i+1;
    }
    LockFreeQueue q;
#ifndef LOCKFREE
    timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    for ( int i=0; i<LOOPS; ++i )
    {
        //std::cout << values[i]  << std::endl;
    	q.Produce(&values[i]);
    }
    volatile int* a=(NULL);
    while(true)
    {
        volatile int* a = q.Consume();
        if (a)
        {
            //std::cout << *a << std::endl;
            if (*a == LOOPS) break;
        }
    }
    gettimeofday(&tv_end, NULL);
    std::size_t delta = (tv_end.tv_sec * 1000000 + tv_end.tv_usec) - (tv_start.tv_sec * 1000000 + tv_start.tv_usec);
    std::cout << delta << std::endl;
    return 0;
#else
    pthread_t thread1, thread2;
    int iret1 = pthread_create( &thread1, NULL, Producer, (void*)&q);
    int iret2 = pthread_create( &thread2, NULL, Consumer, (void*)&q);
    timeval tv_start, tv_end;
    gettimeofday(&tv_start, NULL);
    start = true;
    pthread_join( thread1, NULL);
    pthread_join( thread2, NULL);
    gettimeofday(&tv_end, NULL);
    std::size_t delta = (tv_end.tv_sec * 1000000 + tv_end.tv_usec) - (tv_start.tv_sec * 1000000 + tv_start.tv_usec);
    std::cout << delta << std::endl;
    return 0;
#endif
}
