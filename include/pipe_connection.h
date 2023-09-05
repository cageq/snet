#pragma once
#include "pipe_factory.h"
#include "tcp_connection.h"
#incoude "pipe_factory.h"
enum PipeMsgType {
  PIPE_MSG_SHAKE_HAND = 1,
  PIPE_MSG_HEART_BEAT,
  PIPE_MSG_DATA,
  PIPE_MSG_ACK,
};

struct PipeMsgHead {
  uint32_t length;
  uint32_t type;
  uint64_t data; // user data
  char body[0];
};

template <class UserSession>
class PipeConnection : public TcpConnection<PipeConnection<UserSession>> {
public:
  using Parent = TcpConnection<PipeConnection<UserSession>>;
  using UserSessionPtr = std::shared_ptr<UserSession>;

  PipeConnection(PipeFactory<UserSession> *factroy) : pipe_factory(factroy) {}

  virtual int32_t demarcate_message(char *data, uint32_t len) {
    if (len < sizeof(PipeMsgHead)) {
      return 0;
    }

    PipeMsgHead *msg = (PipeMsgHead *)data;
    if (msg->length + sizeof(PipeMsgHead) > len) {
      return 0;
    }
    return sizeof(PipeMsgHead) + msg->length;
  }

  virtual void handle_event(uint32_t evt) {
    if (evt == CONNECTION_OPEN) {
      //
      if (!Parent::is_passive) {
        // send heartbeat
        this->send_heartbeat();
      }
    }
  }

  int32_t send_heartbeat(const std::string &msg = "") {
    PipeMsgHead head{static_cast<uint32_t>(msg.length()), PIPE_MSG_HEART_BEAT,
                     0};
    return Parent::msend(head, msg);
  }

    void send_shakehand(const std::string &pipeId)
    {            
        PipeMsgHead shakeMsg{ static_cast<uint32_t>(pipeId.length()), PIPE_MSG_SHAKE_HAND,0};
        this->msend(shakeMsg, pipeId);
    }



  virtual int32_t handle_data(char *data, uint32_t len) {

    PipeMsgHead *msg = (PipeMsgHead *)data;

    if (msg->type == PIPE_MSG_SHAKE_HAND) {

      if (Parent::is_passive()) { // server side
        process_server_handshake(msg);
      } else {
        process_client_handshake(msg);
      }
      return true;
    } else if (msg->type == PIPE_MSG_HEART_BEAT) {

     
      return true;
    }

    return len;
  }

  void process_server_handshake(PipeMsgHead *msg) {
    if (msg->length > 0) {
      std::string pipeId =
          std::string((const char *)msg + sizeof(PipeMsgHead), msg->length);

          auto session = pipe_factory->find_user_session(pipeId); 
          if (session){
            
          }

    //   auto pipe = find_bind_pipe(pipeId);
    //   if (pipe) {
    //     if (!pipe->is_ready()) {
    //       pipe->bind(conn);
    //       ilog("bind pipe {} success", pipeId);
    //       PipeMsgHead shakeMsg{static_cast<uint32_t>(pipeId.length()),
    //                            PIPE_MSG_SHAKE_HAND, 0};
    //       conn->msend(shakeMsg, pipeId);
    //       pipe->handle_event(NetEvent::EVT_CONNECT);
    //       pipe->on_ready();
    //     }
    //   } else {
    //     wlog("pipe id not found {}, close connection", pipeId);
    //     conn->close();
    //   }
    } else {
      // create a pipeid for client
      auto pipeId = this->generate_id();
      auto session = find_unbind_pipe(conn->get_cid());

      if (session) {
        session->update_pipeid(pipeId);
      } else {
        // dlog("create and bind session success {}", pipeId);
        session = std::make_shared<PipeSession>();
        session->bind(conn);
        add_unbind_pipe(conn->get_cid(), session);
      }

      if (session) {
        PipeMsgHead shakeMsg{static_cast<uint32_t>(pipeId.length()),
                             PIPE_MSG_SHAKE_HAND, 0};
        conn->msend(shakeMsg, pipeId);
        session->handle_event(NetEvent::EVT_CONNECT);
      }
    }
  }
  PipeFactory<UserSession> *pipe_factory;
  UserSessionPtr user_session;
};
