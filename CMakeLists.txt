cmake_minimum_required (VERSION 2.8.8)

project (Simple-Web-Server)

option(USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)
option(BUILD_TESTING "set ON to build library tests" OFF)

if(NOT MSVC)
    add_compile_options(-std=c++11 -Wall -Wextra -Wsign-conversion)
else()
    add_compile_options(/W1)
endif()

add_library(simple-web-server INTERFACE)

target_include_directories(simple-web-server INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(simple-web-server INTERFACE ${CMAKE_THREAD_LIBS_INIT})

# TODO 2020 when Debian Jessie LTS ends:
# Remove Boost system, thread, regex components; use Boost::<component> aliases; remove Boost target_include_directories
if(USE_STANDALONE_ASIO)
    target_compile_definitions(simple-web-server INTERFACE USE_STANDALONE_ASIO)
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(asio.hpp HAVE_ASIO)
    if(NOT HAVE_ASIO)
        message(FATAL_ERROR "Standalone Asio not found")
    endif()
else()
    find_package(Boost 1.53.0 COMPONENTS system thread REQUIRED)
    target_link_libraries(simple-web-server INTERFACE ${Boost_LIBRARIES})
    target_include_directories(simple-web-server INTERFACE ${Boost_INCLUDE_DIR})
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(simple-web-server INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.53.0 COMPONENTS regex REQUIRED)
        target_link_libraries(simple-web-server INTERFACE ${Boost_LIBRARIES})
        target_include_directories(simple-web-server INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()
if(WIN32)
    target_link_libraries(simple-web-server INTERFACE ws2_32 wsock32)
endif()

if(APPLE)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl")
endif()
find_package(OpenSSL)
if(OPENSSL_FOUND)
    target_compile_definitions(simple-web-server INTERFACE HAVE_OPENSSL)
    target_link_libraries(simple-web-server INTERFACE ${OPENSSL_LIBRARIES})
    target_include_directories(simple-web-server INTERFACE ${OPENSSL_INCLUDE_DIR})
endif()

# If Simple-Web-Server is not a sub-project:
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    add_executable(http_examples http_examples.cpp)
    target_link_libraries(http_examples simple-web-server)
    find_package(Boost 1.53.0 COMPONENTS system thread filesystem REQUIRED)
    target_link_libraries(http_examples ${Boost_LIBRARIES})
    target_include_directories(http_examples PRIVATE ${Boost_INCLUDE_DIR})
    if(OPENSSL_FOUND)
        add_executable(https_examples https_examples.cpp)
        target_link_libraries(https_examples simple-web-server)
        target_link_libraries(https_examples ${Boost_LIBRARIES})
        target_include_directories(https_examples PRIVATE ${Boost_INCLUDE_DIR})
    endif()
    
    set(BUILD_TESTING ON)
    
    install(FILES server_http.hpp client_http.hpp server_https.hpp client_https.hpp crypto.hpp utility.hpp status_code.hpp DESTINATION include/simple-web-server)
endif()

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
