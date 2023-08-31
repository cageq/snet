#pragma once

#include "loop_buffer.h"
#include <errno.h>
#include <memory>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
enum ConnectionEvent{
    CONNECTION_INIT, 
    CONNECTION_OPEN, 
    CONNECTION_CLOSE, 

}; 


template <class T>
class TcpConnection : public std::enable_shared_from_this<T> {

public:
  enum {
    kReadBufferSize = 1024 * 1024 * 8,
    kWriteBufferSize = 1024 * 1024 * 8,
    kMaxPackageLimit = 16 * 1024
  };
  virtual ~TcpConnection() = default;
  template <typename... Args> int32_t msend(Args &&...args) {
    if (is_open()) {
      auto ret = send_buffer.mpush(std::forward<Args>(args)...);
      send_cond.notify_one();
      return ret;
    }
    return -1;
  }

  virtual int32_t demarcate_message(const char *data, uint32_t len) {
    return len;
  }

  virtual int32_t handle_data(const char *data, uint32_t len) { return len; }

  virtual void handle_event(uint32_t evt) {

  }

  void init(int fd) {
    this->conn_sd = fd;

     is_closed = false;
    //recv_thread = std::thread([this]() { this->do_read(); });
    send_thread = std::thread([this]() { this->do_write(); });
    this->handle_event(CONNECTION_OPEN); 
  }

    bool is_open() { 
        return   !is_closed&& ( fcntl(conn_sd, F_GETFD) != -1 || errno != EBADF) ;
    }

  void do_write() {
    do {
      wait(1); 
      auto [data, dataLen] = send_buffer.read();

      if (dataLen > 0 && conn_sd > 0 ) {
        int rc = ::send(conn_sd, data, dataLen, 0);
        if (rc < 0) {
          perror("send() failed");
          is_closed = true;
          break;
        }
        send_buffer.commit(rc);
      }

    } while (!is_closed);
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
            int rc = recv(conn_sd, &read_buffer[read_buffer_pos], bufSize, 0);
            if (rc < 0) {
                if (errno != EWOULDBLOCK) {
                    perror("  recv() failed");
                    is_closed = true;
                }
                break;
            } 



            /**********************************************/
            /* Check to see if the connection has been    */
            /* closed by the client                       */
            /**********************************************/
            if (rc == 0) {
                printf("  Connection closed\n");
                is_closed = true;
                return -1;         
            }

            /**********************************************/
            /* Data was received                          */
            /**********************************************/
            len = rc;
            printf("  %d bytes received\n", len);
            this->process_data(len);
        }

    } while (0);
    return len ; 
  }

  void close() {

    if (!is_closed ){
        is_closed = true; 
        if (conn_sd > 0){
            ::close(conn_sd); 
        }        
    }
    /*************************************************/
    /* If the is_closed flag was turned on, we need */
    /* to clean up this active connection.  This     */
    /* clean up process includes removing the        */
    /* descriptor from the master set and            */
    /* determining the new maximum descriptor value  */
    /* based on the bits that are still turned on in */
    /* the master set.                               */
    /*************************************************/
    // if (is_closed) {
    //   ::close(i);
    //   FD_CLR(i, &master_set);
    //   if (i == max_sd) {
    //     while (FD_ISSET(max_sd, &master_set) == FALSE)
    //       max_sd -= 1;
    //   }
    // }
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

  }
  char read_buffer[kReadBufferSize];
  int32_t read_buffer_pos = 0;

  std::thread send_thread;
  std::thread recv_thread;

  LoopBuffer<> send_buffer;
  std::mutex send_mutex;
  std::condition_variable send_cond;
  int conn_sd = -1 ;
  bool is_closed = false;
};
