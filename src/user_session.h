#pragma once
#include "tcp_connection.h"

class UserSession : public TcpConnection<UserSession>
{
public:
  virtual void handle_event(uint32_t evt)
  {

    printf("handle event %d\n", evt);
  }

  virtual int32_t handle_data(char *data, uint32_t len)
  {

    printf("recv msg %s\n", data);

    //if (is_passive)
    {
      return send(data, len);
    }
    return len;
  }
};
