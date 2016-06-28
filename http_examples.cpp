#include "server_http.hpp"
#include "client_http.hpp"

//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>
#include <boost/filesystem.hpp>
#include <vector>
#include <algorithm>

using namespace std;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;

int main() {
    //HTTP-server at port 8080 using 1 thread
    //Unless you do more heavy non-threaded processing in the resources,
    //1 thread is usually faster than several threads
    HttpServer server(8080, 1);
    
    //Add resources using path-regex and method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    server.resource["^/string$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        //Retrieve string:
        auto content=request->content.string();
        //request->content.string() is a convenience function for:
        //stringstream ss;
        //ss << request->content.rdbuf();
        //string content=ss.str();
        
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    server.resource["^/json$"]["POST"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        try {
            ptree pt;
            read_json(request->content, pt);

            string name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");

            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << name.length() << "\r\n\r\n" << name;
        }
        catch(exception& e) {
            *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    };
    
    //GET-example for the path /info
    //Responds with request-information
    server.resource["^/info$"]["GET"]=[](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        stringstream content_stream;
        content_stream << "<h1>Request from " << request->remote_endpoint_address << " (" << request->remote_endpoint_port << ")</h1>";
        content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
        for(auto& header: request->header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        //find length of content_stream (length received using content_stream.tellp())
        content_stream.seekp(0, ios::end);
        
        *response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    server.resource["^/match/([0-9]+)$"]["GET"]=[&server](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        string number=request->path_match[1];
        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["GET"]=[&server](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
        const auto web_root_path=boost::filesystem::canonical("web");
        boost::filesystem::path path=web_root_path;
        path/=request->path;
        if(boost::filesystem::exists(path)) {
            path=boost::filesystem::canonical(path);
            //Check if path is within web_root_path
            if(distance(web_root_path.begin(), web_root_path.end())<=distance(path.begin(), path.end()) &&
               equal(web_root_path.begin(), web_root_path.end(), path.begin())) {
                if(boost::filesystem::is_directory(path))
                    path/="index.html";
                if(boost::filesystem::exists(path) && boost::filesystem::is_regular_file(path)) {
                    auto ifs=make_shared<ifstream>();
                    ifs->open(path.string(), ifstream::in | ios::binary);
                    
                    if(ifs) {
                        //read and send 128 KB at a time
                        size_t buffer_size=131072;
                        auto buffer=make_shared<vector<char>>(buffer_size);
                        
                        auto send_callback=make_shared<std::function<void(const boost::system::error_code&)> >(nullptr);
                        *send_callback=[&server, response, ifs, buffer, buffer_size, send_callback](const boost::system::error_code &ec) {
                            if(!ec) {
                                streamsize read_length;
                                if((read_length=ifs->read(&(*buffer)[0], buffer_size).gcount())>0) {
                                    response->write(&(*buffer)[0], read_length);
                                    server.send(response, *send_callback);
                                }
                            }
                            else
                                cerr << "Connection interrupted" << endl;
                        };
                        
                        ifs->seekg(0, ios::end);
                        auto length=ifs->tellg();
                        
                        ifs->seekg(0, ios::beg);
                        
                        *response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
                        server.send(response, *send_callback);
                        return;
                    }
                }
            }
        }
        string content="Could not open path "+request->path;
        *response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    //Wait for server to start so that the client can connect
    this_thread::sleep_for(chrono::seconds(1));
    
    //Client examples
    HttpClient client("localhost:8080");
    auto r1=client.request("GET", "/match/123");
    cout << r1->content.rdbuf() << endl;

    string json_string="{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
    auto r2=client.request("POST", "/string", json_string);
    cout << r2->content.rdbuf() << endl;
    
    auto r3=client.request("POST", "/json", json_string);
    cout << r3->content.rdbuf() << endl;
        
    server_thread.join();
    
    return 0;
}
