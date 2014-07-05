#ifndef HTTPSERVER_HPP
#define	HTTPSERVER_HPP

#include <boost/asio.hpp>

#include <regex>
#include <unordered_map>
#include <thread>

using namespace std;
using namespace boost::asio;

struct Request {
    string method, path, http_version;
    
    shared_ptr<istream> content;
    
    unordered_map<string, string> header;
};

class HTTPServer {
public:
    unordered_map<string, unordered_map<string, function<void(ostream&, const Request&, const smatch&)> > > resources;
    
    HTTPServer(unsigned short, size_t);
    
    void start();
            
private:
    io_service m_io_service;
    ip::tcp::endpoint endpoint;
    ip::tcp::acceptor acceptor;
    size_t num_threads;
    vector<thread> threads;

    void accept();
    
    void process_request_and_respond(shared_ptr<ip::tcp::socket> socket);
    
    Request parse_request(istream& stream);
    
    void respond(shared_ptr<ip::tcp::socket> socket, shared_ptr<Request> request);
};

#endif	/* HTTPSERVER_HPP */

