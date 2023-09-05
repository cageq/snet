#pragma once
#include "pipe_connection.h"
#include "tcp_connection.h"
#include <inttypes.h>
#include <memory>
#include <vector> 

enum PipeMode {
  PIPE_SERVER_MODE = 1,
  PIPE_CLIENT_MODE = 2,
  PIPE_DUET_MODE = 3
};
enum PipeStatus {
  PIPE_STATUS_INIT,
  PIPE_STATUS_UNBIND,
  PIPE_STATUS_BIND,
};

template <class UserSession> class PipeSession {
public:
  virtual bool handle_event(uint32_t evt) { return true; }

  virtual int32_t handle_message(char *data, uint32_t dataLen,
                                 uint64_t obdata = 0) {

    return 0;
  }

private:
  using PipeConnectionPtr = PipeConnection<UserSession>;
  void process_server_handshake(TPtr conn, PipeMsgHead *msg) {
    if (msg->length > 0) {
      std::string pipeId =
          std::string((const char *)msg + sizeof(PipeMsgHead), msg->length);

      auto pipe = find_bind_pipe(pipeId);
      if (pipe) {
        if (!pipe->is_ready()) {
          pipe->bind(conn);
          ilog("bind pipe {} success", pipeId);
          PipeMsgHead shakeMsg{static_cast<uint32_t>(pipeId.length()),
                               PIPE_MSG_SHAKE_HAND, 0};
          conn->msend(shakeMsg, pipeId);
          pipe->handle_event(NetEvent::EVT_CONNECT);
          pipe->on_ready();
        }
      } else {
        wlog("pipe id not found {}, close connection", pipeId);
        conn->close();
      }
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

  void process_client_handshake(TPtr conn, PipeMsgHead *msg) {
    if (msg->length > 0) {
      std::string pipeId =
          std::string((const char *)msg + sizeof(PipeMsgHead), msg->length);
      PipeSessionPtr session = find_bind_pipe(pipeId);
      if (session) {
        if (!session->is_ready()) {
          session->bind(conn);
          session->update_pipeid(pipeId);
          dlog("bind pipe session success, {}", pipeId);
          session->handle_event(NetEvent::EVT_CONNECT);
        }
      } else {
        session = find_unbind_pipe(conn->get_cid());
        if (session) {
          session->bind(conn);
          session->update_pipeid(pipeId);
          dlog("rebind pipe session success, {}", pipeId);
          add_bind_pipe(pipeId, session);
          remove_unbind_pipe(conn->get_cid());
          session->handle_event(NetEvent::EVT_CONNECT);
          session->on_ready();
        } else {
          wlog("pipe id not found {} cid is {}", pipeId, conn->get_cid());
          conn->close();
        }
      }
    } else {
      conn->send_shakehand(conn->get_pipeid());
      wlog("handshake from server is empty, resend client shakehand {}",
           conn->get_cid());
    }
  }

  std::string pipe_id;
  std::vector<> pipe_connections;

  PipeStatus pipe_status;
};

using PipeSessionPtr = std::shared_ptr<PipeSession>;