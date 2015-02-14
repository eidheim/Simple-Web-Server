#include "server_http.hpp"
#include "client_http.hpp"

//Added for the json-example
#define BOOST_SPIRIT_THREADSAFE
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

//Added for the default_resource example
#include <fstream>

using namespace SimpleWeb;
//Added for the json-example:
using namespace boost::property_tree;

int main() {

	// HTTP Example

    //HTTP-server at port 8080 using 3 threads
    Server<HTTP> server(8080, 3);

	// HTTPS Example

    //HTTPS-server at port 8080 using 3 threads
    //Server<HTTPS> server(8080, 3, "server.crt", "server.key");
    
    //Add resources using regular expression for path, a method-string, and an anonymous function
    //POST-example for the path /string, responds the posted string
    //  If C++14: use 'auto' instead of 'shared_ptr<Server<HTTPS>::Request>'
	server.resource_map.insert(std::make_pair("^/string?$", 
    	Callback(CallbackType::POST, [](std::ostream& response, std::shared_ptr<Request> request) {
        	//Retrieve string from istream (*request.content)
			std::stringstream ss;
        	request->content >> ss.rdbuf();
			std::string content=ss.str();
        	
        	response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
		})
	));
    
    //POST-example for the path /json, responds firstName+" "+lastName from the posted json
    //Responds with an appropriate error message if the posted json is not valid, or if firstName or lastName is missing
    //Example posted json:
    //{
    //  "firstName": "John",
    //  "lastName": "Smith",
    //  "age": 25
    //}
    server.resource_map.insert(std::make_pair("^/json/?$",
	 	Callback(CallbackType::POST, [](std::ostream& response, std::shared_ptr<Request> request) {
        	try {
        	    ptree pt;
        	    read_json(request->content, pt);

				std::string name=pt.get<std::string>("firstName")+" "+pt.get<std::string>("lastName");

        	    response << "HTTP/1.1 200 OK\r\nContent-Length: " << name.length() << "\r\n\r\n" << name;
        	}
        	catch(std::exception& e) {
        	    response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        	}
		})
    ));
    
    //GET-example for the path /info
    //Responds with request-information
    server.resource_map.insert(std::make_pair("^/info/?$", 
		Callback(CallbackType::GET, [](std::ostream& response, std::shared_ptr<Request> request) {
			std::stringstream content_stream;
        	content_stream << "<h1>Request:</h1>";
        	content_stream << request->method << " " << request->path << " HTTP/" << request->http_version << "<br>";
        	for(auto& header: request->header) {
        	    content_stream << header.first << ": " << header.second << "<br>";
        	}
        	
        	//find length of content_stream (length received using content_stream.tellp())
        	content_stream.seekp(0, std::ios::end);
        	
        	response <<  "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
		})
    ));
    
    //GET-example for the path /match/[number], responds with the matched string in path (number)
    //For instance a request GET /match/123 will receive: 123
	//This uses the resource_vec. The Callback will be used when the request
	//path begins with the passed path. No regex match is allowed.
    server.resource_vec.push_back(std::make_pair("/match", 
		Callback(CallbackType::GET, [](std::ostream& response, std::shared_ptr<Request> request) {
        	//string number=request->path_match[1];
			std::string number=request->path.substr(7);
        	response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n" << number;
		})
    ));
    
    //Default GET-example. If no other matches, this anonymous function will be called. 
    //Will respond with content in the web/-directory, and its subdirectories.
    //Default file: index.html
    //Can for instance be used to retrieve an HTML 5 client that uses REST-resources on this server
	//It is important to set the callback type as it is INVALID by default
	//which leads to the callback not being executed. No regex match is
	//allowed. There are two default callbacks, one for POST and one for GET.
	//The default callbacks match every path, only the method is important.
	server.default_resource.type = CallbackType::GET;
    server.default_resource.functions[static_cast<size_t>(CallbackType::GET)] = 
		[](std::ostream& response, std::shared_ptr<Request> request) 
	{
		std::string filename="web/";
   		
		std::string path;
		if(!request->path.empty() && request->path[0] == '/') {
			path = request->path.substr(1);
		} else {
			path = request->path;
		}
   		
   		//Replace all ".." with "." (so we can't leave the web-directory)
   		size_t pos;
   		while((pos=path.find(".."))!=std::string::npos) {
   		    path.erase(pos, 1);
   		}
   		
   		filename+=path;
		std::ifstream ifs;
   		//A simple platform-independent file-or-directory check do not exist, but this works in most of the cases:
   		if(filename.find('.')==std::string::npos) {
   		    if(filename[filename.length()-1]!='/')
   		        filename+='/';
   		    filename+="index.html";
   		}
   		ifs.open(filename, std::ifstream::in);
   		
   		if(ifs) {
   		    ifs.seekg(0, std::ios::end);
   		    size_t length=ifs.tellg();
   		    
   		    ifs.seekg(0, std::ios::beg);

   		    //The file-content is copied to the response-stream. Should not be used for very large files.
   		    response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n" << ifs.rdbuf();

   		    ifs.close();
   		} else {
			std::string content="Could not open file "+filename;
   		    response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
   		}
   	};
    
	std::thread server_thread([&server](){
        //Start server
        server.start();
    });
    
    //Wait for server to start so that the client can connect
	std::this_thread::sleep_for(std::chrono::seconds(1));
    
    //Client examples
    Client<HTTP> client("localhost:8080");
    auto r1=client.request("GET", "/match/123");
	std::cout << r1->content.rdbuf() << std::endl;

	std::string json="{\"firstName\": \"John\",\"lastName\": \"Smith\",\"age\": 25}";
	std::stringstream ss(json); 
    auto r2=client.request("POST", "/string", ss);
	std::cout << r2->content.rdbuf() << std::endl;
    
    ss.str(json);
    auto r3=client.request("POST", "/json", ss);
	std::cout << r3->content.rdbuf() << std::endl;

	/* https client example
    Client<HTTPS> clients("localhost:8080", false);
    auto r1s=client.request("GET", "/match/123");
	std::cout << r1s->content.rdbuf() << std::endl;

	std::stringstream ss2(json); 
    auto r2s=client.request("POST", "/string", ss2);
	std::cout << r2s->content.rdbuf() << std::endl;
    
    ss2.str(json);
    auto r3s=client.request("POST", "/json", ss2);
	std::cout << r3s->content.rdbuf() << std::endl;
	*/
    
    server_thread.join();
    
    return 0;
}
