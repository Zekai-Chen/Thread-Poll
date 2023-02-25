#ifndef BBSERV_BBSERVER_H
#define BBSERV_BBSERVER_H

#include <list> //list
#include <vector> //vector
#include <string> //string
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include "../Timer/TimerHeap.h" //time heap
#include "../Thread/FuncThreadPool.h"
//#include "../bb/rw_bb.h"

#define MAX_EVENT_NUMBER 5000   //Epoll maximum number of events
#define FD_LIMIT         65535  //Maximum number of clients
// #define BUFFER_SIZE      0xFFFF //Buffer data size

struct Config
{
    bool _d;
    bool _D;
    bool _bp;
    bool _sp;
    bool _Tmax;
    bool _bbfile;

    bool d;
    bool D;
    int bp;
    int sp;
    int Tmax;
    std::string bbfile;
    std::vector<std::string> peers;
};

using namespace std;

class ReadWriteMutex
{
public:
    ReadWriteMutex() = default;
    ~ReadWriteMutex() = default;

    ReadWriteMutex(const ReadWriteMutex &) = delete;
    ReadWriteMutex & operator=(const ReadWriteMutex &) = delete;

    ReadWriteMutex(const ReadWriteMutex &&) = delete;
    ReadWriteMutex & operator=(const ReadWriteMutex &&) = delete;

    void lock_read() {
        std::unique_lock<std::mutex> lock( m_mutex );
        m_cond_read.wait(lock, [this]()-> bool {
            return m_write_count == 0;
        });
        ++m_read_count;
    }

    void unlock_read() {
        std::unique_lock<std::mutex> lock( m_mutex );
        if (--m_read_count == 0 && m_write_count > 0) {
            m_cond_write.notify_one();
        }
    }

    void lock_write() {
        std::unique_lock<std::mutex> lock( m_mutex );
        ++m_write_count;
        m_cond_write.wait(lock, [this]()-> bool {
            return m_read_count == 0 && !m_writing;
        });
        m_writing = true;
    }

    void unlock_write() {
        std::unique_lock<std::mutex> lock( m_mutex );
        if (--m_write_count == 0) {
            m_cond_read.notify_all();
        } else {
            m_cond_write.notify_one();
        }
        m_writing = false;
    }

private:
    volatile size_t m_read_count = 0;
    volatile size_t m_write_count = 0;
    volatile bool m_writing = false;
    std::mutex m_mutex;
    std::condition_variable m_cond_read;
    std::condition_variable m_cond_write;
};

class UniqueReadLock
{
public:
    explicit UniqueReadLock(ReadWriteMutex & rwLock)
            : m_ptr_rw_lock(&rwLock) {
        m_ptr_rw_lock->lock_read();
    }

    ~UniqueReadLock() {
        if (m_ptr_rw_lock) {
            m_ptr_rw_lock->unlock_read();
        }
    }

    UniqueReadLock() = delete;
    UniqueReadLock(const UniqueReadLock &) = delete;
    UniqueReadLock & operator = (const UniqueReadLock &) = delete;
    UniqueReadLock(const UniqueReadLock &&) = delete;
    UniqueReadLock & operator = (const UniqueReadLock &&) = delete;

private:
    ReadWriteMutex * m_ptr_rw_lock = nullptr;
};

class UniqueWriteLock
{
public:
    explicit UniqueWriteLock(ReadWriteMutex & rwLock)
            : m_ptr_rw_lock(&rwLock) {
        m_ptr_rw_lock->lock_write();
    }

    ~UniqueWriteLock() {
        if (m_ptr_rw_lock) {
            m_ptr_rw_lock->unlock_write();
        }
    }

    UniqueWriteLock() = delete;
    UniqueWriteLock(const UniqueWriteLock &) = delete;
    UniqueWriteLock & operator = (const UniqueWriteLock &) = delete;
    UniqueWriteLock(const UniqueWriteLock &&) = delete;
    UniqueWriteLock & operator = (const UniqueWriteLock &&) = delete;

private:
    ReadWriteMutex * m_ptr_rw_lock = nullptr;
};

class BulletinBoard
{
public:
    explicit BulletinBoard()
    {
    }

    ~BulletinBoard()
    {
        if(m_out_file.is_open())
        {
            m_out_file.flush();
            m_out_file.close();
        }
    }

    bool open_bbfile();
    bool read_msg(int msg_id, string & ret);
    int write_msg(string user, string msg);
    bool replace_msg(int msg_id, string user, string msg);
    void close_bbfile();
private:
    int get_msg_gen();

    ReadWriteMutex m_rw_lock;
    ofstream m_out_file;

    int m_msg_gen;
};

class BbServer {
public:
    explicit BbServer();
    ~BbServer();
    void ParseCommandLine(int argc, char ** argv);
    bool InitServer();
    void InitSignalServer();
    void InitDaemonServer();
    void Run();

    void HandleBpCmd(int sockfd, char * pCmd);
    void HandleSyncServ();
    void DelayMs(int howManyMs);

private:
    int m_bp_socketFd = 0; //Created socket file descriptor
    int m_sp_socketFd = 0; //Created socket file descriptor

    static int s_epollFd; //Created epoll file descriptor
    static std::list<int> s_clientsList; //List of connected client sockets
    static int s_pipeFd[2]; //Signal communication pipeline
    TimerHeap m_timerHeap; //time heap timer
    static bool s_isAlarm; //Whether a scheduled task is in progress
    client_data* m_user;
    bool m_timeout = false; //timer task tag
    bool m_serverStop = true;
    BulletinBoard m_bb;
    FuncThreadPool * m_threadPool;

private:
    int SetNonblocking(int fd); //set file descriptor to non-blocking
    void Addfd(int epoll_fd, int sock_fd); //Register events on file descriptors
    static void SignalHandler(int sig); //Signal handling callback function
    void AddSignal(int sig); //Set the signal handler callback function
    void HandleSignal(const std::string &sigMsg); //custom signal handler
    void TimerHandler(); //SIGALRM signal handler
    static void TimerCallBack(client_data *user_data); //Timer callback function
    void CreatePidFile();
    void CloseAllFds();
    void CloseAllClients();
    void CloseOneClient(int sockfd);
};

#endif //BBSERV_BBSERVER_H
