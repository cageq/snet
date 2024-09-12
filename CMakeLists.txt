cmake_minimum_required (VERSION 3.1.0)
project (snet)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -Wall  -Wno-unused-function -g  -Wno-deprecated-declarations  ") 
elseif(UNIX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pthread  -g -Wno-unused-function -Wno-deprecated ")
elseif(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}   -D NDEBUG")
endif()

include_directories(include)


add_executable(tcpclient src/tcpclient.cpp)
add_executable(tcpserver src/tcpserver.cpp)


add_executable(pipecli src/pipecli.cpp)
add_executable(pipesrv src/pipesrv.cpp)


add_executable(testptr src/testptr.cpp )

