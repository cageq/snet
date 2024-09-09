#pragma once
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <list>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <stdint.h>
#include <time.h>
#define MAX_WAIT_EVENT 128 



template <class Connection>
class EpollWorker
{
public:
    using ConnectionPtr = std::shared_ptr<Connection>;
    std::list<ConnectionPtr> connections;

    int32_t start()
    {

        epoll_fd  =  epoll_create(10);

          // 创建一个 timerfd 实例
        timer_fd = timerfd_create(CLOCK_REALTIME, 0);
        if (timer_fd == -1)
        {
            printf("timerfd_create");
        }

        is_running = true;
        epoll_thread = std::thread([this]{ 
            add_timer(); 
            run(); 
                                   
        });
        return 0;
    }
    bool add_event(Connection *  conn, int32_t evts = EPOLLIN | EPOLLOUT | EPOLLERR)
    {
        
        struct epoll_event event{}; 
        event.data.ptr = conn;  

        printf("put pointer %p to event  sd is %d\n", conn, conn->conn_sd); 
 
        event.events = evts; 
        
        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->conn_sd, &event);
        if (ret != 0)
        {
            printf("add conn to worker failed\n"); 
            ::close(conn->conn_sd);
            return false;
        }

        return true;
    }

    void mod_event(Connection *  conn, int  evts = EPOLLIN | EPOLLOUT | EPOLLERR)
    {
        struct epoll_event event{}; 
        event.events = evts;
        event.data.ptr = conn ;
        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->conn_sd, &event);
        if (ret == -1)
        {
            printf("mod epoll event error");
        }
        else
        {
        }
    }

    void add_timer(bool mod = false)
    {
 
        struct itimerspec timerInterval;
        // 设置定时器的初始时间和间隔时间
        timerInterval.it_value.tv_sec = 2; // 2 秒后第一次触发
        timerInterval.it_value.tv_nsec = 0;
        timerInterval.it_interval.tv_sec = 2; // 每隔 2 秒触发一次
        timerInterval.it_interval.tv_nsec = 0;

        if (timerfd_settime(timer_fd, 0, &timerInterval, nullptr) == -1)
        {
            printf("timerfd_settime failed\n");
        }

        struct epoll_event event {}; 
        // 将 timerfd 注册到 epoll 实例中，监听其读事件
        event.data.fd = timer_fd;
        event.events = EPOLLIN; // 监听可读事件
        if (mod ){
            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, timer_fd, &event) == -1)
            {
                printf("epoll_ctl mode failed \n");
            }
        }else {

            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1)
            {
                printf("epoll_ctl add failed\n");
            }
        }
 
    }

    void *run()
    {

        struct epoll_event waitEvents[MAX_WAIT_EVENT];
        while (is_running)
        {
            int ret = epoll_wait(epoll_fd, waitEvents, MAX_WAIT_EVENT, -1);
            if (ret < 0)
            {
                printf("wait error , errno is %d\n", errno);
                continue;
            }

            for (int i = 0; i < ret; i++)
            {
                int evfd = waitEvents[i].data.fd;
                if (evfd == timer_fd){
                    // printf("timeout\n"); 
                    add_timer(true); 
                    continue;
                }
                printf("get connection %d pointer %p  \n", i,  waitEvents[i].data.ptr ); 
                Connection *conn = (Connection *)waitEvents[i].data.ptr;
               
                if (conn){
                    conn->process_event(waitEvents[i].events); 

                    this->mod_event(conn,  EPOLLIN  | EPOLLERR); 
                }else {
                    printf("no found connection \n"); 
                }

            }

        } // end while

        // for (auto &conn : connections)
        // {
        //     this->close_connection(conn);
        // }
        printf("quit process thread");
        connections.clear();
        return 0;
    }
    int timer_fd; 
    bool is_running = false;
    int epoll_fd;
    std::thread epoll_thread;
};
