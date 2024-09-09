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
    bool event_add(ConnectionPtr conn, int32_t evts = EPOLLIN | EPOLLOUT | EPOLLERR)
    {
        auto &event = conn->event;
        event.events = evts;

        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->conn_sd, &event);
        if (ret != 0)
        {
            ::close(conn->conn_sd);
            return false;
        }

        return true;
    }

    void event_mod(ConnectionPtr conn, int evts)
    {
        auto &event = conn->event;
        event.events = evts;
        event.data.ptr = conn;
        int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->conn_sd, &event);
        if (ret == -1)
        {
            printf("mod epoll event error");
        }
        else
        {
        }
    }

    void add_timer()
    {
 
        struct itimerspec timerInterval;

        // 设置定时器的初始时间和间隔时间
        timerInterval.it_value.tv_sec = 2; // 2 秒后第一次触发
        timerInterval.it_value.tv_nsec = 0;
        timerInterval.it_interval.tv_sec = 2; // 每隔 2 秒触发一次
        timerInterval.it_interval.tv_nsec = 0;

        if (timerfd_settime(timer_fd, 0, &timerInterval, NULL) == -1)
        {
            printf("timerfd_settime");
        }


        struct epoll_event event {}; 
        // 将 timerfd 注册到 epoll 实例中，监听其读事件
        event.data.fd = timer_fd;
        event.events = EPOLLIN; // 监听可读事件
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1)
        {
            printf("epoll_ctl");
        }

        printf("Waiting for timer events...\n");
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
                    printf("timeout\n"); 
                    add_timer(); 
                    continue;
                }


                Connection *pConn = (Connection *)waitEvents[i].data.ptr;
                auto conn = std::shared_ptr<Connection>(pConn);
                if (evfd == conn->conn_sd)
                {

                    // if (!is_running)
                    // {
                    //     for (auto &c : connections)
                    //     {
                    //         this->close_connection(c);
                    //     }
                    //     connections.clear();
                    // }
                    // // it's useless to check the event value
                    // // eventfd_t  eval  = 0 ;
                    // // eventfd_read(m_send_event_fd, &eval);
                    // // printf("read event type is %ld", eval );
                    // this->notify_queue.process([&](Connection *connection) {
                    //                 if (connection->status == CONN_CLOSING)
                    //                 {
                    //                     connections.remove(connection);
                    //                     close_connection(connection);
                    //                 }  else {
                    //                         process_send(connection);
                    //                 }

                    //             return true;
                    //         });
                    continue;
                }

                // if (CONN_INIT == conn->status)
                // {
                //     printf("add new fd to proc thread");
                //     conn->status = CONN_OPEN;
                //     connections.emplace_back(conn);
                //     conn->on_connect();
                // }
                // else if (CONN_CLOSING == pConnection->status)
                // {
                //     printf("connection is closing");
                //     connections.remove(conn);
                //     this->close_connection(conn);
                //     continue;
                // }

                if (EPOLLIN == (waitEvents[i].events & EPOLLIN))
                {
                    conn->do_read();
                }
                else if (EPOLLOUT == (waitEvents[i].events & EPOLLOUT))
                {
                    conn->do_send();
                }
                else if (EPOLLERR == (waitEvents[i].events & EPOLLERR))
                {
                    // printf("EPOLLERROR event %d ", waitEvents[i].events);
                    // connections.remove(conn);
                    // this->close_connection(conn);
                    conn->do_close();
                }
                else
                {
                    // printf("epoll other event  %d ", waitEvents[i].events);
                    // connections.remove(conn);
                    // this->close_connection(conn);
                    conn->do_close();
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