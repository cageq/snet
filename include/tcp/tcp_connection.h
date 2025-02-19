#pragma once

#include <cstdio>
#include <errno.h>
#include <memory>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <functional>
#include <mutex>
#include <string>
#include <atomic>

#include "epoll_worker.h"
#include "tcp_factory.h"
#include "utils/string_thief.h"
#include "utils/snet_compat.h"
#include "snet_handler.h"

// inline void string_resize(std::string &str, std::size_t sz)
//{
//	str.resize(sz);
// }

namespace snet
{
	namespace tcp
	{
		enum ConnStatus
		{
			CONN_IDLE,
			CONN_OPEN,
			CONN_CLOSING,
			CONN_CLOSED,
		};

		template <class T>
		class TcpConnection : public std::enable_shared_from_this<T>, public EpollEventHandler
		{

		public:
			template <class Connection>
			friend class TcpFactory;
			template <class Connection, class Factory>
			friend class TcpConnector;

			using TcpWorker = EpollWorker;
			using TcpWorkerPtr = std::shared_ptr<TcpWorker>;
			enum
			{
				kReadBufferSize  = 1024 * 1024 * 8,
				kWriteBufferSize = 1024 * 1024 * 8,
				kMaxPackageLimit = 16 * 1024
			};

			TcpConnection()
			{
				need_reconnect = false;
				epoll_events = 0;
				is_passive = true;
				(connection_index++);
				conn_id = connection_index;
				send_buffer.reserve(kWriteBufferSize);
			}

			virtual ~TcpConnection() = default;

			int32_t send(const std::string &data)
			{
				return msend(data);
			}
			int32_t send(const char *data, uint32_t dataLen)
			{
				return msend(std::string_view(data, dataLen));
			}

			void notify_send()
			{
				if (tcp_worker)
				{
					epoll_events |= EPOLLOUT;
					tcp_worker->mod_event(this->conn_sd, this, epoll_events);
				}
			}

			// reconnect  timer 3000000  3s
			inline void enable_reconnect(uint32_t interval = 3000000)
			{
				need_reconnect = true;

				tcp_worker->start_timer([this]() { 
		 
					if (!is_open()){ 
						do_connect(); 
					}
					return true; 
				}, interval, true);
			}

			void set_tcpdelay()
			{
				int yes = 1;
				int result = setsockopt(conn_sd, IPPROTO_TCP, TCP_NODELAY, (char *)&yes, sizeof(int)); // 1 - on, 0 - off
				if (result < 0)
				{
					perror("set tcp nodelay failed");
				}
			}

			template <class P, class... Args>
			int32_t msend(const P &first, Args &&...rest)
			{
				if (is_open())
				{
					bool isWriting = false;
					int32_t sent = 0;
					{
						std::lock_guard<std::mutex> lk(write_mutex);
						isWriting = !send_buffer.empty();
						sent = this->mpush(first, rest...);
					}
					if (!isWriting)
					{
					//	notify_send();
						do_send();	
					}
					
					return sent;
				}
				printf("connection is not open %d , status %d\n", conn_sd, conn_status);
				return -1;
			}

			virtual int32_t handle_package(char *data, uint32_t len)
			{
				return len;
			}

			virtual int32_t handle_data(char *data, uint32_t len)
			{
				return len;
			}

			virtual bool handle_event(NetEvent evt) { return true; }

			bool is_open()
			{
				if (conn_sd > 0)
				{
					// return (conn_status == CONN_OPEN) && (fcntl(conn_sd, F_GETFD) != -1 || errno != EBADF);
					return (conn_status == CONN_OPEN);
				}
				return false;
			}

			void init(int fd, const std::string &host = "", uint16_t port = 0, bool passive = true)
			{
				remote_host = host;
				remote_port = port;
				is_passive = passive;
				epoll_events = EPOLLET | EPOLLIN  | EPOLLERR;
				if (fd > 0)
				{
					this->conn_sd = fd;
				}
				
			}

			void on_ready()
			{
				if (conn_status == ConnStatus::CONN_IDLE)
				{
					conn_status = ConnStatus::CONN_OPEN;
					// this->set_tcpdelay();

					//printf("on connection is ready fd %d\n", conn_sd);
					this->handle_event(NetEvent::EVT_CONNECT);
				}
			}

			uint64_t start_timer(const TimerHandler &handler, uint64_t interval, bool bLoop = true)
			{
				return tcp_worker->start_timer(handler, interval, bLoop);
			}

