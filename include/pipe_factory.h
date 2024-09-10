#pragma once

 
#include "tcp_connection.h"
//#include "pipe_connection.h"


template <class UserSession>
class PipeConnection; 

template <class UserSession>
class PipeFactory : public TcpFactory<PipeConnection<UserSession>> {
public:
  using UserSessionPtr = std::shared_ptr<UserSession>;
  using PipeTcpConnection = PipeConnection<UserSession>;

  using PipeConnectionPtr = std::shared_ptr<PipeConnection<UserSession>>;

  virtual void on_create(uint64_t cid, PipeConnectionPtr conn) override {
    conn->pipe_factory = this; 
  }

  virtual void on_release(uint64_t cid, PipeConnectionPtr conn) override {

  }

  void add_session(const std::string & pipeId, UserSessionPtr session){
    pipe_map[pipeId] = session; 
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