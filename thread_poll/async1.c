#include <stdlib.h>
#include <pthread.h>
#include "async.h"
#include "utlist.h"
#include <iostream>
#include <fstream>
#include <fcntl.h> //fcntl()
#include <sys/epoll.h> //epoll
#include <netinet/in.h> //sockaddr_in
#include <arpa/inet.h> //inet_pton()
#include <string.h> //memset()
#include <unistd.h> //close() alarm()
#include <signal.h> //signal
#include <sys/wait.h> //waitpid()
#include <sys/mman.h> //shm_open()
#include <pthread.h>
#include <sys/stat.h>
#include <sstream>
#include <asm-generic/ioctls.h>
#include <sys/ioctl.h>
#include "../Thread/ThreadObject.h"
#include "BbServer.h"

void async_init(int num_threads) {

	int fstream = NULL;
	
bool readConfigFile()
{
    fstream cfgFile;
    cfgFile.open(conf_filename);//open a file
    if(!cfgFile.is_open())
    {
        cout<<"can not open cfg file:" << conf_filename << endl;
        return false;
    }

    char tmp[1000];
    while(!cfgFile.eof())//loop through each line
    {
        cfgFile.getline(tmp,1000);//Read first 1000 characters per line, 1000 should be enough
        string line(tmp);
        size_t pos = line.find('=');//Find the position of the "=" sign in each line, before the key and then the value
        if(pos==string::npos) return false;
        string tmpKey = line.substr(0,pos);//before the = sign
        string tmpValue = line.substr(pos+1);//After taking the = sign

        if("BBPORT"==tmpKey && !g_config._bp)
        {
            g_config.bp = stoi(tmpValue);
            if(g_config.bp < 0 || g_config.bp > 65535)
            {
                cout<<"bp in conf file not valid!"<<endl;
                return false;
            }
        }
        else if("SYNCPORT"==tmpKey && !g_config._sp)
        {
            g_config.sp = stoi(tmpValue);
            if(g_config.sp < 0 || g_config.sp > 65535)
            {
                cout<<"sp in conf file not valid!"<<endl;
                return false;
            }
        }
        else if("THMAX"==tmpKey && !g_config._Tmax)
        {
            g_config.Tmax = stoi(tmpValue);
            if(g_config.Tmax < 0)
            {
                cout<<"Tmax in conf file not valid!"<<endl;
                return false;
            }
        }
        else if("BBFILE"==tmpKey && !g_config._bbfile)
        {
            g_config.bbfile = tmpValue;
        }
        else if("DAEMON"==tmpKey && !g_config._d)
        {
            g_config.d = tmpValue == "true";
        }
        else if("DEBUG"==tmpKey && !g_config._D)
        {
            g_config.D = tmpValue == "true";
        }
        else if("PEERS"==tmpKey)
        {
            int l_pos = 0;
            int i_pos = tmpValue.find(',');
            for(; i_pos != string::npos; i_pos = tmpValue.find(',', l_pos))
            {
                g_config.peers.push_back(tmpValue.substr(l_pos, i_pos - l_pos));
                l_pos = i_pos + 1;
            }
            if(l_pos < tmpValue.length())
            {
                g_config.peers.push_back(tmpValue.substr(l_pos));
            }
            for(auto sss : g_config.peers)
            {
                cout << "peers :" << sss << endl;
            }
        }
    }

    cfgFile.close();
    if(g_config.bbfile.length() == 0)
    {
        cout << "Error in conf file not valid! no bbfile provided!" << endl;
        return false;
    }
    return true;
}

bool BulletinBoard::open_bbfile()
{
    m_msg_gen = get_msg_gen();
    cout << "m_msg_gen:" << m_msg_gen << endl;
    m_out_file.open(g_config.bbfile, std::ios::in | std::ios::out | std::ios::app);
    if(!m_out_file.is_open())
    {
        cout << g_config.bbfile << " can not open to write" << endl;
        return false;
    }
    return true;
}

void BulletinBoard::close_bbfile()
{
    if(m_out_file.is_open())
    {
        m_out_file.flush();
        m_out_file.close();
    }
}

bool BulletinBoard::read_msg(int msg_id, string & ret)
{
    if(g_config.D)
    {
        sleep(3);
    }
    if(g_config.D) cout << "read msg with msg_id=[" << msg_id << "] " << endl;
    {
        UniqueReadLock(this->m_rw_lock);
        ifstream in_file(g_config.bbfile);
        while(!in_file.eof())//loop through each line
        {
            string line_text;
            in_file >> line_text;
            size_t pos;
            if(line_text.length() > 0 && (pos = line_text.find('/')) != string::npos)
            {
                int line_msg_id = stoi(line_text.substr(0,pos));
                if(line_msg_id == msg_id)
                {
                    //stringstream ss;
                    //ss << "2.0 MESSAGE " << msg_id << " " << line_text.substr(pos + 1) << endl;
                    //ret = ss.str();
                    ret = line_text.substr(pos + 1);
                    in_file.close();
                    return true;
                }
            }
        }
        in_file.close();
    }
    //stringstream ss;
    //ss << "2.1 UNKNOWN " << msg_id << " !" << endl;
    //return ss.str();
    return false;
}

int BulletinBoard::write_msg(string user, string msg)
{
    if(g_config.D){
        sleep(6);
    }
    if(g_config.D) cout << "write msg user=[" << user << "], msg=[" << msg << "]" << endl;
    int new_msg_id;
    {
        UniqueWriteLock(this->m_rw_lock);
        new_msg_id = m_msg_gen;
        m_out_file << new_msg_id << "/" << user << "/" << msg << std::endl;
        m_msg_gen += 1;
    }
    return new_msg_id;
}

bool BulletinBoard::replace_msg(int msg_id, string user, string msg)
{
    long long begin_pos = -1, end_pos = -1;
    {
        UniqueReadLock(this->m_rw_lock);

        ifstream in_file(g_config.bbfile);
        for(long long i = in_file.tellg(); !in_file.eof(); i = in_file.tellg())
        {
            string line_text;
            in_file >> line_text;
            size_t pos;
            if(line_text.length() > 0 && (pos = line_text.find('/')) != string::npos)
            {
                int line_msg_id = stoi(line_text.substr(0,pos));
                if(line_msg_id == msg_id)
                {
                    begin_pos = i;
                    end_pos = in_file.tellg();
                    break;
                }
            }
        }
        in_file.close();
    }
    bool ret = false;
    if(begin_pos != -1)
    {
        if(g_config.D) cout << "begin_pos: " << begin_pos << " end_pos: "<< end_pos << " " << endl;

        UniqueWriteLock(this->m_rw_lock);
        long long old_pos = m_out_file.tellp();
        m_out_file.seekp(begin_pos);
        for(long long i = begin_pos; i < end_pos; ++ i)
        {
            //Empty old file contents to '\n'
            const char * p = "\n";
            m_out_file.write(p, 1);
        }
        m_out_file.flush();
        m_out_file.seekp(old_pos);
    
        m_out_file << msg_id << "/" << user << "/" << msg << std::endl;
        m_out_file.flush();

        ret = true;
    }
    return ret;
}

int BulletinBoard::get_msg_gen()
{
    int gen = 1;
    ifstream in_file(g_config.bbfile);
    if(in_file.is_open())
    {
      
        in_file.seekg(0, ios::beg);

        char tmp[8192];
        while(!in_file.eof())//loop through each line
        {
            in_file.getline(tmp, 8192);//Read first 8192 characters per line, 8192 should be enough
            string line(tmp);
            if(line.length() > 0)
            {
                gen += 1;
            }
        }
    }

    return gen;
}

//#######################################################################################

int BbServer::s_pipeFd[2];
int BbServer::s_epollFd = 0;
std::list<int> BbServer::s_clientsList;
bool BbServer::s_isAlarm = false;

BbServer::BbServer() = default;

BbServer::~BbServer() {
    s_clientsList.clear();
    // m_users.clear();
    delete [] m_user;
    if(m_threadPool)
    {
        delete m_threadPool;
    }
}

void BbServer::ParseCommandLine(int argc, char ** argv)
{
    //default values
    g_config._d = false;
    g_config._D = false;
    g_config._bp = false;
    g_config._sp = false;
    g_config._Tmax = false;
    g_config._bbfile = false;

    g_config.Tmax = 20;
    g_config.bp = 9000;
    g_config.sp = 10000;
    g_config.d = true;
    g_config.D = false;

    if(argc < 2)
    {
        return;
    }
    int ii = 1;
    while(ii < argc)
    {
        if(strcmp(argv[ii], "-f") == 0)
        {
            g_config.d = false;
            g_config._d = true;
            ii += 1;
        }
        else if(strcmp(argv[ii], "-d") == 0)
        {
            g_config.D = true;
            g_config._D = true;

            ii += 1;
        }

        else if(strcmp(argv[ii], "-c") == 0 && ii + 1 < argc)
        {
            strcpy(conf_filename, argv[ii + 1]);

            ii += 2;
        }
        else if(strcmp(argv[ii], "-b") == 0 && ii + 1 < argc)
        {
            g_config.bbfile = argv[ii + 1];
            g_config._bbfile = true;

            ii += 2;
        }
        else if((strcmp(argv[ii], "-T") == 0 || strcmp(argv[ii], "-t") == 0) && ii + 1 < argc)
        {
            g_config.Tmax = stoi(argv[ii + 1]);
            g_config._Tmax = true;

            ii += 2;
        }
        else if(strcmp(argv[ii], "-p") == 0 && ii + 1 < argc)
        {
            g_config.bp = stoi(argv[ii + 1]);
            g_config._bp = true;

            ii += 2;
        }
        else if(strcmp(argv[ii], "-s") == 0 && ii + 1 < argc)
        {
            g_config.sp = stoi(argv[ii + 1]);
            g_config._sp = true;

            ii += 2;
        }
    }
}

void BbServer::TimerCallBack(client_data *user_data) {
    epoll_ctl(s_epollFd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    if (user_data) {
        close(user_data->sockfd);
        s_clientsList.remove(user_data->sockfd);
        s_isAlarm = false;
        if(g_config.D) cout << "Server: close socket fd : " << user_data->sockfd << endl;
    }
}

void BbServer::Run()
{
    CreatePidFile();
    cout << "START SERVER!" << endl;

    int ret;
    epoll_event events[MAX_EVENT_NUMBER];
    while (!m_serverStop) {
        if(!m_threadPool)
        {
            //start thread pool
            m_threadPool = new FuncThreadPool(g_config.Tmax);

            ThreadObject * sigThread = new ThreadObject([&](){
                HandleSyncServ();
            });
            sigThread->start();
        }
        int number = epoll_wait(s_epollFd, events, MAX_EVENT_NUMBER, -1); //epoll_wait
        if (number < 0 && errno != EINTR) {
            if(g_config.D) cout << "Server: epoll error" << endl;
            break;
        }

        for (int i = 0; i < number; ++i) {
            int sockfd = events[i].data.fd;
            if (sockfd == m_bp_socketFd) { //new socket connection
                struct sockaddr_in client_addr;
                socklen_t len = sizeof(client_addr);

                int conn_fd = accept(m_bp_socketFd, (struct sockaddr*)&client_addr, &len);
                Addfd(s_epollFd, conn_fd); //Enable ET mode for conn_fd
                s_clientsList.emplace_back(conn_fd);
                cout << "Server: New connect fd:" << conn_fd << " Now client number:" << s_clientsList.size() << endl;

                m_user[conn_fd].address = client_addr;
                m_user[conn_fd].sockfd = conn_fd;
                m_user[conn_fd].put_idx = 0;
                m_user[conn_fd].bp_clnt = 1;
                memset(m_user[conn_fd].buf, '\0', BUFFER_SIZE);
                memset(m_user[conn_fd].user_name, '\0', USER_NAME_SIZE);

                // Create a time heap timer
                heap_timer *timer = new heap_timer(TIMESLOT);
                timer->user_data = &m_user[conn_fd];
                timer->callBackFunc = TimerCallBack;
                m_timerHeap.AddTimer(timer);
                if (!s_isAlarm) {
                    s_isAlarm = true;
                    alarm(TIMESLOT);
                }
                m_user[conn_fd].timer = timer;

                const char * hello = "0.0 Hello\n";
                send(conn_fd, hello, strlen(hello), 0);

            } else if ((sockfd == s_pipeFd[0]) && (events[i].events & EPOLLIN)) { //process signal
                char sigBuf[BUFFER_SIZE];
                ret = recv(s_pipeFd[0], sigBuf, sizeof(sigBuf), 0);
                if (ret <= 0) continue;
                else {
                    string sigMsg(sigBuf);
                    HandleSignal(sigMsg); //custom signal handling
                }
            } else if (events[i].events & EPOLLIN) { //Readable events
                if(g_config.D) cout << "zp begin recv client=" << sockfd << endl;

                int recvRet = recv(sockfd, m_user[sockfd].buf + m_user[sockfd].put_idx, BUFFER_SIZE - 1 - m_user[sockfd].put_idx, 0);

                if (recvRet <= 0) { //The peer end normally closes the socket or ends
                    CloseOneClient(sockfd);
                    break;
                } else { //read data once
                    // Update time heap timer
                    heap_timer *timer = m_user[sockfd].timer;
                    if (timer) {
                        timer->expire += TIMESLOT;
                        // After updating the timer, you need to set that there is no scheduled task currently in progress
                        // thereby retriggering the SIGALRM signal in TimerHandler()
                        s_isAlarm = false;
                    }
                    if(recvRet > 0)
                    {
                        m_user[sockfd].put_idx += recvRet;

                        int tmp_begin = 0, tmp_end = 0;
                        for(int ii = 0; ii < m_user[sockfd].put_idx; ii++)
                        {
                            if(m_user[sockfd].buf[ii] == '\r' || m_user[sockfd].buf[ii] == '\n')
                            {
                                if(ii > tmp_begin){
                                    tmp_end = ii;
                                    char * pCmdData = (char *) malloc(tmp_end - tmp_begin + 1);
                                    memcpy(pCmdData, m_user[sockfd].buf + tmp_begin, tmp_end - tmp_begin);
                                    pCmdData[tmp_end - tmp_begin] = '\0';
                                    if(g_config.D) cout << "zp copy to pCmdData" << endl;

                                    this->m_threadPool->append([&]() {
                                        HandleBpCmd(sockfd, pCmdData);
                                    });
                                    //copy left data to beginning of buffer
                                    memcpy(m_user[sockfd].buf, m_user[sockfd].buf + ii + 1, m_user[sockfd].put_idx - ii - 1);
                                    tmp_begin = 0;
                                    m_user[sockfd].put_idx -= (ii + 1);
                                    ii = -1;
                                }
                                else
                                {
                                    tmp_begin += 1;
                                }
                            }
                        }
                    }
                }
            } else {
                cout << "Server: socket something else happened!" << endl;
            }
        }

        if (m_timeout) {
            TimerHandler();
            m_timeout = false;
        }
    }

    cout << "CLOSE SERVER!" << endl;
    CloseAllFds();
    m_threadPool->stopThreadPool();
}
void BbServer::DelayMs(int howManyMs)
{
    struct timeval timer;
    timer.tv_sec        = 0;    // 0 seconds
    timer.tv_usec       = 1000*howManyMs; // 1000us = 1ms
    select(0, NULL, NULL, NULL, &timer);
}
void BbServer::HandleSyncServ()
{
    const int MAXLINE = 8192;
    int sp_fd = m_sp_socketFd;
    int connfd;
    char buff[MAXLINE];

    while(1){
        if((connfd = accept(sp_fd, (struct sockaddr*)NULL, NULL)) == -1){
            cout << "SYNC-Slave: accept sp socket error: %s(errno: %d)" << strerror(errno) << errno << endl;
            break;
        }
        int read_pos = 0;
        //for(int n = recv(connfd, buff, MAXLINE - 1, 0); n > 0; n = recv(connfd, buff + read_pos, MAXLINE - 1 - read_pos, 0))
        //{
        //}
        memset(buff, 0, sizeof(buff));
        int n = recv(connfd, buff, MAXLINE - 1, 0);
        //if(n > 0)
        {
            cout << "SYNC-Slave: receive [" << buff << "] from master: " << connfd << endl;
            string cmd = buff;
            if(cmd.find("precommit ") != string::npos)
            {
                string ready = "ready_yes ok\n";
                cout << "SYNC-Slave: send [" << ready << "] to master: " << connfd << endl;
                send(connfd, ready.c_str(), ready.length(), 0);
                DelayMs(50);
                memset(buff, 0, sizeof(buff));
                recv(connfd, buff, MAXLINE - 1, 0);
                cout << "SYNC-Slave: receive [" << buff << "] from master: " << connfd << endl;
            }
            if(cmd.find("commit ", 9) != string::npos)
            {
                string msg_full = cmd.substr(strlen("commit ") + 1);
                int pp_pos = msg_full.find("/");
                string user = msg_full.substr(0, pp_pos);
                string msg = msg_full.substr(pp_pos + 1);
                pp_pos = msg.find("/");
                if(pp_pos == string::npos)
                {
                    //WRITE message
                    m_bb.write_msg(user, msg);
                }
                else
                {
                    //REPLACE message
                    int msg_id = stoi(msg.substr(0, pp_pos));
                    msg = msg.substr(pp_pos + 1);
                    m_bb.replace_msg(msg_id, user, msg);
                }
            }
        }
        close(connfd);
    }
}

    return NULL;
    /** TODO: create num_threads threads and initialize the thread pool **/
}

void async_run(void (*hanlder)(int), int args) {

void BbServer::HandleBpCmd(int sockfd, char * pCmd)
{
    string line(pCmd);
    if(g_config.D) cout << "zp Recv data2: " << line << endl;
    size_t pos = line.find(' ');
    if(pos!=string::npos)
    {
        string tmpKey = line.substr(0,pos);//before the = sign
        string tmpValue = line.substr(pos+1);//After taking the = sign
        if("USER"==tmpKey)
        {
            if(tmpValue.find('/') == string::npos && tmpValue.find('\n') == string::npos)
            {
                memcpy(m_user[sockfd].user_name, tmpValue.c_str(), tmpValue.length());
                stringstream ss1;
                ss1 << "1.0 Hello " << tmpValue << " !" << endl;
                tmpValue = ss1.str();
                send(sockfd, tmpValue.c_str(), tmpValue.length(), 0);
            }
            else
            {
                tmpValue = "1.2 ERROR USER !\n";
                send(sockfd, tmpValue.c_str(), tmpValue.length(), 0);
            }
            if(g_config.D) cout << "send USER response: " << tmpValue << endl;
        }
        else if("READ"==tmpKey)
        {
            int msg_id = stoi(tmpValue);
            string ret;
            if(m_bb.read_msg(msg_id, ret))
            {
                stringstream ss;
                ss << "2.0 MESSAGE " << msg_id << " " << ret << endl;
                ret = ss.str();
            }
            else
            {
                stringstream ss;
                ss << "2.1 UNKNOWN " << msg_id << " !" << endl;
                ret = ss.str();
            }

            send(sockfd, ret.c_str(), ret.length(), 0);
            if(g_config.D) cout << "send READ response: " << ret << endl;
        }
        else if("QUIT" ==tmpKey)
        {
            string ret = "4.0 BYE !\n";
            send(sockfd, ret.c_str(), ret.length(), 0);
            if(g_config.D) cout << "send QUIT response: " << ret << endl;
            CloseOneClient(sockfd);
        }
        else if("WRITE" == tmpKey || "REPLACE" == tmpKey)
        {
            bool abort = false;
            //two-phase commit
            vector<int> sp_server_fns;

            for(auto peer : g_config.peers)
            {
                int p_pos = peer.find(":");
                string addr = peer.substr(0, p_pos);
                int port = stoi(peer.substr(p_pos + 1));

                int    sp_sockfd;
                struct sockaddr_in    servaddr;

                if( (sp_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
                    printf("SYNC-Master: sp create socket error: %s(errno: %d)\n", strerror(errno),errno);
                    continue;
                }

                memset(&servaddr, 0, sizeof(servaddr));
                servaddr.sin_family = AF_INET;
                servaddr.sin_port = htons(port);
                servaddr.sin_addr.s_addr = inet_addr(addr.c_str());

                // set non-blocking for timeout
                {
                    unsigned long u1 = 1;
                    ioctl(sp_sockfd, FIONBIO, &u1);
                }

                //non-blocking connection
                /*
                if(connect(sp_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
                    printf("SYNC-Master: sp connect error: %s(errno: %d)\n",strerror(errno),errno);
                    continue;
                }
                */
                if(connect(sp_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0)
                {
                    //Detect connection timeout
                    int re = 0;
                    fd_set set;
                    struct timeval tm;

                    int len;
                    int error = -1;

                    tm.tv_sec = 30; //10 seconds timeout
                    tm.tv_usec = 0;
                    FD_ZERO(&set);
                    FD_SET(sp_sockfd, &set);

                    re = select(sp_sockfd + 1, NULL, &set, NULL, &tm);
                    if(re > 0){
                        len = sizeof(int);

                        // Get socket status
                        getsockopt(sp_sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
                        if(error == 0){
                            re = 1;
                        }
                        else{
                            re = -3;
                        }
                    }
                    else{
                        re = -2;
                    }
                    if(re <= 0)
                    {
                        continue;
                    }
                }

                // set blocking
                {
                    unsigned long u2 = 0;
                    ioctl(sp_sockfd, FIONBIO, &u2);
                }

                sp_server_fns.push_back(sp_sockfd);
            }
            if(!abort)
            {
                for(auto next_sp : sp_server_fns)
                {
                    string precommit_cmd("precommit ready\n");
                    if(send(next_sp, precommit_cmd.c_str(), precommit_cmd.length(), 0) < 0)
                    {
                        printf("SYNC-Master: sp end msg error: %s(errno: %d)\n", strerror(errno), errno);
                        abort = true;
                    }
                    cout << "SYNC-Master: send [" << precommit_cmd << "] to slave:" << next_sp << endl;
                    char ret_buf[1024];
                    memset(ret_buf, 0, sizeof(ret_buf));
                    recv(next_sp, ret_buf, sizeof(ret_buf), 0);
                    string ready = ret_buf;
                    cout << "SYNC-Master: receive [" << ready << "] from slave:" << next_sp << endl;
                    if(ready.find("ready_yes ") == string::npos)
                    {
                        abort = true;
                    }
                    memset(ret_buf, 0, sizeof(ret_buf));
                }
            }
            if(!abort)
            {
                stringstream ss;
                ss << "commit " << m_user[sockfd].user_name << "/" << tmpValue << endl;
                string sp_msg = ss.str();

                for(auto next_sp : sp_server_fns)
                {
                    cout << "SYNC-Master: send [" << sp_msg << "] to slave:" << next_sp << endl;
                    send(next_sp, sp_msg.c_str(), sp_msg.length(), 0);
                }
            }
            else
            {
                stringstream ss;
                ss << "abort !" << endl;
                string sp_msg = ss.str();

                for(auto next_sp : sp_server_fns)
                {
                    cout << "SYNC-Master: send [" << sp_msg << "] to slave:" << next_sp << endl;
                    send(next_sp, sp_msg.c_str(), sp_msg.length(), 0);
                }
            }

            //Delay 50 ms to ensure data is sent before closing the socket
            DelayMs(50);
            for(auto next_sp : sp_server_fns)
            {
                close(next_sp);
            }
            sp_server_fns.clear();

            if(abort)
            {
                stringstream ss;
                ss << "3.2 ERROR " << "can not SYNC" << endl;
                string ret = ss.str();
                send(sockfd, ret.c_str(), ret.length(), 0);
                if(g_config.D) cout << "send SYNC response: " << ret << endl;
            }
            else
            {
                if("WRITE"==tmpKey)
                {
                    int msg_id = m_bb.write_msg(m_user[sockfd].user_name, tmpValue);
                    stringstream ss;
                    ss << "3.0 WROTE " << msg_id << endl;
                    string ret = ss.str();

                    send(sockfd, ret.c_str(), ret.length(), 0);
                    if(g_config.D) cout << "send WRITE response: " << ret << endl;
                }
                else
                {
                    int split_pos = tmpValue.find('/');
                    int msg_id = stoi(tmpValue.substr(0, split_pos));
                    string replace_msg = tmpValue.substr(split_pos + 1);

                    string ret;
                    if(m_bb.replace_msg(msg_id, m_user[sockfd].user_name, replace_msg))
                    {
                        stringstream ss;
                        ss << "3.0 WROTE " << msg_id << endl;
                        ret = ss.str();
                    }
                    else
                    {
                        stringstream ss;
                        ss << "3.1 UNKNOWN " << msg_id << endl;
                        ret = ss.str();
                    }
                    send(sockfd, ret.c_str(), ret.length(), 0);
                    if(g_config.D) cout << "send REPLACE response: " << ret << endl;
                }
            }
        }
    }
    free((void *) pCmd);
}

void BbServer::CreatePidFile()
{
    pid_t this_pid = getpid();
    ofstream ofs("bbserv.pid");
    ofs << this_pid;
    ofs.close();
}

void BbServer::CloseAllFds()
{
    close(m_bp_socketFd);
    close(m_sp_socketFd);
    close(s_epollFd);
    close(s_pipeFd[0]);
    close(s_pipeFd[1]);

    m_bb.close_bbfile();
}

void BbServer::CloseAllClients()
{
    for (auto &cln_fd : s_clientsList) {
        CloseOneClient(cln_fd);
    }
    hanlder(args);
    /** TODO: rewrite it to support thread pool **/
}