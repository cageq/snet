#pragma once

#include "http_connection.h"
#include "tcp/tcp_factory.h" 
using namespace snet::tcp; 

namespace snet
{
    namespace http
    {
        template <class T = HttpConnection>
        class HttpFactory : public TcpFactory<T> 
        {

        public:
            using ConnectionPtr = std::shared_ptr<T>;

            inline void register_global_router(const HttpHandler &router)
            {
                global_router = router;
            }
            inline void register_global_router(const HttpRequestHandler &handler)
            {
                global_router.http_handler = handler;
            }

            inline void register_global_router(const HttpContextHandler &handler)
            {
                global_router.context_handler = handler;
            }


            void register_router(const std::string& path, const HttpHandler & handler) {		 
                http_routers[path] = handler;	  
            }

            void register_router(const std::string& path, const HttpRequestHandler & handler) {		 
                http_routers[path].http_handler = handler;	  
            }

            void register_router(const std::string& path, const HttpContextHandler & handler) {		 
                http_routers[path].context_handler = handler;	  
            }

            virtual void on_create(uint64_t cid, ConnectionPtr conn) override 
            {
                conn->init_routers(global_router, &http_routers); 
            } 

            HttpHandler  global_router ; 
            HttpRouteMap http_routers;
        };

    } // namespace http
} // namespace snet
