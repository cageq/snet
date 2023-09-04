#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <signal.h>

#include "tcp_connection.h"
template <class Connection, class Factory = TcpFactory<Connection> > 
class TcpClient {
public:
  using ConnectionPtr = std::shared_ptr<Connection>;

  TcpClient(Factory * factory = nullptr) :connection_factory(factory){
    FD_ZERO(&master_set);
    FD_ZERO(&working_set); 
  }

  bool start() {
    signal(SIGPIPE, SIG_IGN);
    work_thread = std::thread([this]() { this->run(); });
    return true; 
  }

  void stop(){
    is_running = false; 
    if (work_thread.joinable()){
        work_thread.join(); 
    }
  }
  void run() {
    int desc_ready = 0;
    struct timeval timeout;
    is_running = true; 

    do { 
        /*************************************************************/
        /* Initialize the timeval struct to 3 minutes.  If no        */
        /* activity after 3 minutes this program will end.           */
        /*************************************************************/
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        memcpy(&working_set, &master_set, sizeof(master_set));
        int rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);
        if (rc < 0) {
            perror("  select() failed");
            break;
        }
        // timeout
        if (rc == 0) {
            printf("wait timeout\n"); 
            this->process_timeout(); 
        }

        if (rc > 0 ){ 
            printf("process event %d\n", rc); 
            desc_ready = rc;
            for (int sd = 0; sd <= max_sd && desc_ready > 0; ++sd) {
                if (FD_ISSET(sd, &working_set)) {
                    desc_ready -= 1;
                    auto conn = find_connection(sd);
                    if (conn) {
                        auto ret = conn->do_read();
                        if (ret < 0) {
                            conn->do_close(); 
                            FD_CLR(sd, &master_set);
                            //remove_connection(sd);
                            //   this->release(conn);
                        }
                    } else {
                        FD_CLR(sd, &master_set);
                    }
                }
            }
        
        }
    } while (is_running);
  }

  void add_fd(int fd) { FD_SET(fd, &master_set); }
  void del_fd(int fd) { FD_CLR(fd, &master_set); }

  void process_timeout() {

    for (auto &item : connection_map) {
      auto conn = item.second;
      if (!conn->is_open()) {
         auto fd =  conn->do_connect();
        if (fd > 0){
         FD_SET(fd, &master_set); 
         }
      }
    }
  }

  ConnectionPtr connect(const std::string &host, uint16_t port) {
    ConnectionPtr conn ; 
    if (connection_factory != nullptr){
      conn = connection_factory->create();  
    }else {
      conn = std::make_shared<Connection>();  
    }
    
    int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
    conn->init(sockfd, host, port, false );
    auto fd = conn->do_connect();
    if (fd > 0){
        FD_SET(fd, &master_set); 
    }
    if (fd > max_sd) {
      max_sd = fd;
    } 
    connection_map[fd] = conn;
    return conn;
  }

  void add_connection(int sd, ConnectionPtr conn) { connection_map[sd] = conn; }
  int32_t remove_connection(int sd) { return connection_map.erase(sd); }

  ConnectionPtr find_connection(int sd) { 
    auto itr = connection_map.find(sd);
    if (itr != connection_map.end()) {
      return itr->second;
    } 
    return nullptr;
  }
  const std::unordered_map<uint32_t, ConnectionPtr>  & get_connections(){
    return connection_map; 
  }

  std::unordered_map<uint32_t, ConnectionPtr> connection_map;
  fd_set master_set, working_set;
  int max_sd = 0;
  bool is_running = false;
  std::thread work_thread;

  Factory * connection_factory = nullptr; ; 
};