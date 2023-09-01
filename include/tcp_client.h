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
template <class Connection> class TcpClient {
public:
  using ConnectionPtr = std::shared_ptr<Connection>;

  TcpClient() {}

  void start() {
    work_thread = std::thread([this]() { this->run(); });
  }
  void run() {
    int desc_ready = 0;
    struct timeval timeout;
    is_running = true;

    FD_ZERO(&master_set);

    do {

      FD_ZERO(&master_set);
      /*************************************************************/
      /* Initialize the timeval struct to 3 minutes.  If no        */
      /* activity after 3 minutes this program will end.           */
      /*************************************************************/
      timeout.tv_sec = 3;
      timeout.tv_usec = 0;
      memcpy(&working_set, &master_set, sizeof(master_set));
      int rc = select(max_sd + 1, &working_set, NULL, NULL, &timeout);

      if (rc < 0) {
        perror("  select() failed");
        break;
      }
      // timeout
      if (rc == 0) {
        this->process_timeout();
        continue;
      }

      desc_ready = rc;
      for (int sd = 0; sd <= max_sd && desc_ready > 0; ++sd) {
        if (FD_ISSET(sd, &working_set)) {
          desc_ready -= 1;
          auto conn = find_connection(sd);
          if (conn) {
            auto ret = conn->do_read();
            if (ret < 0) {
              FD_CLR(sd, &master_set);
              remove_connection(sd);
              //   this->release(conn);
            }
          } else {
            FD_CLR(sd, &master_set);
          }
        }
      }

    } while (true);
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
    auto conn = std::make_shared<Connection>();

    // struct sockaddr_in servaddr;
    // memset(&servaddr, 0, sizeof(sockaddr_in));
    // servaddr.sin_family = AF_INET;
    // servaddr.sin_port = htons(port);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
 
    conn->init(sockfd, host, port, false );
    auto fd = conn->do_connect();
    if (fd > 0){
        FD_SET(sockfd, &master_set); 
    }
       if (sockfd > max_sd) {
      max_sd = sockfd;
    }

    connection_map[sockfd] = conn;
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

  std::unordered_map<uint32_t, ConnectionPtr> connection_map;
  fd_set master_set, working_set;
  int max_sd = 0;
  bool is_running = false;
  std::thread work_thread;
};