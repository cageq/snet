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
#include "heap_timer.h"
enum ConnectionEvent{
	CONNECTION_INIT, 
	CONNECTION_OPEN, 
	CONNECTION_CLOSE, 
}; 
using TimerHandler = std::function<bool()>;

template <class Connection> 
    class  TcpFactory{
    	public: 
			using ConnectionPtr = std::shared_ptr<Connection>; 
		

			template <class ... Args> 
			ConnectionPtr create(Args ... args ) {
				auto conn =  std::make_shared<Connection> (std::forward<Args>(args)...);			
				this->on_create(conn);
				return conn;
			} 
		


			virtual void release(ConnectionPtr conn ){ 
				on_release(conn); 
			}
 
			virtual void on_create(ConnectionPtr conn){}

			virtual void on_release(ConnectionPtr conn){}

			std::function<ConnectionPtr()> creator; 
			std::function<void(ConnectionPtr)> releaser;  
    }; 


//template <class T, class Factory>
//	class TcpServer; 
//template <class T, class Factory >
//	class TcpClient; 
 

template <class T >
class TcpConnection : public std::enable_shared_from_this<T> {

	public:
		// friend class TcpServer<T, Factory > ; 		 
		// friend class TcpClient<T, Factory> ; 
		enum {
			kReadBufferSize = 1024 * 1024 * 4,
			kWriteBufferSize = 1024 * 1024 * 8,
			kMaxPackageLimit = 16 * 1024
		};
		virtual ~TcpConnection() {
			
		}

		int32_t send(const char * data, uint32_t dataLen){
			if(is_open()){
				int ret = send_buffer.push(data, dataLen); 
				send_cond.notify_one(); 
				return dataLen; 
			}
			return -1; 
		}	
		void set_tcpdelay(){ 
			int yes = 1;
			int result = setsockopt(conn_sd, IPPROTO_TCP, TCP_NODELAY, (char *) &yes,  sizeof(int));    // 1 - on, 0 - off
			if (result < 0){ 
				perror("set tcp nodelay failed"); 
			}
		}
		template <typename... Args> 
			int32_t msend(Args &&...args) {
				if (is_open()) {
					auto ret = send_buffer.mpush(std::forward<Args>(args)...);
					send_cond.notify_one();
					return ret;
				}
			return -1;
		}

		virtual int32_t demarcate_message(char *data, uint32_t len) {
			return len;
		}

		virtual int32_t handle_data(char *data, uint32_t len) { return len; }

		virtual void handle_event(uint32_t evt) { }
		bool is_open() { 
			if (conn_sd > 0){
				return   !is_closed&& ( fcntl(conn_sd, F_GETFD) != -1 || errno != EBADF) ;
			}
			return false; 
		}
		void close() {

			if (!is_closed ){
				is_closed = true; 
				if (conn_sd > 0){
					::close(conn_sd); 
				}        
			}
		}
        int32_t get_id(){
            return conn_sd; 
        }



		void init(int fd, const std::string &host = "", uint16_t port = 0 , bool passive = true ) {
			this->conn_sd = fd; 
			is_passive = passive; 
			remote_host = host; 
			remote_port = port; 
		}
		void on_ready(){
			is_closed = false;
			this->set_tcpdelay();
			//read_thread = std::thread([this]() { this->do_read(); });
			write_thread = std::thread([this]() { this->do_write(); });
			write_thread.detach(); 
			this->handle_event(CONNECTION_OPEN); 
		}
		
