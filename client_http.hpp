#ifndef CLIENT_HTTP_HPP
#define	CLIENT_HTTP_HPP

#include <unordered_map>
#include <map>
#include <random>
#include <mutex>
#include <type_traits>

#ifdef USE_STANDALONE_ASIO
#include <boost/utility/string_ref.hpp>
#include <asio.hpp>
#include <type_traits>
#include <system_error>
namespace SimpleWeb {
    using error_code = std::error_code;
    using errc = std::errc;
    using system_error = std::system_error;
    namespace make_error_code = std;
}
#else
#include <boost/asio.hpp>
#include <boost/utility/string_ref.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/functional/hash.hpp>
namespace SimpleWeb {
    namespace asio = boost::asio;
    using error_code = boost::system::error_code;
    namespace errc = boost::system::errc;
    using system_error = boost::system::system_error;
    namespace make_error_code = boost::system::errc;
}
#endif

# ifndef CASE_INSENSITIVE_EQUAL_AND_HASH
# define CASE_INSENSITIVE_EQUAL_AND_HASH
namespace SimpleWeb {
    inline bool case_insensitive_equal(const std::string &str1, const std::string &str2) {
        return str1.size() == str2.size() &&
               std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
                   return tolower(a) == tolower(b);
               });
    }
    class CaseInsensitiveEqual {
    public:
        bool operator()(const std::string &str1, const std::string &str2) const {
            return case_insensitive_equal(str1, str2);
        }
    };
    // Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
    class CaseInsensitiveHash {
    public:
        size_t operator()(const std::string &str) const {
            size_t h = 0;
            std::hash<int> hash;
            for (auto c : str)
                h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
            return h;
        }
    };
}
# endif

namespace SimpleWeb {
    template <class socket_type>
    class Client;
    
    template <class socket_type>
    class ClientBase {
    public:
        virtual ~ClientBase() {}

        class Response {
            friend class ClientBase<socket_type>;
            friend class Client<socket_type>;
        public:
            std::string http_version, status_code;

            std::istream content;

            std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> header;
            
        private:
            asio::streambuf content_buffer;
            
            Response(): content(&content_buffer) {}
        };
        
        class Config {
            friend class ClientBase<socket_type>;
        private:
            Config() {}
        public:
            /// Set timeout on requests in seconds. Default value: 0 (no timeout). 
            size_t timeout=0;
            /// Set connect timeout in seconds. Default value: 0 (Config::timeout is then used instead).
            size_t timeout_connect=0;
            /// Set proxy server (server:port)
            std::string proxy_server;
        };
        
        /// Set before calling request
        Config config;
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path="/", boost::string_ref content="",
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            auto corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            if(!config.proxy_server.empty() && std::is_same<socket_type, asio::ip::tcp::socket>::value)
                corrected_path="http://"+host+':'+std::to_string(port)+corrected_path;
            
            asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content.size()>0)
                write_stream << "Content-Length: " << content.size() << "\r\n";
            write_stream << "\r\n";
            
            connect();
            
