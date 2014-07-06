Simple-Web-Server
=================

A very simple, fast, multithreaded and platform independent HTTP server implemented using C++11 and Boost.Asio. Created to be an easy way to make REST resources available from C++ applications. 

### Features

* Thread pool
* Platform independent
* HTTP persistent connection (for HTTP/1.1)
* Simple way to add REST resources using regex for path, and anonymous functions

HTTPS is not yet supported, but take a look at http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/example/cpp03/ssl/server.cpp. It does not seem that httpserver.hpp and httpserver.cpp require significant modifications to support HTTPS. 

###Usage

See main.cpp for example usage. 

See particularly the JSON-POST (using Boost.PropertyTree) and the GET /match/[number] examples, which are most relevant.

### Dependency

Boost C++ libraries must be installed, go to http://www.boost.org for download and instructions. 

Will update to use C++17 networking instead in the future when it is supported by g++. 

### Compile and run

Compile with a C++11 compiler supporting regex (for instance g++ 4.9):

g++ -O3 -std=c++11 -lboost_system main.cpp httpserver.cpp -o httpserver

Then to run the server: ./httpserver

Finally, direct your favorite browser to for instance http://localhost:8080/
