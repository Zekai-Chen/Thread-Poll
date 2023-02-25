#include "TimerHeap.h"

TimerHeap::TimerHeap() = default;

TimerHeap::~TimerHeap() {
    while (!m_timer_pqueue.empty()) {
        delete m_timer_pqueue.top();
        m_timer_pqueue.pop();
    }
}

void TimerHeap::AddTimer(heap_timer *timer) {
    if (!timer) return;
    m_timer_pqueue.emplace(timer);
}

void TimerHeap::DelTimer(heap_timer *timer) {
    if (!timer) return;
    // Set the callback function of the target timer to empty, that is, delayed destruction
    //Reduces the overhead of deleting timers, but increases the size of the priority queue
    timer->callBackFunc = nullptr;
}

void TimerHeap::Tick() {
    time_t cur = time(nullptr);
    while (!m_timer_pqueue.empty()) {
        heap_timer *timer = m_timer_pqueue.top();
        //The top-of-heap timer has not expired, exiting the loop
        if (timer->expire > cur) break;
        //Otherwise, execute the task in the top-of-heap timer
        if (timer->callBackFunc) timer->callBackFunc(timer->user_data);
        m_timer_pqueue.pop();
    }
}

heap_timer *TimerHeap::Top() {
    if (m_timer_pqueue.empty()) return nullptr;
    else return m_timer_pqueue.top();
}

