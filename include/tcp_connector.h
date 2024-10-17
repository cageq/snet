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
#include "epoll_worker.h"

namespace snet
{

  template <class Connection, class Factory = TcpFactory<Connection>>
  class TcpConnector : public HeapTimer<>
  {
  public:
    using ConnectionPtr = std::shared_ptr<Connection>;

    using TcpWorker = EpollWorker ;
    using TcpWorkerPtr = std::shared_ptr<TcpWorker>;

    TcpConnector(Factory *factory = nullptr, TcpWorkerPtr worker = nullptr ) : connection_factory(factory)
    {     
        tcp_worker = worker == nullptr?  std::make_shared<TcpWorker>() : worker;      
    }

    bool start()
    {
      signal(SIGPIPE, SIG_IGN);
      tcp_worker->start();
      return true;
    }

    void stop()
    {
      is_running = false;
    }

    void process_timeout()
    {

      for (auto &item : connection_map)
      {
        auto conn = item.second;
        if (!conn->is_open())
        {
          auto fd = conn->do_connect();
          tcp_worker->mod_event(conn->conn_sd);
        }
      }
    }

    template <class... Args>
    ConnectionPtr connect(const std::string &host, uint16_t port, Args &&...args)
    {
      ConnectionPtr conn;
      if (connection_factory != nullptr)
      {
        conn = connection_factory->create(std::forward<Args>(args)...);
      }
      else
      {
        conn = std::make_shared<Connection>(std::forward<Args>(args)...);
      }

      int sockfd = socket(AF_INET, SOCK_STREAM, 0);
      conn->tcp_worker = tcp_worker;
      conn->init(sockfd, host, port, false);
      auto fd = conn->do_connect();
      tcp_worker->add_event(sockfd, conn.get());
      connection_map[fd] = conn;
      return conn;
    }

    void add_connection(int sd, ConnectionPtr conn) { connection_map[sd] = conn; }
    int32_t remove_connection(int sd) { return connection_map.erase(sd); }

    ConnectionPtr find_connection(int sd)
    {
      auto itr = connection_map.find(sd);
      if (itr != connection_map.end())
      {
        return itr->second;
      }
      return nullptr;
    }

    const std::unordered_map<uint32_t, ConnectionPtr> &get_connections()
    {
      return connection_map;
    }

    std::unordered_map<uint32_t, ConnectionPtr> connection_map;

    bool is_running = false;
    TcpWorkerPtr tcp_worker;
    Factory *connection_factory = nullptr;
  };

} // namespace snet