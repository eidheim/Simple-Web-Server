#include "server_http.hpp"
#include "client_http.hpp"
#include <iostream>
#include <cassert>

using namespace std;
using namespace SimpleWeb;

class ServerTest : public ServerBase<HTTP> {
public:
    ServerTest() : ServerBase<HTTP>::ServerBase(8080) {}
            
    void accept() {}
    
    void parse_request_test() {
        HTTP socket(*io_service);
        std::shared_ptr<Request> request(new Request(socket));
        
        std::ostream stream(&request->content.streambuf);
        stream << "GET /test/ HTTP/1.1\r\n";
        stream << "TestHeader: test\r\n";
        stream << "TestHeader2:test2\r\n";
        stream << "TestHeader3:test3a\r\n";
        stream << "TestHeader3:test3b\r\n";
        stream << "\r\n";
        
        assert(parse_request(request));
        
        assert(request->method=="GET");
        assert(request->path=="/test/");
        assert(request->http_version=="1.1");
        
        assert(request->header.size()==4);
        auto header_it=request->header.find("TestHeader");
        assert(header_it!=request->header.end() && header_it->second=="test");
        header_it=request->header.find("TestHeader2");
        assert(header_it!=request->header.end() && header_it->second=="test2");
        
        header_it=request->header.find("testheader");
        assert(header_it!=request->header.end() && header_it->second=="test");
        header_it=request->header.find("testheader2");
        assert(header_it!=request->header.end() && header_it->second=="test2");
        
        auto range=request->header.equal_range("testheader3");
        auto first=range.first;
        auto second=first;
        ++second;
        assert(range.first!=request->header.end() && range.second!=request->header.end() &&
               ((first->second=="test3a" && second->second=="test3b") ||
                (first->second=="test3b" && second->second=="test3a")));
    }
};

class ClientTest : public ClientBase<HTTP> {
public:
    ClientTest(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}
    
    void connect() {}
    
    void constructor_parse_test1() {
        assert(host=="test.org");
        assert(port==8080);
    }
    
    void constructor_parse_test2() {
        assert(host=="test.org");
        assert(port==80);
    }
    
    void parse_response_header_test() {
        std::shared_ptr<Response> response(new Response());
        
        ostream stream(&response->content_buffer);
        stream << "HTTP/1.1 200 OK\r\n";
        stream << "TestHeader: test\r\n";
        stream << "TestHeader2:test2\r\n";
        stream << "TestHeader3:test3a\r\n";
        stream << "TestHeader3:test3b\r\n";
        stream << "\r\n";
        
        parse_response_header(response);
        
        assert(response->http_version=="1.1");
        assert(response->status_code=="200 OK");
        
        assert(response->header.size()==4);
        auto header_it=response->header.find("TestHeader");
        assert(header_it!=response->header.end() && header_it->second=="test");
        header_it=response->header.find("TestHeader2");
        assert(header_it!=response->header.end() && header_it->second=="test2");
        
        header_it=response->header.find("testheader");
        assert(header_it!=response->header.end() && header_it->second=="test");
        header_it=response->header.find("testheader2");
        assert(header_it!=response->header.end() && header_it->second=="test2");
        
        auto range=response->header.equal_range("testheader3");
        auto first=range.first;
        auto second=first;
        ++second;
        assert(range.first!=response->header.end() && range.second!=response->header.end() &&
               ((first->second=="test3a" && second->second=="test3b") ||
                (first->second=="test3b" && second->second=="test3a")));
    }
};

int main() {
    ServerTest serverTest;
    serverTest.io_service=std::make_shared<boost::asio::io_service>();
    
    serverTest.parse_request_test();
    
    ClientTest clientTest("test.org:8080");
    clientTest.constructor_parse_test1();
    
    ClientTest clientTest2("test.org");
    clientTest2.constructor_parse_test2();
    
    clientTest2.parse_response_header_test();


    boost::asio::io_service io_service;
    boost::asio::ip::tcp::socket socket(io_service);
    SimpleWeb::Server<HTTP>::Request request(socket);
    {
        request.path = "/?";
        auto queries = request.parse_query_string();
        assert(queries.empty());
    }
    {
        request.path = "/";
        auto queries = request.parse_query_string();
        assert(queries.empty());
    }
    {
        request.path = "/?a=1%202%20%203&b=3+4&c&d=æ%25ø%26å%3F";
        auto queries = request.parse_query_string();
        {
            auto range = queries.equal_range("a");
            assert(range.first != range.second);
            assert(range.first->second == "1 2  3");
        }
        {
            auto range = queries.equal_range("b");
            assert(range.first != range.second);
            assert(range.first->second == "3 4");
        }
        {
            auto range = queries.equal_range("c");
            assert(range.first != range.second);
            assert(range.first->second == "");
        }
        {
            auto range = queries.equal_range("d");
            assert(range.first != range.second);
            assert(range.first->second == "æ%ø&å?");
        }
    }
}
