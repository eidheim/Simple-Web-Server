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
    
    server.resource["^/header$"]["GET"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        auto content=request->header.find("test1")->second+request->header.find("test2")->second;
        
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
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
        
        {
            stringstream output;
            auto r=client.request("GET", "/header", "", {{"test1", "test"}, {"test2", "ing"}});
            output << r->content.rdbuf();
            assert(output.str()=="testing");
        }
    }
    
    {
        HttpClient client("localhost:8080");
        bool call=false;
        client.request("GET", "/match/123", [&call](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
            assert(!ec);
            stringstream output;
            output << response->content.rdbuf();
            assert(output.str()=="123");
            call=true;
        });
        client.io_service->run();
        assert(call);
        
        {
            vector<int> calls(100);
            vector<thread> threads;
            for(size_t c=0;c<100;++c) {
                calls[c]=0;
                threads.emplace_back([c, &client, &calls] {
                    client.request("GET", "/match/123", [c, &calls](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
                        assert(!ec);
                        stringstream output;
                        output << response->content.rdbuf();
                        assert(output.str()=="123");
                        calls[c]=1;
                    });
                });
            }
            for(auto &thread: threads)
                thread.join();
            client.io_service->reset();
            client.io_service->run();
            for(auto call: calls)
                assert(call);
        }
    }
    
    server.stop();
    server_thread.join();
    
    return 0;
}
