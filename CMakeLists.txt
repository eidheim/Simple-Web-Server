cmake_minimum_required (VERSION 2.8.8)
project (Simple-Web-Server)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
include(${CMAKE_SOURCE_DIR}/cmake/Modules/SimpleWebServer.cmake)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

if("${SimpleWebServer_USE}" STREQUAL "boost")
    find_package(Boost 1.53.0 COMPONENTS "filesystem" REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUSE_BOOST_FS")
    set(FS_LIBRARIES ${Boost_LIBRARIES})
else()
    set(FS_LIBRARIES stdc++fs)
endif()

include_directories(.)

add_executable(http_examples http_examples.cpp)
target_link_libraries(http_examples ${SimpleWebServer_LIBRARIES} ${FS_LIBRARIES})
target_link_libraries(http_examples ${CMAKE_THREAD_LIBS_INIT})

if(OPENSSL_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_OPENSSL")
    target_link_libraries(http_examples ${OPENSSL_LIBRARIES})
    include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

    add_executable(https_examples https_examples.cpp)
    target_link_libraries(https_examples ${SimpleWebServer_LIBRARIES} ${FS_LIBRARIES})
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

install(FILES server_http.hpp client_http.hpp server_https.hpp client_https.hpp DESTINATION include/simple-web-server)
#TODO: Should probably install SimpleWebServer.cmake somewhere too
