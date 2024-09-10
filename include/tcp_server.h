#pragma once
#include <functional>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <vector>
#include <signal.h>
#include <string>
#include <arpa/inet.h>
#include <unordered_map>
#include "tcp_connection.h"
#include "heap_timer.h"

#include "epoll_worker.h"

template <class Connection, class Factory = TcpFactory<Connection>>
class TcpServer : public HeapTimer<> , public Factory
{

public:
	using ConnectionPtr = std::shared_ptr<Connection>;
	using TcpWorker = EpollWorker<Connection>;
	using TcpWorkerPtr = std::shared_ptr<TcpWorker>;

	TcpServer(Factory *factory = nullptr, int32_t workers = 4) : connection_factory(factory)
	{ 
		if (connection_factory == nullptr){
			connection_factory = this; 
		}

		for (int i = 0; i < workers; i++)
		{
			auto worker = std::make_shared<TcpWorker>();
			worker->start(connection_factory);
			tcp_workers.push_back(worker);
		}
	}

	int start(uint16_t port, const std::string &host = "0.0.0.0", uint32_t acceptThrds = 1)
	{
		listen_epoll_fd = epoll_create(10);
		if (-1 == listen_epoll_fd)
		{
			perror("create epoll error ");
			return -1;
		}
		listen_addr = host;
		printf("start listen  %s:%d\n", host.c_str(), port );		

		int ret = fcntl(listen_epoll_fd, F_SETFD, FD_CLOEXEC);
		if (ret < 0)
		{
			printf("set epoll option failed");
			return -1;
		}

		listen_sd = socket(AF_INET, SOCK_STREAM, 0);
		signal(SIGPIPE, SIG_IGN);
		if (listen_sd < 0)
		{
			perror("socket() failed");
			exit(-1);
		}
		set_reuse();
		set_nonblocking(listen_sd);
		do_bind(port, host.c_str());
		do_listen();	

		for (uint32_t i = 0; i < acceptThrds; ++i)
		{
			listen_threads.emplace_back([this](){ run();});
		}	

		return 0;
	}

	void stop()
	{
		if (is_running)
		{
			is_running = false;
			::close(listen_sd);
		}
 
		for (auto &th : listen_threads)
		{
			if (th.joinable())
			{
				th.join(); // 等待线程结束
			}
		}
	}
 
	void broadcast(const std::string & msg ){

		for (auto item :connection_factory->connection_map){
			if (item.second){
				item.second->msend(msg); 
			}
		}
	}

private:

	void run()
	{
		is_running = true;
		struct epoll_event event{};
		event.data.fd = listen_sd;
		event.events  = EPOLLET| EPOLLIN | EPOLLERR;
		// register listen fd to epoll fd
		int ret = epoll_ctl(listen_epoll_fd, EPOLL_CTL_ADD, listen_sd, &event);
		if (-1 == ret)
		{
			// trace;
			printf("add listen fd to epoll fd failed\n");
			is_running = false; 
			return ; 
		}

		// struct sockaddr_in addr;
		struct epoll_event waitEvents[MAX_WAIT_EVENT] = {};
		while (is_running)
		{
			printf("wait listener epoll\n");
			// wait untill events
			int nFds = epoll_wait(listen_epoll_fd, waitEvents, MAX_WAIT_EVENT, -1);
			if (nFds < 0)
			{
				if (errno == EAGAIN || errno == EWOULDBLOCK) {

					continue;
				}else {
					printf("wait error , errno is %d\n", errno); 
					break; 
				}	
				
			}

			for (int i = 0; i < nFds; i++)
			{
				// new connection, event fd will be equal listen fd
				if ((listen_sd == waitEvents[i].data.fd) && ((EPOLLIN == waitEvents[i].events) & EPOLLIN))
				{
					struct sockaddr_in cliAddr;
					int addrlen = sizeof(cliAddr);
	 
					// int clientFd = accept(listen_sd, (struct sockaddr *)&cliAddr, (socklen_t*)&addrlen);
					int clientFd = accept4(listen_sd, (struct sockaddr *)&cliAddr, (socklen_t *)&addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC); 
					if (clientFd < 0)
					{
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
						{
							// non-blocking  mode has no connection
							continue;
						}
						else
						{
							printf("accept failed, errno %d ", errno);
							return;
						}
					}

					set_nodelay(clientFd);
					//set_noblock(clientFd);
 
					ConnectionPtr conn;
					if (connection_factory != nullptr)
					{
						conn = connection_factory->create();
					}
					else
					{
						conn = std::make_shared<Connection>();
					}
 
					auto worker = get_worker();   
					conn->tcp_worker = worker; 
 
					char remoteHost[INET_ADDRSTRLEN] ={};
    				inet_ntop(AF_INET, &cliAddr.sin_addr, remoteHost, sizeof(remoteHost));
					int32_t remotePort = ntohs(cliAddr.sin_port);

					printf("accept new connection from %s:%u\n", remoteHost, remotePort);
					conn->init(clientFd, remoteHost, remotePort, true );
					worker->add_event(conn.get());			
				}
			}
		}

		printf("quiting listen thread ");
	}

