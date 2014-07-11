#include "server.hpp"

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
    Server<HTTP> httpserver(8080, 4);
    
    //Add resources using regular expression for path, a method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    httpserver.resources["^/string/?$"]["POST"]=[](ostream& response, const Request& request, const smatch& path_match) {
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
    httpserver.resources["^/json/?$"]["POST"]=[](ostream& response, const Request& request, const smatch& path_match) {
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
    httpserver.resources["^/info/?$"]["GET"]=[](ostream& response, const Request& request, const smatch& path_match) {
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
    httpserver.resources["^/match/([0-9]+)/?$"]["GET"]=[](ostream& response, const Request& request, const smatch& path_match) {
        string number=path_match[1];
        response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
    };
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve a HTML 5 client that uses REST-resources on this server
    httpserver.default_resource["^/?(.*)$"]["GET"]=[](ostream& response, const Request& request, const smatch& path_match) {
        string filename="web/";
        
        string path=path_match[1];
        
        //Remove all but the last '.' (so we can't leave the web-directory)
        size_t last_pos=path.rfind(".");
        size_t current_pos=0;
        size_t pos;
        while((pos=path.find('.', current_pos))!=string::npos && pos!=last_pos) {
            cout << pos << endl;
            current_pos=pos;
            path.erase(pos, 1);
            last_pos--;
        }
        
        filename+=path;
        ifstream ifs;
        //HTTP file-or-directory check:
        if(filename.find('.')!=string::npos) {
            ifs.open(filename, ifstream::in);
        }
        else {
            if(filename[filename.length()-1]!='/')
                filename+='/';
            filename+="index.html";
            ifs.open(filename, ifstream::in);            
        }
        
        if(ifs) {
            ifs.seekg(0, ios::end);
            size_t length=ifs.tellg();
            
            ifs.seekg(0, ios::beg);

            response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

            ifs.close();
        }
        else {
            string content="Could not open file "+filename;
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
        }
    };
    
    //Start HTTP-server
    httpserver.start();
    
    return 0;
}
