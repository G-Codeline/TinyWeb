/*
 *Author:GeneralSandman
 *Code:https://github.com/GeneralSandman/TinyWeb
 *E-mail:generalsandman@163.com
 *Web:www.dissigil.cn
 */

/*---XXX---
 *
 ****************************************
 *
 */

#ifndef PROCESS_H
#define PROCESS_H

#include <tiny_base/api.h>
#include <tiny_base/buffer.h>
#include <tiny_base/log.h>
#include <tiny_base/signalmanager.h>
#include <tiny_base/sync.h>
#include <tiny_core/socketpair.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <iostream>
#include <signal.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>

#include <tiny_core/status.h>
extern int status_quit_softly; //QUIT
extern int status_terminate;   //TERM,INT
extern int status_exiting;
extern int status_restart;
extern int status_reconfigure; //HUP,reboot
extern int status_child_quit;  //CHLD

typedef boost::function<void()> Fun;

void test_child_MessageCallback(Connection* con, Buffer* buf, Time time);
void test_child_CloseCallback(Connection* con);
void test_child_period_print(void);
class EventLoop;
class Slave;

class Process : boost::noncopyable {
private:
    EventLoop* m_pEventLoop;
    Slave* m_pSlave;
    std::string m_nName;
    int m_nNumber;
    pid_t m_nPid;
    SocketPair m_nPipe;

    SignalManager m_nSignalManager;
    int status;

    std::shared_ptr<Sync> m_pSync;

    static Process* m_pProcessInstance;

    static void childSignalHandler(int sign)
    {
        m_fSignalHandler(m_pProcessInstance, sign);
    }

    static void m_fSignalHandler(Process* proc, int sign)
    {
        assert(nullptr != proc);
        if (nullptr == proc)
            return;
        pid_t pid = getpid();
        LOG(Debug) << "[child] (" << pid << ") signal manager get signal(" << sign << ")\n";

        switch (sign) {
        case SIGINT:
        case SIGTERM:
            status_terminate = 1;
            LOG(Debug) << "[child] (" << pid << ") will terminate\n";
            break;
        case SIGQUIT:
            status_quit_softly = 1;
            LOG(Debug) << "[child] (" << pid << ") will quit softly\n";
            break;
        case SIGPIPE:
            break;
        case SIGUSR1:
            status_restart = 1;
            LOG(Debug) << "[child] (" << pid << ") restart\n";
            break;
        case SIGUSR2:
            status_reconfigure = 1;
            LOG(Debug) << "[child] (" << pid << ") reload\n";
            break;
        default:
            break;
        }
    }

public:
    Process(const std::string& name, int number, int sockfd[2],
    std::shared_ptr<Sync> sync);
    void setAsChild(int port);
    void createListenServer(int listen);
    void setSignalHandlers();
    void start();
    pid_t getPid();
    bool started();
    int join();


    void writeToShareMemory(const std::string& data)
    {
        void* address = nullptr;
        m_pSync->sem->lock();
        address = m_pSync->memory->getSpace();
        memcpy(address, (const void*)data.c_str(), data.size());
        m_pSync->sem->unLock();
    }

    std::string readFromSharedMemory(void)
    {
        char* address = nullptr;
        unsigned int len = 0;
        std::string res;


        m_pSync->sem->lock();
        address = (char*)m_pSync->memory->getSpace();
        
        len = strlen(address);
        res.reserve(len);
        res.assign((const char*)address, len);

        m_pSync->sem->unLock();

        return res;
    }

    ~Process();
    friend class ProcessPool;
};

#endif // !PROCESS_H
