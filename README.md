Simple-Web-Server
=================

A very simple, fast, multithreaded and platform independent HTTP server implemented using C++11 and Boost::Asio. Makes it easy to create REST resources for a C++ application. 

### Features

* Thread pool
* HTTP persistent connection (for HTTP/1.1)
* Simple way to add REST resources using regex for method and path

###Usage

See main.cpp for example usage. 

### Dependency

Boost C++ libraries must be installed, go to http://www.boost.org for download and instructions. 

### Compile and run

Compile with a C++11 compiler supporting regex (for instance g++ 4.9):

g++ -O3 -std=c++11 -lboost_system main.cpp httpserver.cpp -o httpserver

Then to run the server: ./httpserver
