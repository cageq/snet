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
#include "epoll_worker.h"
#include "net_url.h"

namespace snet
{

	template <class Connection, class Factory = TcpFactory<Connection>>
	class TcpListener : public Factory, public EpollEventHandler
	{
	public:
		using ConnectionPtr = std::shared_ptr<Connection>;
		using TcpWorker = EpollWorker ;
		using TcpWorkerPtr = std::shared_ptr<TcpWorker>;

		TcpListener(Factory *factory, std::vector<TcpWorkerPtr> workers)  
		{
			connection_factory = factory == nullptr? this : factory;
			listen_worker = std::make_shared<TcpWorker>();
			listen_worker->start();
			for (auto worker : workers)
			{
				tcp_workers.emplace_back(worker);
			}
		}

		TcpListener(Factory *factory = nullptr, int32_t workers = 1)  
		{
			listen_worker = std::make_shared<TcpWorker>();

			connection_factory = factory == nullptr? this : factory;
 
			listen_worker->start();

			for (int i = 0; i < workers; i++)
			{
				auto worker = std::make_shared<TcpWorker>();
				worker->start();
				tcp_workers.emplace_back(worker);
			}
		}

		int start(const std::string & url ){
			NetUrl netUrl ; 
			netUrl.parse(url); 
			return start(netUrl.port, netUrl.host); 
		}


		int start(uint16_t port, const std::string &host = "0.0.0.0", uint32_t acceptThrds = 1)
		{		 
			listen_addr = host;
			printf("start listen  %s:%d\n", host.c_str(), port); 	 
			listen_sd = socket(AF_INET, SOCK_STREAM, 0);
			signal(SIGPIPE, SIG_IGN);
			if (listen_sd < 0)
			{
				perror("socket() failed");
				exit(-1);
			}
			set_reuse();
			// set_nonblocking(listen_sd);
			do_bind(port, host.c_str());
			do_listen();

			listen_worker->add_event(listen_sd, this); 

			listen_worker->start_timer([this](){
				if (connection_factory){
					connection_factory->clear_released();
				}				
				return true; 
			}, 10000, true ); 
			return 0;
		}

		void stop()
		{
			if (is_running)
			{
				is_running = false;
				::close(listen_sd);
			}
			listen_worker->stop();

			for (auto &worker : tcp_workers)
			{
				worker->stop();
			}
		}

		void broadcast(const std::string &msg)
		{
			for (auto item : connection_factory->connection_map)
			{
				if (item.second)
				{
					item.second->msend(msg);
				}
			}
		}

		uint32_t start_timer(const TimerHandler &handler, uint32_t interval, bool loop = true)
		{
			return listen_worker->start_timer(handler, interval, loop);
		}

		virtual void process_event(int32_t evts)
		{

			if (EPOLLIN == (evts & EPOLLIN))
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
						return;
					}
					else
					{
						printf("accept failed, errno %d ", errno);
						return;
					}
				}

				set_nodelay(clientFd);
				// set_noblock(clientFd);

				ConnectionPtr conn  = connection_factory->create();

				auto worker = get_worker();
				conn->tcp_worker = worker;

				char remoteHost[INET_ADDRSTRLEN] = {};
				inet_ntop(AF_INET, &cliAddr.sin_addr, remoteHost, sizeof(remoteHost));
				int32_t remotePort = ntohs(cliAddr.sin_port);

				printf("accept new connection from %s:%u\n", remoteHost, remotePort);
				conn->init(clientFd, remoteHost, remotePort, true); 
		 
				worker->add_event(clientFd, conn.get());
			}
		}

	private:
	 

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
				perror("setsockopt(reuse address) failed");
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
				perror("bind() failed, exit");
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
			worker_index++;
			worker_index = worker_index % tcp_workers.size();
			return tcp_workers[worker_index];
		}

		uint32_t worker_index = 0;
 
		int listen_sd = -1;
		bool is_running = false;

		std::string listen_addr;
		Factory *connection_factory = nullptr;

		TcpWorkerPtr listen_worker;
		std::vector<TcpWorkerPtr> tcp_workers; 
	 
	};

}