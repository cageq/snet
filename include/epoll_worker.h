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
#include "tcp_factory.h"
#include <vector> 
#include "shared_ptr.h"
#define MAX_WAIT_EVENT 128

template <class Connection>
class EpollWorker
{
public:
    using ConnectionPtr = SharedPtr<Connection>;
    using TcpFactoryPtr =   TcpFactory<Connection> *  ; 
    int32_t start(TcpFactoryPtr factory )
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
        epoll_thread = std::thread([this] {
						  add_timer();
						  run();
                      });
    
        return 0;
    }

    bool add_event(const ConnectionPtr & conn, int32_t evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
    {
        if (conn->conn_sd > 0 )
        {
            struct epoll_event event ={};
            //event.data.u64 = *((uint64_t *) &conn);
			//when put into epoll, increase a ref count, avoid dangling pointer 
			new (&event.data.u64)ConnectionPtr(conn); 
            event.events = evts;
            int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->conn_sd, &event);
            if (ret != 0)
            {
                printf("add conn to worker failed\n");
                ::close(conn->conn_sd);
				conn.release(); 
                return false;
            }

            return true;
        }
        return false;
    }

    bool mod_event(const ConnectionPtr& conn, int evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
    {
        if (conn->conn_sd > 0 )
        {
            struct epoll_event event= {};
            event.events = evts;
            event.data.u64 = *((uint64_t *) &conn);
            int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->conn_sd, &event);
            if (ret == -1)
            {
                printf("mod epoll event error");
            }
            else
            {
                //printf("mod epoll event success\n");
            }
            return ret >= 0;
        }
        return false;
    }

    bool del_event(const ConnectionPtr  & conn )
    {
        if (conn->conn_sd > 0 ){
      
            int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->conn_sd, nullptr);
            if (ret == 0)
            {
                printf("del event success %d, use count %d\n", conn->conn_sd,conn.use_count() );
				tcp_factory->remove_connection(conn->get_cid()); 
				conn.release(); 
            }
            else
            {
                printf("del epoll event failed , errno %d\n", errno);
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

        struct epoll_event event { };
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
                    continue;
                }

				//get rid of compiler warning 
				void * pObj = (void *) &waitEvents[i].data.u64; 
                ConnectionPtr &conn = *((ConnectionPtr*) pObj);
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

    int timer_fd = -1 ;
    bool is_running = false;
    int epoll_fd = -1 ;
    std::thread epoll_thread;
    TcpFactoryPtr  tcp_factory = nullptr; 
};