            auto timer=get_timeout_timer();
            asio::async_write(*socket, write_buffer,
                                     [this, &content, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    if(!content.empty()) {
                        auto timer=get_timeout_timer();
                        asio::async_write(*socket, asio::buffer(content.data(), content.size()),
                                             [this, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                            if(timer)
                                timer->cancel();
                            if(ec) {
                                std::lock_guard<std::mutex> lock(socket_mutex);
                                this->socket=nullptr;
                                throw system_error(ec);
                            }
                        });
                    }
                }
                else {
                    std::lock_guard<std::mutex> lock(socket_mutex);
                    socket=nullptr;
                    throw system_error(ec);
                }
            });
            io_service->reset();
            io_service->run();
            
            return request_read();
        }
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path, std::iostream& content,
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            auto corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            if(!config.proxy_server.empty() && std::is_same<socket_type, asio::ip::tcp::socket>::value)
                corrected_path="http://"+host+':'+std::to_string(port)+corrected_path;
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            
            asio::streambuf write_buffer;
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
            
            connect();
            
            auto timer=get_timeout_timer();
            asio::async_write(*socket, write_buffer,
                                     [this, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                if(timer)
                    timer->cancel();
                if(ec) {
                    std::lock_guard<std::mutex> lock(socket_mutex);
                    socket=nullptr;
                    throw system_error(ec);
                }
            });
            io_service->reset();
            io_service->run();
            
            return request_read();
        }
        
        void close() {
            std::lock_guard<std::mutex> lock(socket_mutex);
            if(socket) {
                error_code ec;
                socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
                socket->lowest_layer().close();
            }
        }
        
    protected:
        std::shared_ptr<asio::io_service> io_service;
        asio::ip::tcp::resolver resolver;
        
        std::unique_ptr<socket_type> socket;
        std::mutex socket_mutex;
        
        std::string host;
        unsigned short port;
                
        ClientBase(const std::string& host_port, unsigned short default_port) : io_service(new asio::io_service()), resolver(*io_service) {
            auto parsed_host_port=parse_host_port(host_port, default_port);
            host=parsed_host_port.first;
            port=parsed_host_port.second;
        }
        
        std::pair<std::string, unsigned short> parse_host_port(const std::string &host_port, unsigned short default_port) {
            std::pair<std::string, unsigned short> parsed_host_port;
            size_t host_end=host_port.find(':');
            if(host_end==std::string::npos) {
                parsed_host_port.first=host_port;
                parsed_host_port.second=default_port;
            }
            else {
                parsed_host_port.first=host_port.substr(0, host_end);
                parsed_host_port.second=static_cast<unsigned short>(stoul(host_port.substr(host_end+1)));
            }
            return parsed_host_port;
        }
        
        virtual void connect()=0;
        
        std::shared_ptr<asio::deadline_timer> get_timeout_timer(size_t timeout=0) {
            if(timeout==0)
                timeout=config.timeout;
            if(timeout==0)
                return nullptr;
            
            auto timer=std::make_shared<asio::deadline_timer>(*io_service);
            timer->expires_from_now(boost::posix_time::seconds(timeout));
            timer->async_wait([this](const error_code& ec) {
                if(!ec) {
                    close();
                }
            });
            return timer;
        }
        
        void parse_response_header(const std::shared_ptr<Response> &response) const {
            std::string line;
            getline(response->content, line);
            size_t version_end=line.find(' ');
            if(version_end!=std::string::npos) {
                if(5<line.size())
                    response->http_version=line.substr(5, version_end-5);
                if((version_end+1)<line.size())
                    response->status_code=line.substr(version_end+1, line.size()-(version_end+1)-1);

                getline(response->content, line);
                size_t param_end;
                while((param_end=line.find(':'))!=std::string::npos) {
                    size_t value_start=param_end+1;
                    if((value_start)<line.size()) {
                        if(line[value_start]==' ')
                            value_start++;
                        if(value_start<line.size())
                            response->header.insert(std::make_pair(line.substr(0, param_end), line.substr(value_start, line.size()-value_start-1)));
                    }

                    getline(response->content, line);
                }
            }
        }
        
        std::shared_ptr<Response> request_read() {
            std::shared_ptr<Response> response(new Response());
            
            asio::streambuf chunked_streambuf;
            
            auto timer=get_timeout_timer();
            asio::async_read_until(*socket, response->content_buffer, "\r\n\r\n",
                                          [this, &response, &chunked_streambuf, timer](const error_code& ec, size_t bytes_transferred) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    size_t num_additional_bytes=response->content_buffer.size()-bytes_transferred;
                    
                    parse_response_header(response);
                                        
                    auto header_it=response->header.find("Content-Length");
                    if(header_it!=response->header.end()) {
                        auto content_length=stoull(header_it->second);
                        if(content_length>num_additional_bytes) {
                            auto timer=get_timeout_timer();
                            asio::async_read(*socket, response->content_buffer,
                                                    asio::transfer_exactly(content_length-num_additional_bytes),
                                                    [this, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                                if(timer)
                                    timer->cancel();
                                if(ec) {
                                    std::lock_guard<std::mutex> lock(socket_mutex);
                                    this->socket=nullptr;
                                    throw system_error(ec);
                                }
                            });
                        }
                    }
                    else if((header_it=response->header.find("Transfer-Encoding"))!=response->header.end() && header_it->second=="chunked") {
                        request_read_chunked(response, chunked_streambuf);
                    }
                    else if(response->http_version<"1.1" || ((header_it=response->header.find("Connection"))!=response->header.end() && header_it->second=="close")) {
                        auto timer=get_timeout_timer();
                        asio::async_read(*socket, response->content_buffer,
                                                [this, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                            if(timer)
                                    timer->cancel();
                            if(ec) {
                                std::lock_guard<std::mutex> lock(socket_mutex);
                                this->socket=nullptr;
                                if(ec!=asio::error::eof)
                                    throw system_error(ec);
                            }
                        });
                    }
                }
                else {
                    std::lock_guard<std::mutex> lock(socket_mutex);
                    socket=nullptr;
                    throw system_error(ec);
                }
            });
            io_service->reset();
            io_service->run();
            
            return response;
        }
        
        void request_read_chunked(const std::shared_ptr<Response> &response, asio::streambuf &streambuf) {
            auto timer=get_timeout_timer();
            asio::async_read_until(*socket, response->content_buffer, "\r\n",
                                      [this, &response, &streambuf, timer](const error_code& ec, size_t bytes_transferred) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    std::string line;
                    getline(response->content, line);
                    bytes_transferred-=line.size()+1;
                    line.pop_back();
                    std::streamsize length=stol(line, 0, 16);
                    
                    auto num_additional_bytes=static_cast<std::streamsize>(response->content_buffer.size()-bytes_transferred);
                    
                    auto post_process=[this, &response, &streambuf, length] {
                        std::ostream stream(&streambuf);
                        if(length>0) {
                            std::vector<char> buffer(static_cast<size_t>(length));
                            response->content.read(&buffer[0], length);
                            stream.write(&buffer[0], length);
                        }
                        
                        //Remove "\r\n"
                        response->content.get();
                        response->content.get();
                        
                        if(length>0)
                            request_read_chunked(response, streambuf);
                        else {
                            std::ostream response_stream(&response->content_buffer);
                            response_stream << stream.rdbuf();
                        }
                    };
                    
                    if((2+length)>num_additional_bytes) {
                        auto timer=get_timeout_timer();
                        asio::async_read(*socket, response->content_buffer,
                                                asio::transfer_exactly(2+length-num_additional_bytes),
                                                [this, post_process, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                            if(timer)
                                timer->cancel();
                            if(!ec) {
                                post_process();
                            }
                            else {
                                std::lock_guard<std::mutex> lock(socket_mutex);
                                this->socket=nullptr;
                                throw system_error(ec);
                            }
                        });
                    }
                    else
                        post_process();
                }
                else {
                    std::lock_guard<std::mutex> lock(socket_mutex);
                    socket=nullptr;
                    throw system_error(ec);
                }
            });
        }
    };
    
    template<class socket_type>
    class Client : public ClientBase<socket_type> {};
    
    typedef asio::ip::tcp::socket HTTP;
    
    template<>
    class Client<HTTP> : public ClientBase<HTTP> {
    public:
        Client(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}
        
    protected:
        void connect() {
            if(!socket || !socket->is_open()) {
                std::unique_ptr<asio::ip::tcp::resolver::query> query;
                if(config.proxy_server.empty())
                    query=std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(host, std::to_string(port)));
                else {
                    auto proxy_host_port=parse_host_port(config.proxy_server, 8080);
                    query=std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(proxy_host_port.first, std::to_string(proxy_host_port.second)));
                }
                resolver.async_resolve(*query, [this](const error_code &ec,
                                                     asio::ip::tcp::resolver::iterator it){
                    if(!ec) {
                        {
                            std::lock_guard<std::mutex> lock(socket_mutex);
                            socket=std::unique_ptr<HTTP>(new HTTP(*io_service));
                        }
                        
                        auto timer=get_timeout_timer(config.timeout_connect);
                        asio::async_connect(*socket, it, [this, timer]
                                (const error_code &ec, asio::ip::tcp::resolver::iterator /*it*/){
                            if(timer)
                                timer->cancel();
                            if(!ec) {
                                asio::ip::tcp::no_delay option(true);
                                this->socket->set_option(option);
                            }
                            else {
                                std::lock_guard<std::mutex> lock(socket_mutex);
                                this->socket=nullptr;
                                throw system_error(ec);
                            }
                        });
                    }
                    else {
                        std::lock_guard<std::mutex> lock(socket_mutex);
                        socket=nullptr;
                        throw system_error(ec);
                    }
                });
                io_service->reset();
                io_service->run();
            }
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */
