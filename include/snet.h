#pragma once 
#include <inttypes.h>
#include <string> 
#include <thread> 
#include "utils/net_url.h"
namespace snet
{
 

	struct NetOptions
	{
        bool tcp_delay{false}; 
		bool sync {false};
		bool reuse { true} ;
		uint32_t backlogs = 128;								// listening queue size
		uint32_t workers =  std::thread::hardware_concurrency()  ; // iocp/epoll workers number
		uint32_t send_buffer_size = 2 * 1024 *1024 ;
		uint32_t recv_buffer_size = 2 * 1024 *1024 ;
		uint32_t sync_mode = 0;    
        uint32_t sync_accept_threads = 0 ; 
        std::string encryption  ; // empty no crypt else  dh+rc4 
		std::string chain_file = "cert/server.pem";
		std::string dh_file = "cert/dh2048.pem";
	};

    inline NetOptions options_from_url(const utils::NetUrl & urlInfo){
        NetOptions opts ;
        opts.tcp_delay = urlInfo.get<bool>("tcp_delay", opts.tcp_delay); 
        opts.sync = urlInfo.get<bool>("sync", opts.sync); 
        opts.reuse = urlInfo.get<bool>("reuse", opts.reuse); 
        opts.backlogs = urlInfo.get<uint32_t >("backlogs", opts.backlogs); 
        opts.send_buffer_size = urlInfo.get<uint32_t >("sbuf_size", opts.send_buffer_size); 
        opts.recv_buffer_size = urlInfo.get<uint32_t >("rbuf_size",opts.recv_buffer_size); 
        opts.encryption = urlInfo.get("encryption", opts.encryption); 
        opts.chain_file = urlInfo.get("cert", opts.chain_file); 
        opts.dh_file    = urlInfo.get("cert_key",opts.dh_file); 
        return opts; 
    }

    inline NetOptions options_from_url(const std::string & url){
        utils::NetUrl urlInfo(url); 
        return options_from_url(urlInfo); 
    }


} // namespace snet



#include "tcp_connection.h"
#include "tcp_listener.h"
#include "tcp_connector.h"
#include "tcp_pipe.h"
