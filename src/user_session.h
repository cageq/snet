#pragma once
#include "tcp_connection.h"
using namespace snet::tcp;
class UserSession : public TcpConnection<UserSession>
{
public:
  virtual ~UserSession()
  {
    printf("destroy user session \n");
  }
  virtual bool handle_event(snet::NetEvent evt) override
  {

    printf("handle event in session  %d\n", evt);
    return true;
  }

  virtual int32_t handle_data(char *data, uint32_t len) override
  {

//    printf("recv: %.*s\n",len,  data);

    if (is_passive)
    {
      return send(data, len);
    }
    return len;
  }
};
