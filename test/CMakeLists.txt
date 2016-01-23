set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

add_executable(parse_test parse_test.cpp)
target_link_libraries(parse_test ${Boost_LIBRARIES})
target_link_libraries(parse_test ${CMAKE_THREAD_LIBS_INIT})

if(MSYS) #TODO: Is MSYS true when MSVC is true?
    target_link_libraries(parse_test ws2_32 wsock32)
endif()

add_test(parse_test parse_test)