	/*************************************************************/
	/* Allow socket descriptor to be reuseable                   */
	/*************************************************************/
	void set_reuse()
	{
		int on = 1;
		int rc = setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR,
							(char *)&on, sizeof(on));
		if (rc < 0)
		{
			perror("setsockopt() failed");
			::close(listen_sd);
			exit(-1);
		}
	}

	static void set_nodelay(int sock)
	{
		int opts = fcntl(sock, F_GETFL, 0);
		fcntl(sock, F_SETFL, opts | O_NONBLOCK);
	}

	static void set_noblock(int sock)
	{
		int opts = fcntl(sock, F_GETFL, 0);
		if (opts < 0)
		{
			printf("fcntl(sock,GETFL)");
			return;
		}
		if (fcntl(sock, F_SETFL, opts | O_NONBLOCK) < 0)
		{
			printf("fcntl(sock,SETFL,opts)");
		}
	}

	/*************************************************************/
	/* Set socket to be nonblocking. All of the sockets for      */
	/* the incoming connections will also be nonblocking since   */
	/* they will inherit that state from the listening socket.   */
	/*************************************************************/
	void set_nonblocking(int sd)
	{
		int on = 1;
		int rc = ioctl(sd, FIONBIO, (char *)&on);
		if (rc < 0)
		{
			perror("ioctl() failed");
			::close(sd);
			exit(-1);
		}
	}

	void do_bind(uint16_t port, const char *ipAddr = "0.0.0.0")
	{

		// struct sockaddr_in6   addr;
		// memset(&addr, 0, sizeof(addr));
		// addr.sin6_family      = AF_INET6;
		// memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
		// addr.sin6_port        = htons(port);

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(ipAddr);
		addr.sin_port = htons(port);
		int rc = bind(listen_sd, (struct sockaddr *)&addr, sizeof(sockaddr_in));
		if (rc < 0)
		{
			perror("bind() failed");
			::close(listen_sd);
			exit(-1);
		}
	}

	void do_listen()
	{
		/*************************************************************/
		/* Set the listen back log                                   */
		/*************************************************************/
		int rc = listen(listen_sd, 32);
		if (rc < 0)
		{
			perror("listen() failed");
			::close(listen_sd);
			exit(-1);
		}
	}

	TcpWorkerPtr get_worker()
	{
		worker_index ++; 
		worker_index = worker_index % tcp_workers.size();
		return tcp_workers[worker_index];
	}

	uint32_t worker_index = 0;
	int listen_epoll_fd = -1;

	int listen_sd = -1;
	bool is_running = false;

	std::vector<std::thread> listen_threads;
 

	std::string listen_addr;
	Factory *connection_factory = nullptr;

	std::vector<TcpWorkerPtr> tcp_workers;
};
