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
#include <vector>
#include "utils/heap_timer.h"

#define MAX_WAIT_EVENT 128

using namespace snet::utils; 
namespace snet
{
	namespace tcp{


		class EpollEventHandler
		{
			public:
				virtual void process_event(int32_t events) = 0;
		};

		class EpollWorker : public HeapTimer<std::chrono::microseconds>
		{
			public:
				int32_t start()
				{ 
					epoll_fd = epoll_create(10);
					int ret = fcntl(epoll_fd, F_SETFD, FD_CLOEXEC);
					if (ret < 0)
					{
						printf("set epoll option failed");
						return -1;
					}

					timer_fd = timerfd_create(CLOCK_REALTIME, 0);
					if (timer_fd == -1)
					{
						printf("timerfd_create failed\n");
						return -1;
					}

					is_running = true;
					epoll_thread = std::thread([this]  { add_timer(); run(); });
					return 0;
				}

				bool add_event(int32_t sd, EpollEventHandler *handler, int32_t evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
				{
					if (sd > 0)
					{ 
						struct epoll_event event = {};
						event.data.ptr = handler;
						// conn.use_count() ++;
						event.events = evts;
						int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sd, &event);
						if (ret != 0)
						{
							printf("add conn to worker failed\n");
							::close(sd);
							return false;
						}

						return true;
					}
					return false;
				}

				bool mod_event(int32_t sd, EpollEventHandler *handler, int evts = EPOLLET | EPOLLIN | EPOLLOUT | EPOLLERR)
				{
					if (sd > 0)
					{
						struct epoll_event event = {};
						event.events = evts;
						event.data.ptr = handler;
						int ret = epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sd, &event);
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

				bool del_event(int32_t sd)
				{
					if (sd > 0)
					{
						int ret = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sd, nullptr);
						if (ret == -1)
						{
							printf("del epoll event failed\n");
						}
						else
						{
							printf("del epoll event success %d\n", sd);
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
					timerInterval.it_interval.tv_sec = interval/1000000; //microseconds 
					timerInterval.it_interval.tv_nsec = (interval%1000000) * 1000;

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
								add_timer(true,timer_loop() );  
								continue;
							}

							EpollEventHandler *evtHandler = (EpollEventHandler *)waitEvents[i].data.ptr;
							if (evtHandler)
							{
								evtHandler->process_event(waitEvents[i].events);
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

				void stop()
				{
					is_running = false;
					epoll_thread.join();
				}

				int timer_fd = -1;
				bool is_running = false;
				int epoll_fd = -1;
				std::thread epoll_thread;
		};
		using SNetWorker = EpollWorker; 
	}
}

