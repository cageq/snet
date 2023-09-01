#pragma once 
#include "tcp_connection.h"


class UserSession : public TcpConnection<UserSession>{
    public: 
    
  virtual int32_t handle_data(char *data, uint32_t len) { 
	   if (is_passive){
      printf("recv msg %s\n", data); 
     }
    
	  
	  return send(data, len); 
  }
}; 
