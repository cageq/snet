cmake_minimum_required (VERSION 3.6.0)
project (snet)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

#add_subdirectory(opt/boringssl EXCLUDE_FROM_ALL)
#include_directories(opt/boringssl/include) 
if(APPLE)
    set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
	FIND_PACKAGE(OpenSSL)
	if(${OpenSSL_FOUND})
		include_directories(${OPENSSL_INCLUDE_DIR})
		link_directories(${OPENSSL_ROOT_DIR}/lib)
	endif()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall  -Wno-unused-function -g  -Wno-deprecated-declarations  ") #  -stdlib=libc++ 
elseif(UNIX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread -g  -Wno-unused-function -Wno-deprecated ")
elseif(WIN32)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}   -D NDEBUG")
endif()


add_definitions(-DFMT_HEADER_ONLY=ON)


include_directories(include)
include_directories(include/http)
include_directories(include/pipe)
include_directories(include/tcp)
include_directories(include/utils)

# add_subdirectory(opt/fmt-10.2.1 EXECLUDE_FROM_ALL ) 
include_directories(fmt-10.2.1/include)

#add_executable(tcpserver src/tcp_server.cpp)
add_executable(tcpclient  src/democlient.cpp)

add_executable(tcpserver src/demoserver.cpp)
add_executable(webserver src/webserver.cpp)
 

add_executable(pipecli src/pipecli.cpp)
add_executable(pipesrv src/pipesrv.cpp)
