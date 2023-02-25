#include "FuncThreadPool.h"

FuncThreadPool::FuncThreadPool(int threadNumber) : mStop(false)
{
    if (threadNumber <= 0) throw std::exception();

    for (int i = 0; i < threadNumber; ++i) {
        std::cout << "FuncThreadPool: Create " << i + 1 << " thread" << std::endl;
        std::thread([this] { FuncThreadPool::entryFunc(this); }).detach(); //Create and detach threads
    }
}

FuncThreadPool::~FuncThreadPool() {
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        mStop = true;
    }
    mLockObject.notify_all(); //Notify all threads to stop
}

bool FuncThreadPool::append(FuncThreadPool::Task task) {
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        mTaskQueue.emplace(task);
    }
    mLockObject.notify_one(); //notification thread
    return true;
}

void *FuncThreadPool::entryFunc(void *arg) {
    FuncThreadPool *ptr = static_cast<FuncThreadPool *>(arg);
    ptr->run();
    return nullptr;
}

void FuncThreadPool::stopThreadPool()
{
    std::cout << "stop thread pool" << std::endl;
    {
        std::unique_lock<std::mutex> lock(m_Mutex);
        mStop = true;
    }
    mLockObject.notify_all(); //Notify all threads to stop
}

void FuncThreadPool::run()
{
    std::unique_lock<std::mutex> lock(m_Mutex);
    while (!mStop) {
        mLockObject.wait(lock);
        if (!mTaskQueue.empty()) {
            Task task = mTaskQueue.front();
            mTaskQueue.pop();
            task();
        }
    }
}
