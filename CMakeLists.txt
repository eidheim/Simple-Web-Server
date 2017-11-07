cmake_minimum_required (VERSION 2.8.8)
project (Simple-Web-Server)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W1")
elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -Wall -Wextra -Wsign-conversion")
endif()

include_directories(.)

find_package(Threads REQUIRED)

if(MSVC)
 set(Boost_USE_STATIC_LIBS ON)
 set(Boost_USE_MULTITHREADED ON)
endif()

set(BOOST_COMPONENTS system filesystem thread)
find_package(Boost 1.53.0 COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()

add_executable(http_examples http_examples.cpp client_http.hpp server_http.hpp status_code.hpp utility.hpp)
target_link_libraries(http_examples ${Boost_LIBRARIES})
target_link_libraries(http_examples ${CMAKE_THREAD_LIBS_INIT})

#TODO: add requirement for version 1.0.1g (can it be done in one line?)
find_package(OpenSSL)

if(OPENSSL_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OPENSSL")
    target_link_libraries(http_examples ${OPENSSL_LIBRARIES})
    include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

    add_executable(https_examples https_examples.cpp client_http.hpp client_https.hpp server_http.hpp server_https.hpp status_code.hpp utility.hpp)
    target_link_libraries(https_examples ${Boost_LIBRARIES})
    target_link_libraries(https_examples ${OPENSSL_LIBRARIES})
    target_link_libraries(https_examples ${CMAKE_THREAD_LIBS_INIT})
endif()

if(MSYS) #TODO: Is MSYS true when MSVC is true?
    target_link_libraries(http_examples ws2_32 wsock32)
    if(OPENSSL_FOUND)
        target_link_libraries(https_examples ws2_32 wsock32)
    endif()
endif()

enable_testing()
add_subdirectory(tests)

install(FILES server_http.hpp client_http.hpp server_https.hpp client_https.hpp crypto.hpp utility.hpp status_code.hpp DESTINATION include/simple-web-server)
