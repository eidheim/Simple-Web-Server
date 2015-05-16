add_executable(parse_test parse_test.cpp)
target_link_libraries(parse_test ${Boost_LIBRARIES})
target_link_libraries(parse_test ${CMAKE_THREAD_LIBS_INIT})
add_test(parse_test parse_test)
