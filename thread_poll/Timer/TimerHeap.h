#ifndef CHATROOMSERVER_TIMERHEAP_H
#define CHATROOMSERVER_TIMERHEAP_H

#include <netinet/in.h>
#include <functional>
#include <vector>
#include <queue>

#define BUFFER_SIZE 0xFFFF  //Buffer data size
#define USER_NAME_SIZE 0xFF  //Buffer data size
#define TIMESLOT    30      //Timing

class heap_timer; //forward declaration

// client data
struct client_data {
    sockaddr_in address;
    int sockfd;
    int put_idx;
    int bp_clnt;
    char buf[BUFFER_SIZE];
    char user_name[USER_NAME_SIZE];
    heap_timer *timer;
};

// timer class
class heap_timer {
public:
    heap_timer(int delay) {
        expire = time(nullptr) + delay;
    }

public:
    time_t expire; //The absolute time when the timer takes effect
    std::function<void(client_data *)> callBackFunc; //Callback
    client_data *user_data; //User data
};

struct cmp { //Comparison function to implement small root heap
    bool operator () (const heap_timer* a, const heap_timer* b) {
        return a->expire > b->expire;
    }
};

class TimerHeap {
public:
    explicit TimerHeap();
    ~TimerHeap();

public:
    void AddTimer(heap_timer *timer); //Add timer
    void DelTimer(heap_timer *timer); //delete timer
    void Tick(); 
    heap_timer* Top();

private:
    std::priority_queue<heap_timer *, std::vector<heap_timer *>, cmp> m_timer_pqueue; //time heap
};

#endif //CHATROOMSERVER_TIMERHEAP_H

