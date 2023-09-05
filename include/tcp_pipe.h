#pragma once

#include "tcp_client.h"
#include "tcp_connection.h"
#include "tcp_server.h"

#include "pipe_connection.h"
#include "pipe_factory.h"
#include "pipe_session.h"
#include <memory>
#include <unistd.h>
#include <unordered_map>

template <class UserSession> class TcpPipe : public PipeFactory<UserSession> {

public:
  using PipeTcpConnection = PipeConnection<UserSession>;

  using UserSessionPtr = std::shared_ptr<UserSession>;

  TcpPipe() : tcp_client(this), tcp_server(this) {}

  void start(const std::string &host = "0.0.0.0", uint16_t port = 9999) {

    if ((pipe_mode & PIPE_SERVER_MODE) && port > 0) {
      tcp_server.start(port);
    }

    if (pipe_mode & PIPE_CLIENT_MODE) {
      tcp_client.start();
    }
  }

  void attach(UserSessionPtr pipe, const std::string &host = "",
              uint16_t port = 0) {
    if (pipe) {
    }
  }

private:
  TcpClient<PipeTcpConnection> tcp_client;
  TcpServer<PipeTcpConnection> tcp_server; 

  PipeFactory<UserSession>  pipe_factory; 

  PipeMode pipe_mode;
};