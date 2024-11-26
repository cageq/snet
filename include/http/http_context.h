#pragma once
#include <functional> 
#include "http_request.h"
#include "http_response.h"
 
namespace snet
{
    namespace http
    { 
 
        class HttpContext
        {
        public: 
            HttpRequest  request; 
            HttpResponse response; 

            int32_t write(const std::string & msg){
                return response.write(msg); 
            }

        };


        using HttpContextPtr = std::shared_ptr<HttpContext>;  

        using HttpRequestHandler = std::function<int32_t (HttpRequest &, HttpResponse & )> ; 
		using HttpContextHandler = std::function<int32_t (HttpContextPtr )> ; 

    }

}