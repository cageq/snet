#pragma once

#include "loop_buffer.h"
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

#include "epoll_worker.h"
#include "tcp_factory.h"

inline void string_resize(std::string &str, std::size_t sz)
{
	str.resize(sz);
}

enum ConnStatus
{
	CONN_IDLE,
	CONN_OPEN,
	CONN_CLOSING,
	CONN_CLOSED,
};

enum ConnEvent
{
	CONN_EVENT_INIT,
	CONN_EVENT_OPEN,
	CONN_EVENT_SEND,
	CONN_EVENT_RECV,
	CONN_EVENT_CLOSE
};

using TimerHandler = std::function<bool()>;

template <class T>
class TcpConnection : public std::enable_shared_from_this<T>
{

public:
	using TcpWorker = EpollWorker<T>;
	using TcpWorkerPtr = std::shared_ptr<TcpWorker>;

	TcpConnection() = default;

	enum
	{
		kReadBufferSize = 1024 * 1024 * 8,
		kWriteBufferSize = 1024 * 1024 * 8,
		kMaxPackageLimit = 16 * 1024
	};

	virtual ~TcpConnection()
	{
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
			//tcp_worker->mod_event(static_cast<T *>(this), epoll_events);
		}
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
			int32_t sent = 0;
			{
				std::lock_guard<std::mutex> lk(write_mutex);
				sent = this->mpush(first, rest...);
			}
			notify_send(); 		
			return sent;
		}
		return -1;
	}
	 

	virtual int32_t demarcate_message(char *data, uint32_t len)
	{
		return len;
	}

	virtual int32_t handle_data(char *data, uint32_t len) { return len; }

	virtual void handle_event(uint32_t evt) {}

	bool is_open()
	{
		if (conn_sd > 0)
		{
			return (status == CONN_OPEN) && (fcntl(conn_sd, F_GETFD) != -1 || errno != EBADF);
		}
		return false;
	}

	void close()
	{
		this->do_close();
	}

	int32_t get_id()
	{
		return conn_sd;
	}

	void init(int fd, const std::string &host = "", uint16_t port = 0, bool passive = true)
	{
		epoll_events = EPOLLET| EPOLLIN| EPOLLERR; 
		this->conn_sd = fd;
		is_passive = passive;
		remote_host = host;
		remote_port = port;
	}

	void on_ready()
	{
		this->set_tcpdelay();
		this->handle_event(CONN_EVENT_OPEN);
	}

	int32_t do_connect()
	{
		// conn_sd = socket(AF_INET, SOCK_STREAM, 0);
		struct sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(sockaddr_in));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(remote_port);

		if (inet_pton(AF_INET, remote_host.c_str(), &servaddr.sin_addr) <= 0)
		{
			printf("inet_pton error for %s\n", remote_host.c_str());
			return -1;
		}

		if (::connect(conn_sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		{
			printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
			return -1;
		}
		this->on_ready();
		return conn_sd;
	}

	bool  is_writting {false}; 
	void do_send()
	{
		if (is_open())
		{
			{
				std::lock_guard<std::mutex> lk(write_mutex); 
				if(!send_buffer.empty()){
					if (cache_buffer.empty() ){
						send_buffer.swap(cache_buffer);   
					}
				}
			}
 
			if (!cache_buffer.empty()   )
			{			 
				printf("real send %lu\n", cache_buffer.size());

				int rc = ::send(conn_sd, cache_buffer.data(), cache_buffer.size(), 0);
				if (rc < 0)
				{
					perror("send() failed");
					do_close();
					return;
				}else if (rc > 0  && (uint32_t) rc < cache_buffer.size()){
					cache_buffer.erase(0, rc); 
					do_send(); 
				}else {
					string_resize(cache_buffer, 0); 
				}							 
			}
		}
	}

	int32_t do_read()
	{
		printf("start to read data\n");
		int len = 0;
		do
		{

			if (conn_sd > 0)
			{
				uint32_t bufSize = kReadBufferSize - read_buffer_pos;
				if (bufSize > 0)
				{
					int rc = recv(conn_sd, &read_buffer[read_buffer_pos], bufSize, 0);
					if (rc < 0)
					{
						if (errno != EWOULDBLOCK)
						{
							perror("  recv() failed");
							return -1;
						}
						break;
					}

					if (rc == 0)
					{
						printf("connection closed\n");
						return -1;
					}

					len = rc;
				}
				// printf("  %d bytes received\n", len);
				this->process_data(len);
			}

		} while (0);
		return len;
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

	bool process_data(uint32_t nread)
	{
		read_buffer_pos += nread;
		int32_t readPos = 0;
		while (readPos < read_buffer_pos)
		{
			int32_t pkgLen =
				demarcate_message(&read_buffer[readPos], read_buffer_pos);

			if (pkgLen <= 0 || pkgLen > read_buffer_pos - readPos)
			{
				if (pkgLen > kReadBufferSize)
				{
					this->do_close();
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
		return true;
	}

	void do_close()
	{
		if (status < ConnStatus::CONN_CLOSING)
		{
			status = ConnStatus::CONN_CLOSING;

			this->handle_event(CONN_EVENT_CLOSE);

			if (tcp_worker)
			{
				tcp_worker->del_event(static_cast<T *>(this));
			}
			if (conn_sd > 0)
			{
				::close(conn_sd);
				conn_sd = -1;
			}
			status =  ConnStatus::CONN_CLOSED; 
		}
	}

	void process_event(int32_t evts)
	{

		if (status == ConnStatus::CONN_IDLE)
		{
			status = ConnStatus::CONN_OPEN;			
			this->on_ready();
		}

		if (EPOLLIN == (evts & EPOLLIN))
		{
			int ret = this->do_read();
			if (ret > 0)
			{
				epoll_events |=   EPOLLIN; 
				//tcp_worker->mod_event(static_cast<T *>(this), epoll_events );
			}
		}

		if (EPOLLOUT == (evts & EPOLLOUT))
		{
			this->do_send();
		}

		if (EPOLLERR == (evts & EPOLLERR))
		{
			printf("EPOLLERROR event %d ", evts);

			this->do_close();
		}
	}

	char read_buffer[kReadBufferSize];
	int32_t read_buffer_pos = 0;



	int conn_sd = -1;

	uint16_t remote_port;
	std::string remote_host;

	ConnStatus status = ConnStatus::CONN_IDLE;
	TcpWorkerPtr tcp_worker;

protected:
	template <typename P>
	inline uint32_t write_data(const P &data)
	{
		send_buffer.append(std::string_view((const char *)&data, sizeof(P)));
		return send_buffer.size();
	}

	inline uint32_t write_data(const std::string_view &data)
	{
		send_buffer.append(data);
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
			send_buffer.append(std::string_view(data));
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
	std::string cache_buffer;
	 
	int32_t epoll_events = 0 ; 
	bool is_passive = true;
};
