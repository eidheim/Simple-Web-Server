#include "server_http.hpp"

//Added for the json-example:
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include<fstream>

using namespace std;
using namespace SimpleWeb;
//Added for the json-example:
using namespace boost::property_tree;

int main() {
    //HTTP-server at port 8080 using 4 threads
    Server<HTTP> server(8080, 4);
    
    //Add resources using regular expression for path, a method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    server.resource["^/string/?$"]["POST"]=[](ostream& response, Request& request) {
        //Retrieve string from istream (*request.content)
        stringstream ss;
        *request.content >> ss.rdbuf();
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
    server.resource["^/json/?$"]["POST"]=[](ostream& response, Request& request) {
        try {
            ptree pt;
            read_json(*request.content, pt);

            string name=pt.get<string>("firstName")+" "+pt.get<string>("lastName");

            response << "HTTP/1.1 200 OK\r\nContent-Length: " << name.length() << "\r\n\r\n" << name;
        }
        catch(exception& e) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
    };
    
    //GET-example for the path /info
    //Responds with request-information
    server.resource["^/info/?$"]["GET"]=[](ostream& response, Request& request) {
        stringstream content_stream;
        content_stream << "<h1>Request:</h1>";
        content_stream << request.method << " " << request.path << " HTTP/" << request.http_version << "<br>";
        for(auto& header: request.header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }
        
        //find length of content_stream (length received using content_stream.tellp())
        content_stream.seekp(0, ios::end);
        
        response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
    server.resource["^/match/([0-9]+)/?$"]["GET"]=[](ostream& response, Request& request) {
        string number=request.path_match[1];
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
    server.default_resource["^/?(.*)$"]["GET"]=[](ostream& response, Request& request) {
        string filename="web/";
        
        string path=request.path_match[1];
        
        //Remove all but the last '.' (so we can't leave the web-directory)
        size_t last_pos=path.rfind(".");
        size_t current_pos=0;
        size_t pos;
        while((pos=path.find('.', current_pos))!=string::npos && pos!=last_pos) {
            current_pos=pos;
            path.erase(pos, 1);
            last_pos--;
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

            //The file-content is copied to the response-stream. Should not be used for very large files.
            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

            ifs.close();
        }
        else {
            string content="Could not open file "+filename;
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
        }
    };
    
    //Start HTTP-server
    server.start();
    
    return 0;
}
