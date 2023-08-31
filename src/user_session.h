#pragma once 
#include "tcp_connection.h"


class UserSession : public TcpConnection<UserSession>{
    public: 
    
  virtual int32_t handle_data(char *data, uint32_t len) { 
	  
	  
	  return send(data, len); 
  }
}; 
