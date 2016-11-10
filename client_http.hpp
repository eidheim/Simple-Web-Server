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
            return request(*socket, request_type, path, content.size(), [content](std::ostream& out) { out.write(content.data(), content.length()); }, header, !proxy_host.empty());
        }
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path, std::iostream& content,
                                          const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            

            return request(*socket, request_type, path, content_length, [&content](std::ostream& out) { out << content.rdbuf(); }, header, !proxy_host.empty());
        }
        
    protected:
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::endpoint endpoint;
        boost::asio::ip::tcp::resolver resolver;
        
        std::shared_ptr<socket_type> socket;
        bool socket_error;
        
        std::string host;
        unsigned short port;

        std::string proxy_host;
        unsigned short proxy_port;
                
        ClientBase(const std::string& host_port, unsigned short default_port) : 
                resolver(io_service), socket_error(false) {
            set_host_port(host_port, default_port, host, port);

            endpoint=boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
        }

        ClientBase(const std::string& host_port, unsigned short default_port, const std::string& proxyhost_port) : 
                resolver(io_service), socket_error(false) {
            set_host_port(host_port, default_port, host, port);
            set_host_port(proxyhost_port, 8080, proxy_host, proxy_port);

            endpoint=boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), proxy_port);
        }
        
        virtual void connect()=0;
        virtual void proxy_connect()=0;

        virtual std::string proxy_path(const std::string& path) { return path; }
        
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

        void writeHeader(std::ostream& write_stream, const std::string& requestType, const std::string& requestPath, const std::map<std::string, std::string>& header, size_t contentLength) {
          write_stream << requestType << " " << requestPath << " HTTP/1.1\r\n";
          write_stream << "Host: " << host << "\r\n";
          for(auto& h: header) {
            write_stream << h.first << ": " << h.second << "\r\n";
          }
          if(contentLength > 0)
            write_stream << "Content-Length: " << contentLength << "\r\n";
          write_stream << "\r\n";
        }

        template<typename T>
        std::shared_ptr<Response> request(T& socket, const std::string& request_type, const std::string& path, size_t contentLength, std::function<void (std::ostream& contentStream)> contentWriter,
                                         const std::map<std::string, std::string>& header, bool useProxy) {
            std::string corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";

            if (useProxy)
              corrected_path=proxy_path(corrected_path);
            
            boost::asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            writeHeader(write_stream, request_type, corrected_path, header, contentLength);
            if (contentLength > 0)
                contentWriter(write_stream);

            try {
                if (useProxy)
                    proxy_connect();
                else
                    connect();
                
                boost::asio::write(socket, write_buffer);
            }
            catch(const std::exception& e) {
                socket_error=true;
                throw std::invalid_argument(e.what());
            }
            
            return request_read(socket);
        }

        std::shared_ptr<Response> request_read() {
          return request_read(*socket);
        }

        template<typename T>
        std::shared_ptr<Response> request_read(T& socket) {
            std::shared_ptr<Response> response(new Response());
            
            try {
                size_t bytes_transferred = boost::asio::read_until(socket, response->content_buffer, "\r\n\r\n");
                
                size_t num_additional_bytes=response->content_buffer.size()-bytes_transferred;
                
                parse_response_header(response, response->content);
                
                auto header_it=response->header.find("Content-Length");
                if(header_it!=response->header.end()) {
                    auto content_length=stoull(header_it->second);
                    if(content_length>num_additional_bytes) {
                        boost::asio::read(socket, response->content_buffer, 
                                boost::asio::transfer_exactly(content_length-num_additional_bytes));
                    }
                }
                else if((header_it=response->header.find("Transfer-Encoding"))!=response->header.end() && header_it->second=="chunked") {
                    boost::asio::streambuf streambuf;
                    std::ostream content(&streambuf);
                    
                    std::streamsize length;
                    std::string buffer;
                    do {
                        size_t bytes_transferred = boost::asio::read_until(socket, response->content_buffer, "\r\n");
                        std::string line;
                        getline(response->content, line);
                        bytes_transferred-=line.size()+1;
                        line.pop_back();
                        length=stol(line, 0, 16);
            
                        auto num_additional_bytes=static_cast<std::streamsize>(response->content_buffer.size()-bytes_transferred);
                    
                        if((2+length)>num_additional_bytes) {
                            boost::asio::read(socket, response->content_buffer, 
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

    private:
        void set_host_port(const std::string& host_port, unsigned short default_port, std::string& host_out, unsigned short& port_out) {
            size_t host_end=host_port.find(':');
            if(host_end==std::string::npos) {
                host_out=host_port;
                port_out=default_port;
            } else {
                host_out=host_port.substr(0, host_end);
                port_out=static_cast<unsigned short>(stoul(host_port.substr(host_end+1)));
            }
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

        Client(const std::string& server_port_path, const std::string& proxyhost_port) : ClientBase<HTTP>::ClientBase(server_port_path, 80, proxyhost_port) {
            socket=std::make_shared<HTTP>(io_service);
        }

    protected:
        virtual void connect() override {
            connect_to(host, port);
        }

        /** Simply establish a regular TCP connection to the proxy
         */
        virtual void proxy_connect() override {
            connect_to(proxy_host, proxy_port);
        }

        /** Common connect() implementation for proxy_connect() and connect()
         */
        void connect_to(const std::string& host, unsigned short port) {
            if(socket_error || !socket->is_open()) {
                boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
                boost::asio::connect(*socket, resolver.resolve(query));
                  
                boost::asio::ip::tcp::no_delay option(true);
                socket->set_option(option);
                  
                socket_error=false;
            }
        }

        /** HTTP proxy requests need to change the request's path
         */
        virtual std::string proxy_path(const std::string& path) override {
            return "http://" + host + ":" + std::to_string(port) + path;
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */
