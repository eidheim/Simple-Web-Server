#ifndef REQUEST
#define REQUEST

#include <boost/asio.hpp>
#include <unordered_map>
namespace SimpleWeb {

class Request {
public:
    std::string method, path, http_version;
    std::istream content;
    std::unordered_map<std::string, std::string> header;
    std::smatch path_match;

    Request(): content(&content_buffer) {}
    
    boost::asio::streambuf content_buffer;
};
}
#endif
