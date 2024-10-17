#pragma once
#include "tcp_connection.h"
using namespace snet; 
class UserSession : public TcpConnection<UserSession>
{
public:
  virtual bool handle_event(NetEvent evt) override
  {

    printf("handle event in session  %d\n", evt);
    return true; 
  }

  virtual int32_t handle_data(char *data, uint32_t len) override
  {

    printf("recv: %s\n", data);

    if (is_passive)
    {
      return send(data, len);
    }
    return len;
  }
};
