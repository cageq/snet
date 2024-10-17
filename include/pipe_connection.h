#pragma once
#include "pipe_factory.h"
#include "tcp_connection.h"

namespace snet
{
  enum PipeMsgType
  {
    PIPE_MSG_SHAKE_HAND = 1,
    PIPE_MSG_HEART_BEAT,
    PIPE_MSG_DATA,
    PIPE_MSG_ACK,
  };

  struct PipeMsgHead
  {
    uint32_t length;
    uint32_t type;
    uint64_t data; // user data
    char body[0];
  };

  template <class UserSession>
  class PipeConnection : public TcpConnection<PipeConnection<UserSession>>
  {
  public:
    using Parent = TcpConnection<PipeConnection<UserSession>>;
    using UserSessionPtr = std::shared_ptr<UserSession>;

    PipeConnection(UserSessionPtr session = nullptr, PipeFactory<UserSession> *factroy = nullptr) : user_session(session), pipe_factory(factroy) {}

    virtual int32_t handle_package(char *data, uint32_t len)
    {
      if (len < sizeof(PipeMsgHead))
      {
        return 0;
      }

      PipeMsgHead *msg = (PipeMsgHead *)data;
      if (msg->length + sizeof(PipeMsgHead) > len)
      {
        return 0;
      }
      return sizeof(PipeMsgHead) + msg->length;
    }

    virtual bool handle_event(NetEvent evt) override
    {
      printf("handle pipe connection %d\n", evt);
      if (evt == NetEvent::EVT_CONNECT)
      {

        if (!Parent::is_passive)
        {
          this->send_shakehand(this->user_session->pipe_id);
        }
        else
        {
        }
      }
      return true; 
    }

    int32_t send_heartbeat(const std::string &msg = "")
    {
      PipeMsgHead head{static_cast<uint32_t>(msg.length()), PIPE_MSG_HEART_BEAT,
                       0};
      return Parent::msend(head, msg);
    }

    void send_shakehand(const std::string &pipeId)
    {
      PipeMsgHead shakeMsg{static_cast<uint32_t>(pipeId.length()), PIPE_MSG_SHAKE_HAND, 0};
      this->msend(shakeMsg, pipeId);
    }

    virtual int32_t handle_data(char *data, uint32_t len) override
    {
      printf("handle data length %d\n", len);
      PipeMsgHead *msg = (PipeMsgHead *)data;
      if (msg->type == PIPE_MSG_SHAKE_HAND)
      {
        if (Parent::is_passive)
        { // server side
          process_server_handshake(msg);
        }
        else
        {
          process_client_handshake(msg);
        }
        return len;
      }
      else if (msg->type == PIPE_MSG_HEART_BEAT)
      {
        if (Parent::is_passive)
        {
          this->send_heartbeat();
        }
        return len;
      }

      user_session->handle_message(msg->body, msg->length, msg->data);
      return len;
    }

    void process_client_handshake(PipeMsgHead *msg)
    {
      if (msg->length > 0)
      {
        std::string pipeId = std::string((const char *)msg->body, msg->length);
        if (user_session)
        {
          user_session->on_ready();
        }
      }
    }

    void process_server_handshake(PipeMsgHead *msg);
    UserSessionPtr user_session;
    PipeFactory<UserSession> *pipe_factory = nullptr;
  };

  template <class UserSession>
  void PipeConnection<UserSession>::process_server_handshake(PipeMsgHead *msg)
  {
    if (msg->length > 0)
    {
      std::string pipeId = std::string((const char *)msg->body, msg->length);

      auto session = pipe_factory->find_user_session(pipeId);
      if (session)
      {
        session->on_ready();
        session->handle_event(1);
      }
      else
      {
        printf("not found pipe id %s\n", pipeId.c_str());
      }
      this->send_shakehand(pipeId);
    }
  }

}