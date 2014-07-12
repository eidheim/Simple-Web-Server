Simple-Web-Server
=================

A very simple, fast, multithreaded and platform independent HTTP and HTTPS server implemented using C++11 and Boost.Asio. Created to be an easy way to make REST resources available from C++ applications. 

### Features

* Thread pool
* Platform independent
* HTTPS support
* HTTP persistent connection (for HTTP/1.1)
* Simple way to add REST resources using regex for path, and anonymous functions

###Usage

See main_http.cpp or main_https.cpp for example usage. 

See particularly the JSON-POST (using Boost.PropertyTree) and the GET /match/[number] examples, which are most relevant.

### Dependency

Boost C++ libraries must be installed, go to http://www.boost.org for download and instructions. 

For HTTPS: OpenSSL libraries from https://www.openssl.org are required. 

Will update to use C++17 networking instead in the future when it is supported by g++. 

### Compile and run

Compile with a C++11 compiler supporting regex (for instance g++ 4.9):

#### HTTP

g++ -O3 -std=c++11 -lboost_system main_http.cpp -o http_server

Then to run the server: ./http_server

Finally, direct your favorite browser to for instance http://localhost:8080/

#### HTTPS

g++ -O3 -std=c++11 -lboost_system -lssl -lcrypto main_https.cpp -o https_server

Before running the server, an RSA private key (server.key) and a (self-signed) certificate (server.crt) must be created. Follow, for instance, the instructions given here: http://www.akadia.com/services/ssh_test_certificate.html

Then to run the server: ./https_server

Finally, direct your favorite browser to for instance https://localhost:8080/

