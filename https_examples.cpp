#include "server_https.hpp"
#include "client_https.hpp"

//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include<fstream>

using namespace std;
//Added for the json-example:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTPS> HttpsServer;
typedef SimpleWeb::Client<SimpleWeb::HTTPS> HttpsClient;

int main() {
    //HTTPS-server at port 8080 using 4 threads
    HttpsServer server(8080, 1, "server.crt", "server.key");
    
    //Add resources using path-regex and method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    server.resource["^/string$"]["POST"]=[](HttpsServer::Response& response, shared_ptr<HttpsServer::Request> request) {
        //Retrieve string from istream (request->content)
        stringstream ss;
        request->content >> ss.rdbuf();
        string content=ss.str();
        
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
    };
    
    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    server.resource["^/json$"]["POST"]=[](HttpsServer::Response& response, shared_ptr<HttpsServer::Request> request) {
        try {
            ptree pt;
            read_json(request->content, pt);

            string name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");
            
            response << "HTTP/1.1 200 OK\r\nContent-Length: " << name.length() << "\r\n\r\n" << name;
        }
        catch(exception& e) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    };
    
    //GET-example for the path /info
    //Responds with request-information
    server.resource["^/info$"]["GET"]=[](HttpsServer::Response& response, shared_ptr<HttpsServer::Request> request) {
        stringstream content_stream;
        content_stream << "<h1>Request:</h1>";
        content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
        for(auto& header: request->header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        //find length of content_stream (length received using content_stream.tellp())
        content_stream.seekp(0, ios::end);
        
        response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    server.resource["^/match/([0-9]+)$"]["GET"]=[](HttpsServer::Response& response, shared_ptr<HttpsServer::Request> request) {
        string number=request->path_match[1];
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["GET"]=[](HttpsServer::Response& response, shared_ptr<HttpsServer::Request> request) {
        string filename="web";
        
        string path=request->path;
        
        //Replace all ".." with "." (so we can't leave the web-directory)
        size_t pos;
        while((pos=path.find(".."))!=string::npos) {
            path.erase(pos, 1);
        }
        
        filename+=path;
        ifstream ifs;
        //A simple platform-independent file-or-directory check do not exist, but this works in most of the cases:
        if(filename.find('.')==string::npos) {
            if(filename[filename.length()-1]!='/')
                filename+='/';
            filename+="index.html";
        }
        ifs.open(filename, ifstream::in);
        
        if(ifs) {
            ifs.seekg(0, ios::end);
            size_t length=ifs.tellg();
            
            ifs.seekg(0, ios::beg);

            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
            
            //read and send 128 KB at a time if file-size>buffer_size
            size_t buffer_size=131072;
            if(length>buffer_size) {
                vector<char> buffer(buffer_size);
                size_t read_length;
                while((read_length=ifs.read(&buffer[0], buffer_size).gcount())>0) {
                    response.stream.write(&buffer[0], read_length);
                    response << HttpsServer::flush;
                }
            }
            else
                response << ifs.rdbuf();

            ifs.close();
        }
        else {
            string content="Could not open file "+filename;
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
        }
    };
    
    thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    //Wait for server to start so that the client can connect
    this_thread::sleep_for(chrono::seconds(1));
    
    //Client examples
    //Second Client() parameter set to false: no certificate verification
    HttpsClient client("localhost:8080", false);
    auto r1=client.request("GET", "/match/123");
    cout << r1->content.rdbuf() << endl;

    string json="{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
    stringstream ss(json);    
    auto r2=client.request("POST", "/string", ss);
    cout << r2->content.rdbuf() << endl;
    
    ss.str(json);
    auto r3=client.request("POST", "/json", ss);
    cout << r3->content.rdbuf() << endl;
    
    server_thread.join();
    
    return 0;
}