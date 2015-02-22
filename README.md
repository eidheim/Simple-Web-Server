Simple-Web-Server
=================

A very simple, fast, multithreaded, platform independent HTTP and HTTPS server and client library implemented using C++11 and Boost.Asio. Created to be an easy way to make REST resources available from C++ applications. 

See also https://github.com/eidheim/Simple-WebSocket-Server for an easy way to make WebSocket/WebSocket Secure endpoints in C++. 

### Features

* Thread pool
* Platform independent
* HTTPS support
* HTTP persistent connection (for HTTP/1.1)
* Client supports chunked transfer encoding
* Timeouts, if any of Server::timeout_request and Server::timeout_content are >0 (default: Server::timeout_request=5 seconds, and Server::timeout_content=300 seconds)
* Simple way to add REST resources using regex for path, and anonymous functions
* Possibility to flush response to clients both synchronously (Server::flush) and asynchronously (Server::async_flush).

###Usage

Note: newest version is NOT backward compatible with earlier versions. 

See http_examples.cpp or https_examples.cpp for example usage. 

See particularly the JSON-POST (using Boost.PropertyTree) and the GET /match/[number] examples, which are most relevant.

The default_resource includes example use of Server::flush. Note that Server::async_flush might be slightly slower than Server::flush unless you need to process computationally expensive tasks while simultaneously sending large datasets to a client. 

### Dependencies

Boost C++ libraries must be installed, go to http://www.boost.org for download and instructions. 

For HTTPS: OpenSSL libraries from https://www.openssl.org are required. 

### Compile and run

Compile with a C++11 compiler supporting regex (for instance g++ 4.9):

On Linux using g++: add `-pthread`

Note: added `-lboost_thread` to make the json-example thread safe. Also added `-lboost_coroutine -lboost_context` to make synchronous and asynchronous flushing of response stream work. On some systems you might have to use postfix `-mt` to link to these libraries.

You can now also compile using CMake and make:

```
cmake .
make
```

#### HTTP

`g++ -O3 -std=c++11 http_examples.cpp -lboost_system -lboost_thread -lboost_coroutine -lboost_context -o http_examples`

Then to run the server and client examples: `./http_examples`

Also, direct your favorite browser to for instance http://localhost:8080/

#### HTTPS

`g++ -O3 -std=c++11 https_examples.cpp -lboost_system -lboost_thread -lboost_coroutine -lboost_context -lssl -lcrypto -o https_examples`

Before running the server, an RSA private key (server.key) and an SSL certificate (server.crt) must be created. Follow, for instance, the instructions given here (for a self-signed certificate): http://www.akadia.com/services/ssh_test_certificate.html

Then to run the server and client examples: `./https_examples`

Also, direct your favorite browser to for instance https://localhost:8080/

