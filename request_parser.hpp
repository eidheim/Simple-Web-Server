#ifndef REQUEST_PARSER
#define REQUEST_PARSER
#include <regex>
#include <memory>

#include "request.hpp"

namespace SimpleWeb {
class Request;

inline void parser_regex(std::shared_ptr<Request> request, std::istream& stream) {
    std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

    std::smatch sm;

    //First parse request method, path, and HTTP-version from the first line
    std::string line;
    getline(stream, line);
    line.pop_back();
    if(std::regex_match(line, sm, e)) {        
        request->method=sm[1];
        request->path=sm[2];
        request->http_version=sm[3];

        bool matched;
        e="^([^:]*): ?(.*)$";
        //Parse the rest of the header
        do {
            getline(stream, line);
            line.pop_back();
            matched=std::regex_match(line, sm, e);
            if(matched) {
                request->header[sm[1]]=sm[2];
            }

        } while(matched==true);
    }
}

inline void parser_handwritten(std::shared_ptr<Request> request, std::istream& stream) {
    std::string line;
    getline(stream, line);
    line.pop_back();

	size_t first = line.find(' ');
	if(first == std::string::npos) {
		return;
	}

	size_t second = line.find(" HTTP/", first+1);
	if(second == std::string::npos) {
		return;
	}

	request->method = line.substr(0, first);
	request->path = line.substr(first+1, second-first-1);
	request->http_version = line.substr(second+6);

    bool matched;
    //e="^([^:]*): ?(.*)$";
    //Parse the rest of the header
    do {
        getline(stream, line);
        line.pop_back();
		first = line.find(':');
		if(first == std::string::npos) {
			return;
		}

		if(first + 1 >= line.size()) {
			return;
		}

		matched = true;

        if(matched) {
            request->header[line.substr(0, first)] = line.substr(first+1);
        }

    } while(matched);
}
}
#endif
