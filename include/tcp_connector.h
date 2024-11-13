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

#include "net_url.h" 
#include "tcp_connection.h"
#include "epoll_worker.h"

namespace snet
{

  template <class Connection, class Factory = TcpFactory<Connection>>
  class TcpConnector : public Factory, public HeapTimer<>
  {
  public:
    using ConnectionPtr = std::shared_ptr<Connection>;

    using TcpWorker = EpollWorker;
    using TcpWorkerPtr = std::shared_ptr<TcpWorker>;

    TcpConnector(Factory *factory = nullptr, TcpWorkerPtr worker = nullptr)
    {
      connection_factory = factory == nullptr ? this : factory;
      if (worker){
        tcp_worker = worker;
      }else {
        default_worker = std::make_shared<TcpWorker>();
        tcp_worker = default_worker;
      }
      
    }

    bool start()
    {
      signal(SIGPIPE, SIG_IGN);
      if (default_worker ){
        default_worker->start();
      }     
 
      return true;
    }

    void stop()
    {
      is_running = false;
    }

    void process_timeout()
    {

      // for (auto &item : connection_factory->connection_map)
      // {
      //   auto conn = item.second;
      //   if (!conn->is_open())
      //   {
      //     auto fd = conn->do_connect();
      //     tcp_worker->mod_event(conn->conn_sd, conn.get());
      //   }
      // }
    }

    void enable_reconnect(){
      
    }

    ConnectionPtr add_connection(const std::string &url  ){
      NetUrl netUrl ;
      netUrl.parse(url);
 
      return connect(netUrl.host, netUrl.port);
    }

    template <class... Args>
    ConnectionPtr connect(const std::string &host, uint16_t port, Args &&...args)
    {
      ConnectionPtr conn = connection_factory->create(false, std::forward<Args>(args)...); 
      conn->need_reconnect = true; 
      conn->tcp_worker = tcp_worker;
      conn->init(0, host, port, false);
      auto fd = conn->do_connect(); 

      return conn;
    }

    void add_connection(int sd, ConnectionPtr conn) { connection_factory->add_connection(sd, conn); }
    int32_t remove_connection(int sd) { connection_factory->remove_connection(sd); return 0;  }

    ConnectionPtr find_connection(int sd)
    {
      return connection_factory->find_connection(sd);
    }

    bool is_running = false;
    TcpWorkerPtr tcp_worker;
    TcpWorkerPtr default_worker; 
    Factory *connection_factory = nullptr;
  };

} // namespace snet