			bool restart_timer(uint64_t timerId, uint64_t interval = 0)
			{
				return tcp_worker->restart_timer(timerId, interval);
			}
			void stop_timer(uint64_t timerId)
			{
				tcp_worker->stop_timer(timerId);
			}
			bool is_connected()
			{
				if (conn_sd > 0)
				{
					return conn_status == ConnStatus::CONN_OPEN;
				}
				return false;
			}

			void close()
			{								
				if (conn_status < ConnStatus::CONN_CLOSING)
				{
					conn_status = ConnStatus::CONN_CLOSING; 
					notify_send(); 
				}
			}

			void do_close()
			{
				
				if (conn_status == ConnStatus::CONN_CLOSING)
				{ 
					conn_status = ConnStatus::CONN_CLOSED;
					 
					if (tcp_worker)
					{
						tcp_worker->del_event(this->conn_sd);
					}
					this->handle_event(NetEvent::EVT_DISCONNECT);

					if (need_reconnect)
					{
						printf("need reconnect fd %d\n", conn_sd);
						conn_status = ConnStatus::CONN_IDLE;
					}
					else
					{

						if (conn_sd > 0)
						{
							static uint32_t close_count = 0; 
							printf("close socket %d count %d\n", conn_sd, ++close_count); 						
							::close(conn_sd);
							conn_sd = -1;
						}
				
						if (factory)
						{
							auto self = this->shared_from_this(); 
							tcp_worker->post_task([self ](){
								self->factory->remove_connection(self->get_cid());
							}); 						 
						}
					}
				}
			}

			int32_t do_connect()
			{ 
				conn_sd = socket(AF_INET, SOCK_STREAM, 0); 
				//printf("do connect socket fd %d remote host %s, port %d\n", conn_sd, remote_host.c_str(), remote_port);
				struct sockaddr_in servaddr;
				memset(&servaddr, 0, sizeof(sockaddr_in));
				servaddr.sin_family = AF_INET;
				servaddr.sin_port = htons(remote_port);

				if (inet_pton(AF_INET, remote_host.c_str(), &servaddr.sin_addr) <= 0)
				{
					printf("inet_pton error for %s\n", remote_host.c_str());
					return -1;
				}

				int ret = ::connect(conn_sd, (struct sockaddr *)&servaddr, sizeof(servaddr));
				if (ret < 0)
				{
					::close(conn_sd);
					printf("connect to %s:%d failed: %s(errno: %d)\n", remote_host.c_str(), remote_port, strerror(errno), errno);
					return -1;
				}
				set_noblock(conn_sd); 
				printf("connect to %s:%d success fd %d status %d\n", remote_host.c_str(), remote_port, conn_sd, conn_status);
 
				epoll_events |= EPOLLOUT;
				tcp_worker->add_event(conn_sd, this, epoll_events ); 
				return conn_sd;
			}

			void do_send()
			{
				{
					std::lock_guard<std::mutex> lk(write_mutex);
					if (send_buffer.empty())
					{
						// no data to send
						epoll_events &= ~EPOLLOUT;
						tcp_worker->mod_event(this->conn_sd, this, epoll_events);
						return;
					}

					// printf("send to %d data size %lu\n",conn_sd,  send_buffer.size());
					int rc = ::send(conn_sd, send_buffer.data(), send_buffer.size(), 0);
					if (rc < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{
							// send buffer is full
							epoll_events |= EPOLLOUT;
							tcp_worker->mod_event(this->conn_sd, this, epoll_events);
						}
						else
						{
							perror("send failed");
							close();
						}
						return;
					}
					else if (rc > 0 && (uint32_t)rc < send_buffer.size())
					{

						send_buffer.erase(0, rc);
					}
					else
					{

						string_resize(send_buffer, 0);
						return ; 
					}
				}
				// send until all buffer is empty
				do_send();
			}

			int32_t do_read()
			{
				int len = 0;
				uint32_t bufSize = kReadBufferSize - read_buffer_pos;
				if (bufSize > 0)
				{
					int rc = ::recv(conn_sd, &read_buffer[read_buffer_pos], bufSize, 0);
					if (rc < 0)
					{
						if (errno == EAGAIN || errno == EWOULDBLOCK)
						{

						}
						else
						{
							perror("recv failed");
							this->close();
							return -1;
						}
		
						return 0;
					}

					if (rc == 0)
					{						
						this->close();
						return -1;
					}
					len = rc;
				}
				//printf("read data %d\n", len); 
				auto hasMore =  this->process_data(len);
				if (hasMore  ){				
					this->do_read(); 
				}
 
				return len;
			}

