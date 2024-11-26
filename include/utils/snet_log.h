#pragma once

#include <stdio.h> 
#include <functional>
#include <inttypes.h>
#include <string> 
#include <fmt/format.h>
#include <fmt/chrono.h>



namespace snet {
    namespace log {
 
        enum SNetLogLevel :uint32_t {
            LOG_LEVEL_OFF, 
            LOG_LEVEL_TRACE,
            LOG_LEVEL_DEBUG,
            LOG_LEVEL_INFO,
            LOG_LEVEL_WARN,
            LOG_LEVEL_ERROR,
            LOG_LEVEL_FATAL,
        }; 

		static const char * log_level_str [] = {

			"off", 
			"trace",
			"debug", 
			"info",
			"warn",
			"error", 
			"fatal", 
		}; 
        //level : 0 off, 1 trace, 2 debug, 3 info , 4 warn, 5 error, 6 fatal , 0x10 extend
		using SNetLogWriter = std::function<void(uint32_t level, const std::string &  msg )> ; 

        inline uint32_t from_string_level(const std::string & lv ){
            std::string str = lv; 
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
                return std::tolower(c);
            });
            
            if (str == "off" || lv.empty()){
                return SNetLogLevel::LOG_LEVEL_OFF; 
            }
            if (str == "trace"){
                return SNetLogLevel::LOG_LEVEL_TRACE; 
            }
            if (str == "debug"){
                return SNetLogLevel::LOG_LEVEL_DEBUG; 
            }

            if (str == "info"){
                return SNetLogLevel::LOG_LEVEL_INFO; 
            }

            if (str == "warn"){
                return SNetLogLevel::LOG_LEVEL_WARN; 
            }

            if (str == "error"){
                return SNetLogLevel::LOG_LEVEL_ERROR; 
            }

            if (str == "fatal"){
                return SNetLogLevel::LOG_LEVEL_FATAL; 
            }
            return SNetLogLevel::LOG_LEVEL_OFF; 
        }

		//need c++17 

#if __GLIBCXX__  >   20190311  
        inline SNetLogLevel snet_log_level = SNetLogLevel::LOG_LEVEL_OFF; 
        inline SNetLogWriter  snet_logger; 
#else 
        static SNetLogLevel snet_log_level = SNetLogLevel::LOG_LEVEL_OFF; 
        static SNetLogWriter  snet_logger; 
#endif // 

        inline void register_logger(SNetLogWriter logger ){
            snet_logger = logger; 
        }

        inline void set_log_level(uint32_t level){
            snet_log_level = static_cast<SNetLogLevel>(level);             
        }

        template <class ... Args> 
        void snet_tlog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_TRACE){
                snet_logger(SNetLogLevel::LOG_LEVEL_TRACE, fmt::format(format, std::forward<Args >(args)... )); 
            }    
        }


        template <class ... Args> 
        void snet_dlog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_DEBUG){         
                snet_logger(SNetLogLevel::LOG_LEVEL_DEBUG, fmt::format(format, std::forward<Args >(args)... )); 
            }    
        }

        template <class ... Args> 
        void snet_ilog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_INFO){
                snet_logger(SNetLogLevel::LOG_LEVEL_INFO, fmt::format(format, std::forward<Args >(args)...  )); 
            }    
        }


        template <class ... Args> 
        void snet_wlog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_WARN){
                snet_logger(SNetLogLevel::LOG_LEVEL_WARN, fmt::format(format,std::forward<Args >(args)...  )); 
            }    
        }

        template <class ... Args> 
        void snet_elog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_ERROR){
                snet_logger(SNetLogLevel::LOG_LEVEL_ERROR, fmt::format(format, std::forward<Args >(args)... )); 
            }    
        }


        template <class ... Args> 
        void snet_clog(fmt::format_string<Args ...>  format, Args && ... args ){
            if (snet_logger && snet_log_level <= SNetLogLevel::LOG_LEVEL_FATAL){
                snet_logger(SNetLogLevel::LOG_LEVEL_FATAL, fmt::format(format, std::forward<Args >(args)... )); 
            }    
        }

    } //namespace log 
}  //namespace snet 


inline void snet_init_logger(uint32_t level, snet::log::SNetLogWriter logWriter = nullptr)
{
	if (logWriter != nullptr){    
		snet::log::register_logger(logWriter); 
	}
	snet::log::set_log_level(0x1F & level); 
}

inline void snet_add_console_sink(uint32_t level = 1){
	auto consoleSink  = [](uint32_t level, const std::string & log ){
		auto lv = static_cast<snet::log::SNetLogLevel>(level); 
		printf("[snet][%s] %s\n",snet::log::log_level_str[lv],  log.c_str()); 
	}; 
	snet_init_logger(level, consoleSink); 
}