		int32_t do_connect(){
			conn_sd =  socket(AF_INET, SOCK_STREAM, 0); 
			struct sockaddr_in servaddr;
			memset(&servaddr, 0, sizeof(sockaddr_in));		
			servaddr.sin_family = AF_INET;
			servaddr.sin_port = htons(remote_port); 
		 
			if (inet_pton(AF_INET, remote_host.c_str(), &servaddr.sin_addr) <= 0) {
				printf("inet_pton error for %s\n", remote_host.c_str());
				return -1;
			}

			if (::connect(conn_sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
				printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
				return -1;
			}
			this->on_ready();
			return conn_sd; 
		}

		void do_write() {
			do {
                if (send_buffer.empty()){
                    wait(1); 
                }else {
                    auto [data, dataLen] = send_buffer.read();

                    if (dataLen > 0 && conn_sd > 0 ) {
                        int rc = ::send(conn_sd, data, dataLen, 0);
                        if (rc < 0) {
                            perror("send() failed");
							do_close(); 
                            break;
                        }
                        send_buffer.commit(rc);
                    }
                }			

			} while (is_open());
		}

		int32_t  do_read() {
		 
			int len = 0 ;
			/*************************************************/
			/* Receive all incoming data on this socket      */
			/* before we loop back and call select again.    */
			/*************************************************/
			do {
				/**********************************************/
				/* Receive data on this connection until the  */
				/* recv fails with EWOULDBLOCK.  If any other */
				/* failure occurs, we will close the          */
				/* connection.                                */
				/**********************************************/

				if (conn_sd > 0){
					uint32_t bufSize = kReadBufferSize - read_buffer_pos;
					if (bufSize> 0){
						int rc = recv(conn_sd, &read_buffer[read_buffer_pos], bufSize, 0);
						if (rc < 0) {
							if (errno != EWOULDBLOCK) {
								perror("  recv() failed");								
								return -1; 
							}
							break;
						} 

						/**********************************************/
						/* Check to see if the connection has been    */
						/* closed by the client                       */
						/**********************************************/
						if (rc == 0) {
							printf("  Connection closed\n");							
							return -1;         
						}

						/**********************************************/
						/* Data was received                          */
						/**********************************************/
						len = rc;
					}
					//printf("  %d bytes received\n", len);
					this->process_data(len);
				}

			} while (0);
			return len ; 
		}

		uint64_t start_timer(const  TimerHandler &handler, uint64_t interval, bool bLoop = true)
		{
			return heap_timer->start_timer(handler, interval, bLoop); 			
		}
		bool restart_timer(uint64_t timerId , uint64_t interval = 0 ){
			return heap_timer->restart_timer(timerId, interval); 
		}
		void stop_timer(uint64_t timerId){
			heap_timer->stop_timer(timerId); 
		}

		bool process_data(uint32_t nread) {
			read_buffer_pos += nread;
			int32_t readPos = 0;
			while (readPos < read_buffer_pos) {
				int32_t pkgLen =
					demarcate_message(&read_buffer[readPos], read_buffer_pos);

				if (pkgLen <= 0 || pkgLen > read_buffer_pos - readPos) {
					if (pkgLen > kReadBufferSize) {
						this->do_close();
						return false;
					}
					break;
				}

				handle_data(&read_buffer[readPos], pkgLen);
				readPos += pkgLen;

				// add flow control
			}

			if (readPos < read_buffer_pos) {
				memmove(read_buffer, &read_buffer[readPos], read_buffer_pos - readPos);
			}

			read_buffer_pos -= readPos;
			return true;
		}

		void wait(int msTimeOut) {

			std::unique_lock<std::mutex> lk(send_mutex);
			send_cond.wait_for(lk, std::chrono::microseconds(msTimeOut), [this] {
					return !send_buffer.empty();
					});
		}

 

		void do_close() {
			if (!is_closed){
				::close(conn_sd); 
				is_closed = true; 
			}			
		}
		char read_buffer[kReadBufferSize];
		int32_t read_buffer_pos = 0;

		std::thread write_thread;
		std::thread read_thread;

		LoopBuffer<> send_buffer;
		std::mutex send_mutex;
		std::condition_variable send_cond;
		int conn_sd = -1 ;
		bool is_closed = false;
		uint16_t remote_port; 
		std::string remote_host; 
		HeapTimer<> * heap_timer = nullptr ; 
		protected:
		bool is_passive = true ; 		 

	
};