			bool process_data(uint32_t nread)
			{
				read_buffer_pos += nread;
				int32_t readPos = 0;
				while (readPos < read_buffer_pos)
				{
					int32_t pkgLen =
						handle_package(&read_buffer[readPos], read_buffer_pos);

					if (pkgLen <= 0 || pkgLen > read_buffer_pos - readPos)
					{
						if (pkgLen > kReadBufferSize)
						{
							this->close();
							return false;
						}
						break;
					}

					handle_data(&read_buffer[readPos], pkgLen);
					readPos += pkgLen;
					// add flow control
				}

				if (readPos < read_buffer_pos)
				{
					memmove(read_buffer, &read_buffer[readPos], read_buffer_pos - readPos);
				}

				read_buffer_pos -= readPos;
				return read_buffer_pos > 0 ;
			}

			virtual void process_event(int32_t evts) override
			{
				 //printf("on tcp connection event %#x status %d sd %d\n", evts,  conn_status ,  conn_sd);
				// if ((EPOLLIN == (evts & EPOLLIN)) ||(EPOLLOUT == (evts & EPOLLOUT)) ) {
				if (conn_status == ConnStatus::CONN_IDLE)
				{
					this->on_ready();
				}
				//}

				if (EPOLLIN == (evts & EPOLLIN))
				{					
					int ret = this->do_read();
					if (ret > 0)
					{
						//  epoll_events |=   EPOLLIN;
						//  tcp_worker->mod_event(this->conn_sd,this, epoll_events );
					}
				}

				if (EPOLLOUT == (evts & EPOLLOUT))
				{ 					
					this->do_send(); 
				}

				if (EPOLLERR == (evts & EPOLLERR))
				{
					printf("EPOLLERROR event %d ", evts);
					this->close();
				}

								//do check closing state 
				if (conn_status == ConnStatus::CONN_CLOSING){ 				
					do_close();
				}
			}

			char read_buffer[kReadBufferSize + 64];
			int32_t read_buffer_pos = 0;

			uint16_t remote_port;
			std::string remote_host;

			ConnStatus conn_status{ConnStatus::CONN_IDLE};
			TcpWorkerPtr tcp_worker;
			inline uint64_t get_cid()
			{
				return conn_id;
			}

			int conn_sd = -1;
			bool need_reconnect = false ;


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

		protected:
			template <typename P>
			inline uint32_t write_data(const P &data)
			{
#if __cplusplus > 201103L
				send_buffer.append(std::string_view((const char *)&data, sizeof(P)));
#else
				send_buffer.append(std::string((const char *)&data, sizeof(P)));
#endif //
				return send_buffer.size();
			}

			inline uint32_t write_data(const std::string_view &data)
			{
#if __cplusplus > 201103L
				send_buffer.append(data);
#else
				send_buffer.append(std::string(data));
#endif //
				return send_buffer.size();
			}

			inline uint32_t write_data(const std::string &data)
			{
				send_buffer.append(data);
				return send_buffer.size();
			}

			inline uint32_t write_data(const char *data)
			{
				if (data != nullptr)
				{
#if __cplusplus > 201103L
					send_buffer.append(std::string_view(data));
#else
					send_buffer.append(std::string(data));
#endif //
					return send_buffer.size();
				}
				return 0;
			}

			template <typename F, typename... Args>
			int32_t mpush(const F &data, Args &&...rest)
			{
				auto len = this->write_data(data);
				return len > 0 ? len + mpush(std::forward<Args &&>(rest)...) : 0;
			}

			int32_t mpush()
			{
				return 0;
			}

			std::mutex write_mutex;
			std::string send_buffer;
		 
 

			int32_t epoll_events ;
			bool is_passive;
			uint64_t conn_id{0}; // connection id
#if __cplusplus > 201103L
			static std::atomic_int64_t connection_index;
#else
			static uint64_t connection_index;
#endif //
			TcpFactory<T> *factory{nullptr};
		};

#if __cplusplus > 201103L
		template <class T>
		std::atomic_int64_t TcpConnection<T>::connection_index{1024};
#else
		template <class T>
		uint64_t TcpConnection<T>::connection_index{1024};
#endif

	}
}
