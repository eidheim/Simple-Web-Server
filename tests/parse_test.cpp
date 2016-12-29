#include "server_http.hpp"
#include "client_http.hpp"
#include <iostream>

using namespace std;
using namespace SimpleWeb;

class ServerTest : public ServerBase<HTTP> {
public:
    ServerTest() : 
            ServerBase<HTTP>::ServerBase(8080) {}
            
    void accept() {}
    
    bool parse_request_test() {
        HTTP socket(*io_service);
        std::shared_ptr<Request> request(new Request(socket));
        
        std::ostream stream(&request->content.streambuf);
        stream << "GET /test/ HTTP/1.1\r\n";
        stream << "TestHeader: test\r\n";
        stream << "TestHeader2:test2\r\n";
        stream << "\r\n";
        
        if(!parse_request(request))
            return 0;
        
        if(request->method!="GET")
            return 0;
        if(request->path!="/test/")
            return 0;
        if(request->http_version!="1.1")
            return 0;
        
        if(request->header.size()!=2)
            return 0;
        auto header_it=request->header.find("TestHeader");
        if(header_it==request->header.end() || header_it->second!="test")
            return 0;
        header_it=request->header.find("TestHeader2");
        if(header_it==request->header.end() || header_it->second!="test2")
            return 0;
        
        return 1;
    }
};

class ClientTest : public ClientBase<HTTP> {
public:
    ClientTest(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}
    
    void connect() {}
    
    bool constructor_parse_test1() {
        if(host!="test.org")
            return 0;
        if(port!=8080)
            return 0;
        
        return 1;
    }
    
    bool constructor_parse_test2() {
        if(host!="test.org")
            return 0;
        if(port!=80)
            return 0;
        
        return 1;
    }
    
    bool parse_response_header_test() {
        std::shared_ptr<Response> response(new Response());
        
        ostream stream(&response->content_buffer);
        stream << "HTTP/1.1 200 OK\r\n";
        stream << "TestHeader: test\r\n";
        stream << "TestHeader2:test2\r\n";
        stream << "\r\n";
        
        parse_response_header(response);
        
        if(response->http_version!="1.1")
            return 0;
        if(response->status_code!="200 OK")
            return 0;
        
        if(response->header.size()!=2)
            return 0;

        auto header_it=response->header.find("TestHeader");
        if(header_it==response->header.end() || header_it->second!="test")
          return 0;
        header_it=response->header.find("TestHeader2");
        if(header_it==response->header.end() || header_it->second!="test2")
          return 0;
        
        return 1;
    }
};

int main() {
    ServerTest serverTest;
    serverTest.io_service=std::make_shared<boost::asio::io_service>();
    
    if(!serverTest.parse_request_test()) {
        cerr << "FAIL Server::parse_request" << endl;
        return 1;
    }
    
    ClientTest clientTest("test.org:8080");
    if(!clientTest.constructor_parse_test1()) {
        cerr << "FAIL Client::Client" << endl;
        return 1;
    }
    
    ClientTest clientTest2("test.org");
    if(!clientTest2.constructor_parse_test2()) {
        cerr << "FAIL Client::Client" << endl;
        return 1;
    }
    
    if(!clientTest2.parse_response_header_test()) {
        cerr << "FAIL Client::parse_response_header" << endl;
        return 1;
    }
    
    return 0;
}