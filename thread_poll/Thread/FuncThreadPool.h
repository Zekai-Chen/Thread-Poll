#ifndef CHATROOMSERVER_FUNCTHREADPOOL_H
#define CHATROOMSERVER_FUNCTHREADPOOL_H

#include <iostream>
#include <functional>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>


class FuncThreadPool {
public:
    typedef std::function<void(void)> Task;

    explicit FuncThreadPool(int threadNumber = 5);
    ~FuncThreadPool();

    bool append(Task task); //add task interface

    void stopThreadPool();

private:
    static void *entryFunc(void *arg);
    void run();

private:
    std::queue<Task> mTaskQueue; //task queue
    std::mutex m_Mutex; //Mutex
    std::condition_variable mLockObject; //condition variable
    bool mStop; //Whether the thread pool executes
};

#endif //CHATROOMSERVER_FUNCTHREADPOOL_H
