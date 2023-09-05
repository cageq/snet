#pragma once

#include "pipe_connection.h"
#include "tcp_connection.h"
template <class UserSession>
class PipeFactory : public TcpFactory<PipeConnection<UserSession>> {
public:
  using UserSessionPtr = std::shared_ptr<UserSession>;
  using PipeTcpConnection = PipeConnection<UserSession>;

  using PipeConnectionPtr = std::shared_ptr<PipeConnection<UserSession>>;

  virtual void on_create(PipeConnectionPtr conn) {}

  virtual void on_release(PipeConnectionPtr conn) {}

  template <class... Args>
  UserSessionPtr add_remote_pipe(const std::string &pipeId,
                                 const std::string &host, uint16_t port,
                                 Args &&...args) {

    pipe_map[pipeId] =
        std::make_shared<UserSession>(std::forward<Args>(args)...);
  }

  template <class... Args>
  UserSessionPtr add_local_pipe(const std::string &pipeId, Args &&...args) {

    pipe_map[pipeId] =
        std::make_shared<UserSession>(std::forward<Args>(args)...);
  }

  UserSessionPtr find_user_session(const std::string &pipeId) {
    auto itr = pipe_map.find(pipeId);
    if (itr != pipe_map.end()) {
      return itr->second;
    }
    return nullptr;
  }

  std::unordered_map<std::string, UserSessionPtr> pipe_map;
};