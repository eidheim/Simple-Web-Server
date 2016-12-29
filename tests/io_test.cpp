#include "server_http.hpp"
#include "client_http.hpp"

#include <cassert>

using namespace std;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;

int main() {
    HttpServer server;
    server.config.port=8080;
    
    server.resource["^/string$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        auto content=request->content.string();
        
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    server.resource["^/info$"]["GET"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        stringstream content_stream;
        content_stream << request->method << " " << request->path << " " << request->http_version << " ";
        content_stream << request->header.find("test parameter")->second;

        content_stream.seekp(0, ios::end);
        
        *response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };
    
    server.resource["^/match/([0-9]+)$"]["GET"]=[&server](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        string number=request->path_match[1];
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    this_thread::sleep_for(chrono::seconds(1));
    {
        HttpClient client("localhost:8080");
    
        {
            stringstream output;
            auto r=client.request("POST", "/string", "A string");
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            stringstream content("A string");
            auto r=client.request("POST", "/string", content);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("GET", "/info", "", {{"Test Parameter", "test value"}});
            output << r->content.rdbuf();
            assert(output.str()=="GET /info 1.1 test value");
        }
        
        {
            stringstream output;
            auto r=client.request("GET", "/match/123");
            output << r->content.rdbuf();
            assert(output.str()=="123");
        }
    }
    {
        HttpClient client("localhost:8080");
        
        // test performing the stream version of the request methods first
        {
            stringstream output;
            stringstream content("A string");
            auto r=client.request("POST", "/string", content);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string", "A string");
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
    }
    
    server.stop();
    server_thread.join();
    
    return 0;
}
