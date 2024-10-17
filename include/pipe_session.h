#pragma once
#include "pipe_connection.h"
#include "tcp_connection.h"
#include <inttypes.h>
#include <memory>
#include <vector>

namespace snet
{

  enum PipeMode
  {
    PIPE_SERVER_MODE = 1,
    PIPE_CLIENT_MODE = 2,
    PIPE_DUET_MODE = 3
  };
  enum PipeStatus
  {
    PIPE_STATUS_INIT,
    PIPE_STATUS_UNBIND,
    PIPE_STATUS_BIND,
  };

  template <class UserSession>
  class PipeSession
  {
  public:
    virtual ~PipeSession() {}
    using PipeConnectionPtr = PipeConnection<UserSession>;
    virtual bool handle_event(uint32_t evt) { return true; }

    virtual int32_t handle_message(char *data, uint32_t dataLen,
                                   uint64_t obdata = 0)
    {
      return 0;
    }

    void on_ready()
    {
      printf("pipe is ready %s \n", pipe_id.c_str());
      pipe_status = PIPE_STATUS_BIND;
    }

    std::string pipe_id;
    std::vector<PipeConnectionPtr> pipe_connections;

    PipeStatus pipe_status;
  };

}