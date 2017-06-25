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
    
    server.resource["^/string2$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        response->write(request->content.string());
    };
    
    server.resource["^/string3$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        std::stringstream stream;
        stream << request->content.rdbuf();
        response->write(stream);
    };
    
    server.resource["^/string4$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
        response->write(SimpleWeb::StatusCode::client_error_forbidden, {{"Test1", "test2"}, {"tesT3", "test4"}});
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
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::success_ok);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string", "A string");
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::success_ok);
            assert(r->content.string()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string2", "A string");
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::success_ok);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string3", "A string");
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::success_ok);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string4", "A string");
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::client_error_forbidden);
            assert(r->header.size()==3);
            assert(r->header.find("test1")->second=="test2");
            assert(r->header.find("tEst3")->second=="test4");
            assert(r->header.find("content-length")->second=="0");
            output << r->content.rdbuf();
            assert(output.str()=="");
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
        
        HttpClient::Connection *connection;
        {
            // test performing the stream version of the request methods first
            stringstream output;
            stringstream content("A string");
            auto r=client.request("POST", "/string", content);
            output << r->content.rdbuf();
            assert(output.str()=="A string");
            assert(client.connections->size()==1);
            connection=client.connections->front().get();
        }
        
        {
            stringstream output;
            auto r=client.request("POST", "/string", "A string");
            output << r->content.rdbuf();
            assert(output.str()=="A string");
            assert(client.connections->size()==1);
            assert(connection==client.connections->front().get());
        }
        
        {
            stringstream output;
            auto r=client.request("GET", "/header", "", {{"test1", "test"}, {"test2", "ing"}});
            output << r->content.rdbuf();
            assert(output.str()=="testing");
            assert(client.connections->size()==1);
            assert(connection==client.connections->front().get());
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
            assert(client.connections->size()==100);
            client.io_service->reset();
            client.io_service->run();
            assert(client.connections->size()==1);
            for(auto call: calls)
                assert(call);
        }
    }
    
    {
        HttpClient client("localhost:8080");
        assert(client.connections->size()==0);
        for(size_t c=0;c<5000;++c) {
            auto r1=client.request("POST", "/string", "A string");
            assert(SimpleWeb::status_code(r1->status_code)==SimpleWeb::StatusCode::success_ok);
            assert(r1->content.string()=="A string");
            assert(client.connections->size()==1);
            
            stringstream content("A string");
            auto r2 = client.request("POST", "/string", content);
            assert(SimpleWeb::status_code(r2->status_code) == SimpleWeb::StatusCode::success_ok);
            assert(r2->content.string() == "A string");
            assert(client.connections->size() == 1);
        }
    }
    
    for(size_t c=0;c<500;++c) {
        {
            HttpClient client("localhost:8080");
            auto r=client.request("POST", "/string", "A string");
            assert(SimpleWeb::status_code(r->status_code)==SimpleWeb::StatusCode::success_ok);
            assert(r->content.string()=="A string");
            assert(client.connections->size()==1);
        }
        
        {
            HttpClient client("localhost:8080");
            stringstream content("A string");
            auto r = client.request("POST", "/string", content);
            assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
            assert(r->content.string() == "A string");
            assert(client.connections->size() == 1);
        }
    }
    
    server.stop();
    server_thread.join();
    
    return 0;
}
