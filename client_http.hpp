#ifndef CLIENT_HTTP_HPP
#define	CLIENT_HTTP_HPP

#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>

#include <unordered_map>
#include <map>
#include <random>

namespace SimpleWeb {
    template <class socket_type>
    class ClientBase {
    public:
        virtual ~ClientBase() {}

        class Response {
            friend class ClientBase<socket_type>;
            
            //Based on http://www.boost.org/doc/libs/1_60_0/doc/html/unordered/hash_equality.html
            class iequal_to {
            public:
              bool operator()(const std::string &key1, const std::string &key2) const {
                return boost::algorithm::iequals(key1, key2);
              }
            };
            class ihash {
            public:
              size_t operator()(const std::string &key) const {
                std::size_t seed=0;
                for(auto &c: key)
                  boost::hash_combine(seed, std::tolower(c));
                return seed;
              }
            };
        public:
            std::string http_version, status_code;

            std::istream content;

            std::unordered_multimap<std::string, std::string, ihash, iequal_to> header;
            
        private:
            boost::asio::streambuf content_buffer;
            
            Response(): content(&content_buffer) {}
        };
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path="/", boost::string_ref content="",
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            std::string corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            
            boost::asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content.size()>0)
                write_stream << "Content-Length: " << content.size() << "\r\n";
            write_stream << "\r\n";
            
            try {
                connect();
                
                boost::asio::write(*socket, write_buffer);
                if(content.size()>0)
                    boost::asio::write(*socket, boost::asio::buffer(content.data(), content.size()));
                
            }
            catch(const std::exception& e) {
                socket_error=true;
                throw std::invalid_argument(e.what());
            }
            
            return request_read();
        }
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path, std::iostream& content,
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            std::string corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            
            boost::asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content_length>0)
                write_stream << "Content-Length: " << content_length << "\r\n";
            write_stream << "\r\n";
            if(content_length>0)
                write_stream << content.rdbuf();
            
            try {
                connect();
                
                boost::asio::write(*socket, write_buffer);
            }
            catch(const std::exception& e) {
                socket_error=true;
                throw std::invalid_argument(e.what());
            }
            
            return request_read();
        }
        
    protected:
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::endpoint endpoint;
        boost::asio::ip::tcp::resolver resolver;
        
        std::shared_ptr<socket_type> socket;
        bool socket_error;
        
        std::string host;
        unsigned short port;
                
        ClientBase(const std::string& host_port, unsigned short default_port) : 
                resolver(io_service), socket_error(false) {
            size_t host_end=host_port.find(':');
            if(host_end==std::string::npos) {
                host=host_port;
                port=default_port;
            }
            else {
                host=host_port.substr(0, host_end);
                port=static_cast<unsigned short>(stoul(host_port.substr(host_end+1)));
            }

            endpoint=boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
        }
        
        virtual void connect()=0;
        
        void parse_response_header(const std::shared_ptr<Response> &response, std::istream& stream) const {
            std::string line;
            getline(stream, line);
            size_t version_end=line.find(' ');
            if(version_end!=std::string::npos) {
                if(5<line.size())
                    response->http_version=line.substr(5, version_end-5);
                if((version_end+1)<line.size())
                    response->status_code=line.substr(version_end+1, line.size()-(version_end+1)-1);

                getline(stream, line);
                size_t param_end;
                while((param_end=line.find(':'))!=std::string::npos) {
                    size_t value_start=param_end+1;
                    if((value_start)<line.size()) {
                        if(line[value_start]==' ')
                            value_start++;
                        if(value_start<line.size())
                            response->header.insert(std::make_pair(line.substr(0, param_end), line.substr(value_start, line.size()-value_start-1)));
                    }

                    getline(stream, line);
                }
            }
        }
        
        std::shared_ptr<Response> request_read() {
            std::shared_ptr<Response> response(new Response());
            
            try {
                size_t bytes_transferred = boost::asio::read_until(*socket, response->content_buffer, "\r\n\r\n");
                
                size_t num_additional_bytes=response->content_buffer.size()-bytes_transferred;
                
                parse_response_header(response, response->content);
                
                auto header_it=response->header.find("Content-Length");
                if(header_it!=response->header.end()) {
                    auto content_length=stoull(header_it->second);
                    if(content_length>num_additional_bytes) {
                        boost::asio::read(*socket, response->content_buffer, 
                                boost::asio::transfer_exactly(content_length-num_additional_bytes));
                    }
                }
                else if((header_it=response->header.find("Transfer-Encoding"))!=response->header.end() && header_it->second=="chunked") {
                    boost::asio::streambuf streambuf;
                    std::ostream content(&streambuf);
                    
                    std::streamsize length;
                    std::string buffer;
                    do {
                        size_t bytes_transferred = boost::asio::read_until(*socket, response->content_buffer, "\r\n");
                        std::string line;
                        getline(response->content, line);
                        bytes_transferred-=line.size()+1;
                        line.pop_back();
                        length=stol(line, 0, 16);
            
                        auto num_additional_bytes=static_cast<std::streamsize>(response->content_buffer.size()-bytes_transferred);
                    
                        if((2+length)>num_additional_bytes) {
                            boost::asio::read(*socket, response->content_buffer, 
                                boost::asio::transfer_exactly(2+length-num_additional_bytes));
                        }

                        buffer.resize(static_cast<size_t>(length));
                        response->content.read(&buffer[0], length);
                        content.write(&buffer[0], length);
            
                        //Remove "\r\n"
                        response->content.get();
                        response->content.get();
                    } while(length>0);
                    
                    std::ostream response_content_output_stream(&response->content_buffer);
                    response_content_output_stream << content.rdbuf();
                }
            }
            catch(const std::exception& e) {
                socket_error=true;
                throw std::invalid_argument(e.what());
            }
            
            return response;
        }
    };
    
    template<class socket_type>
    class Client : public ClientBase<socket_type> {};
    
    typedef boost::asio::ip::tcp::socket HTTP;
    
    template<>
    class Client<HTTP> : public ClientBase<HTTP> {
    public:
        Client(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {
            socket=std::make_shared<HTTP>(io_service);
        }
        
    protected:
        void connect() {
            if(socket_error || !socket->is_open()) {
                boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
                boost::asio::connect(*socket, resolver.resolve(query));
                
                boost::asio::ip::tcp::no_delay option(true);
                socket->set_option(option);
                
                socket_error=false;
            }
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */
