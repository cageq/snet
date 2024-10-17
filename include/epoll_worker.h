#pragma once
#include <inttypes.h>
#include <unistd.h>
#include <stdio.h>
#include <list>
#include <errno.h>
#include <sys/epoll.h>
#include <unordered_map>
#include <sys/timerfd.h>
#include <stdint.h>
#include <time.h>
#include "tcp_factory.h"
#include <vector>
#define MAX_WAIT_EVENT 128

namespace snet
{

    template <class Connection>
    class EpollWorker
    {
    public:
        using ConnectionPtr = std::shared_ptr<Connection>;
        using TcpFactoryPtr = TcpFactory<Connection> *;
        int32_t start(TcpFactoryPtr factory)
        {
            tcp_factory = factory;
            epoll_fd = epoll_create(10);
            timer_fd = timerfd_create(CLOCK_REALTIME, 0);
            if (timer_fd == -1)
            {
                printf("timerfd_create failed\n");
                return -1;
            }

            is_running = true;
            epoll_thread = std::thread([this]
                                       {
                                       add_timer();
                                       run(); });

            return 0;
        }

        bool add_event(ConnectionPtr conn, int32_t evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
        {
            if (conn->conn_sd > 0)
            {

                online_connections[conn->conn_sd] = conn;
                struct epoll_event event = {};
                event.data.ptr = conn.get();
                // conn.use_count() ++;
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
            return false;
        }

        bool mod_event(Connection *conn, int evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
        {
            if (conn->conn_sd > 0)
            {
                struct epoll_event event = {};
                event.events = evts;
                event.data.ptr = conn;
                int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->conn_sd, &event);
                if (ret == -1)
                {
                    printf("mod epoll event error");
                }
                else
                {
                    // printf("mod epoll event success\n");
                }
                return ret >= 0;
            }
            return false;
        }

        bool del_event(Connection *conn)
        {
            if (conn->conn_sd > 0)
            {

                online_connections.erase((uint64_t)conn->conn_sd);
                int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->conn_sd, nullptr);
                if (ret == -1)
                {
                    printf("del epoll event failed\n");
                }
                else
                {
                    printf("del epoll event success %d\n", conn->conn_sd);
                }
                return ret >= 0;
            }
            return false;
        }

        bool add_timer(bool mod = false, uint32_t interval = 1, uint32_t delay = 1)
        {
            struct itimerspec timerInterval;
            timerInterval.it_value.tv_sec = delay;
            timerInterval.it_value.tv_nsec = 0;
            timerInterval.it_interval.tv_sec = interval;
            timerInterval.it_interval.tv_nsec = 0;

            if (timerfd_settime(timer_fd, 0, &timerInterval, nullptr) == -1)
            {
                printf("timerfd_settime failed\n");
                return false;
            }

            struct epoll_event event
            {
            };
            event.data.fd = timer_fd;
            event.events = EPOLLIN;
            if (mod)
            {
                if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, timer_fd, &event) == -1)
                {
                    printf("epoll_ctl mode failed \n");
                    return false;
                }
            }
            else
            {

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_fd, &event) == -1)
                {
                    printf("epoll_ctl add failed\n");
                    return false;
                }
            }

            return true;
        }

        void release(uint64_t cid, ConnectionPtr conn)
        {

            release_connections.push_back(conn);
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
                    if (evfd == timer_fd)
                    {
                        add_timer(true);

                        for (auto conn : release_connections)
                        {
                            conn->do_close();
                            tcp_factory->remove_connection(conn->get_cid());
                        }
                        release_connections.clear();
                        continue;
                    }

                    // TODO not safe,but efficiency
                    Connection *conn = (Connection *)waitEvents[i].data.ptr;
                    if (conn)
                    {
                        conn->process_event(waitEvents[i].events);
                    }
                    else
                    {
                        printf("no found connection \n");
                    }
                }

            } // end while

            printf("quit process thread");
            return 0;
        }

        std::vector<ConnectionPtr> release_connections;
        std::unordered_map<uint64_t, ConnectionPtr> online_connections;
        int timer_fd = -1;
        bool is_running = false;
        int epoll_fd = -1;
        std::thread epoll_thread;
        TcpFactoryPtr tcp_factory = nullptr;
    };

}