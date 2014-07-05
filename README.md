Simple-Web-Server
=================

A very simple, fast, multithreaded and platform independent HTTP server implemented using C++11 and Boost::Asio. Makes it easy to create REST resources for a C++ application. 

See main.cpp for example usage. 

Compile with a C++11 compiler supporting regex (for instance g++ 4.9):

g++ -O3 -std=c++11 -lboost_system main.cpp httpserver.cpp -o httpserver
