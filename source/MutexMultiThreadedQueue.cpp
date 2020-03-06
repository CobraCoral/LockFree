#include <queue>
#include <cstdlib>
#include <iostream>
#include <sys/time.h>
#include <pthread.h>

volatile bool start=false;
std::size_t LOOPS(0);
const std::size_t CACHE_LINE(64);
int *values(NULL);
pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;

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
        pthread_mutex_lock( &mutex_ );
        //std::cout << "producing [" << *t << "] last[" << (void*)last << "] divider[" << (void*)divider << "] first[" << (void*)first << "]" << std::endl;
        static std::size_t idx(0);
        last->next = const_cast<volatile LFNode*>(&nodes[idx++]);
        last->next->value = t; // add the new item
        last = last->next; // publish it
        pthread_mutex_unlock( &mutex_ );
    }

    int* Consume()
    {
        pthread_mutex_lock( &mutex_ );
        if( divider != last )
        {         
            //std::cout << "consuming last[" << (void*)last << "] divider[" << (void*)divider << "] first[" << (void*)first << "]" << std::endl;
            // if queue is nonempty
            int* result = divider->next->value; // C: copy it back
            divider = divider->next; // D: publish that we took it
            pthread_mutex_unlock( &mutex_ );
            return result;                  // and report success
        }
        pthread_mutex_unlock( &mutex_ );
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
}
