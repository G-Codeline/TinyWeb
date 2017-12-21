/*
*Author:GeneralSandman
*Code:https://github.com/GeneralSandman/TinyWeb
*E-mail:generalsandman@163.com
*Web:www.generalsandman.cn
*/

/*---XXX---
*
****************************************
*
*/
 
#include "timerqueue.h"
#include "timer.h"
#include "channel.h"
#include "log.h"

#include <sys/timerfd.h>
#include <boost/bind.hpp>
#include <unistd.h>
#include <set>

struct timespec howMuchTimeFromNow(Time when)
{
    int64_t microseconds = when.microSecondsSinceEpoch() - Time::now().microSecondsSinceEpoch();
    if (microseconds < 100)
    {
        microseconds = 100;
    }
    struct timespec ts;
    ts.tv_sec = static_cast<time_t>(
        microseconds / Time::kMicroSecondsPerSecond);
    ts.tv_nsec = static_cast<long>(
        (microseconds % Time::kMicroSecondsPerSecond) * 1000);
    return ts;
}

void TimerQueue::m_fHandleRead()
{
    //get happened and invoke the timer's callback
    //if a timer will repet,reset
    //else delete this timer

    std::vector<Timer *> happened;
    m_fGetHappen(happened);
    for (auto t : happened)
        t->run();

    m_fResetHappened(happened);
}

void TimerQueue::m_fInsertTimer(Timer *timer)
{
    bool mustResetTimeFd = false;
    Time time = timer->getTime();
    if (m_nTimers.empty())
        mustResetTimeFd = true;
    else if (time < m_nTimers.begin()->first)
        mustResetTimeFd = true;

    m_nTimers.insert(std::pair<Time, Timer *>(time, timer));
    if (mustResetTimeFd)
        m_fResetTimeFd(time);
    //
    //m_fResetTimeFd():If timer which we have added will happen before 
    //      make sure this timer can be invoked.
}

void TimerQueue::m_fGetHappen(std::vector<Timer *> &happened)
{
    Time time = Time::now();
    std::pair<Time, Timer *> flag = std::pair<Time, Timer *>(time, nullptr);
    std::set<std::pair<Time, Timer *>>::iterator end = m_nTimers.lower_bound(flag);

    std::vector<std::pair<Time, Timer *>> res;
    std::copy(m_nTimers.begin(), end, std::back_inserter(res));
    for (auto t : res)
    {
        happened.push_back(t.second);
    }
    m_nTimers.erase(m_nTimers.begin(), end);
}

void TimerQueue::m_fResetHappened(std::vector<Timer *> &happened)
{
    for (auto t : happened)
    {
        if (t->isRepet())
        {
            t->reset();
            m_fInsertTimer(t); //reinsert timers which want repet.
        }
        else
        {
            delete t;
            t = nullptr;
        }
    }
}

void TimerQueue::m_fResetTimeFd(Time expiration)
{
    struct itimerspec newValue;
    bzero(&newValue, sizeof newValue);
    newValue.it_value = howMuchTimeFromNow(expiration);
    int ret = ::timerfd_settime(m_nFd, 0, &newValue, NULL);
}

TimerQueue::TimerQueue(EventLoop *loop)
{
    //create a timer file descriptor:no block and close-on-exec
    //create a Channel to handle timer's read event.
    m_nFd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    m_pTimeChannel = new Channel(loop, m_nFd);
    m_pTimeChannel->setReadCallback(boost::bind(&TimerQueue::m_fHandleRead, this));
    m_pTimeChannel->enableRead();

    LOG(Debug) << "class TimerQueue constructor\n";
}

void TimerQueue::addTimer(Time &time, timerReadCallback c, bool repet, double interval)
{
    //create timer and add it to queue.
    Timer *timer = new Timer(time, c, repet, interval);
    m_fInsertTimer(timer);
}

TimerQueue::~TimerQueue()
{
    //delete all Timers,
    //close timer file descriptor,
    //delete Channel
    for (auto t : m_nTimers)
        delete t.second;
    close(m_nFd);
    delete m_pTimeChannel;
    m_pTimeChannel = nullptr;

    LOG(Debug) << "class TimerQueue destructor\n";
}