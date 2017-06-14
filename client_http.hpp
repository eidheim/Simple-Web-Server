#ifndef CLIENT_HTTP_HPP
#define	CLIENT_HTTP_HPP

#include <unordered_map>
#include <vector>
#include <random>
#include <mutex>
#include <type_traits>

#include <boost/utility/string_ref.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#ifdef USE_STANDALONE_ASIO
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
    using Header = std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;
    
    template <class socket_type>
    class Client;
    
    template <class socket_type>
    class ClientBase {
    public:
        class Response {
            friend class ClientBase<socket_type>;
            friend class Client<socket_type>;
        public:
            std::string http_version, status_code;

            std::istream content;

            Header header;
            
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
        
    protected:
        class RequestCallback {
        public:
            bool stop=false;
            std::mutex stop_mutex;
        };
        
        class Connection {
        public:
            Connection(const std::string &host, unsigned short port, const Config &config, std::unique_ptr<socket_type> &&socket) :
                    host(host), port(port), config(config), socket(std::move(socket)) {
                if(config.proxy_server.empty())
                    query=std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(host, std::to_string(port)));
                else {
                    auto proxy_host_port=parse_host_port(config.proxy_server, 8080);
                    query=std::unique_ptr<asio::ip::tcp::resolver::query>(new asio::ip::tcp::resolver::query(proxy_host_port.first, std::to_string(proxy_host_port.second)));
                }
            }
            std::string host;
            unsigned short port;
            Config config;
            
            std::unique_ptr<socket_type> socket;
            bool in_use=false;
            bool reconnecting=false;
            
            std::unique_ptr<asio::ip::tcp::resolver::query> query;
        };
        
        class Session {
        public:
            Session(const std::shared_ptr<asio::io_service> &io_service, const std::shared_ptr<Connection> &connection, std::unique_ptr<asio::streambuf> &&request_buffer) :
                    io_service(io_service), connection(connection), request_buffer(std::move(request_buffer)), response(new Response()) {}
            std::shared_ptr<asio::io_service> io_service;
            std::shared_ptr<Connection> connection;
            std::unique_ptr<asio::streambuf> request_buffer;
            std::shared_ptr<Response> response;
            std::function<void(const error_code&)> callback;
        };
        
    public:
        /// Set before calling request
        Config config;
        
        /// If you have your own asio::io_service, store its pointer here before calling request().
        /// When using asynchronous requests, running the io_service is up to the programmer.
        std::shared_ptr<asio::io_service> io_service;
        
        virtual ~ClientBase() {
            boost::unique_lock<boost::shared_mutex> lock(*stop_request_callback_mutex);
            *stop_request_callback=true;
        }
        
        /// Synchronous request. The io_service is run within this function.
        std::shared_ptr<Response> request(const std::string& method, const std::string& path=std::string("/"), boost::string_ref content="", const Header& header=Header()) {
            auto session=std::make_shared<Session>(io_service, get_connection(), create_request_header(method, path, header));
            std::shared_ptr<Response> response;
            session->callback=[this, &response, session](const error_code &ec) {
                {
                    std::lock_guard<std::mutex> lock(*connections_mutex);
                    session->connection->in_use=false;
                }
                response=session->response;
                if(ec)
                    throw system_error(ec);
            };
            
            std::ostream write_stream(session->request_buffer.get());
            if(content.size()>0)
                write_stream << "Content-Length: " << content.size() << "\r\n";
            write_stream << "\r\n" << content;
            
            Client<socket_type>::connect(session);
            
            io_service->reset();
            io_service->run();
            
            return response;
        }
        
        /// Synchronous request. The io_service is run within this function.
        std::shared_ptr<Response> request(const std::string& method, const std::string& path, std::iostream& content, const Header& header=Header()) {
            auto session=std::make_shared<Session>(io_service, get_connection(), create_request_header(method, path, header));
            std::shared_ptr<Response> response;
            session->callback=[this, &response, session](const error_code &ec) {
                {
                    std::lock_guard<std::mutex> lock(*connections_mutex);
                    session->connection->in_use=false;
                }
                response=session->response;
                if(ec)
                    throw system_error(ec);
            };
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            std::ostream write_stream(session->request_buffer.get());
            if(content_length>0)
                write_stream << "Content-Length: " << content_length << "\r\n";
            write_stream << "\r\n";
            if(content_length>0)
                write_stream << content.rdbuf();
            
            Client<socket_type>::connect(session);
            
            io_service->reset();
            io_service->run();
            
            return response;
        }
        
        /// Asynchronous request where setting and/or running Client's io_service is required.
        /// The request callback is not run if the calling Client object has been destroyed.
        void request(const std::string &method, const std::string &path, boost::string_ref content, const Header& header,
                     std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback_) {
            auto session=std::make_shared<Session>(io_service, get_connection(), create_request_header(method, path, header));
            auto request_callback=std::make_shared<std::function<void(std::shared_ptr<Response>, const error_code&)>>(std::move(request_callback_));
            auto connections_mutex=this->connections_mutex;
            auto stop_request_callback=this->stop_request_callback;
            auto stop_request_callback_mutex=this->stop_request_callback_mutex;
            session->callback=[session, request_callback, connections_mutex, stop_request_callback, stop_request_callback_mutex](const error_code &ec) {
                {
                    std::lock_guard<std::mutex> lock(*connections_mutex);
                    session->connection->in_use=false;
                }
                
                boost::shared_lock<boost::shared_mutex> lock(*stop_request_callback_mutex);
                if(*stop_request_callback)
                    return;
                
                if(*request_callback)
                    (*request_callback)(session->response, ec);
            };
            
            std::ostream write_stream(session->request_buffer.get());
            if(content.size()>0)
                write_stream << "Content-Length: " << content.size() << "\r\n";
            write_stream << "\r\n" << content;
            
            Client<socket_type>::connect(session);
        }
        
        /// Asynchronous request where setting and/or running Client's io_service is required.
        /// The request callback is not run if the calling Client object has been destroyed.
        void request(const std::string &method, const std::string &path, boost::string_ref content,
                     std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback) {
            request(method, path, content, Header(), std::move(request_callback));
        } 
        
        /// Asynchronous request where setting and/or running Client's io_service is required.
        /// The request callback is not run if the calling Client object has been destroyed.
        void request(const std::string &method, const std::string &path,
                     std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback) {
            request(method, path, std::string(), Header(), std::move(request_callback));
        }
        
        /// Asynchronous request where setting and/or running Client's io_service is required.
        /// The request callback is not run if the calling Client object has been destroyed.
        void request(const std::string &method, std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback) {
            request(method, std::string("/"), std::string(), Header(), std::move(request_callback));
        }
        
        /// Asynchronous request where setting and/or running Client's io_service is required.
        /// The request callback is not run if the calling Client object has been destroyed.
        void request(const std::string &method, const std::string &path, std::iostream& content, const Header& header,
                     std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback_) {
            auto session=std::make_shared<Session>(io_service, get_connection(), create_request_header(method, path, header));
            auto request_callback=std::make_shared<std::function<void(std::shared_ptr<Response>, const error_code&)>>(std::move(request_callback_));
            auto connections_mutex=this->connections_mutex;
            auto stop_request_callback=this->stop_request_callback;
            auto stop_request_callback_mutex=this->stop_request_callback_mutex;
            session->callback=[session, request_callback, connections_mutex, stop_request_callback, stop_request_callback_mutex](const error_code &ec) {
                {
                    std::lock_guard<std::mutex> lock(*connections_mutex);
                    session->connection->in_use=false;
                }
                
                boost::shared_lock<boost::shared_mutex> lock(*stop_request_callback_mutex);
                if(*stop_request_callback)
                    return;
                
                if(*request_callback)
                    (*request_callback)(session->response, ec);
            };
            
            content.seekp(0, std::ios::end);
            auto content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            std::ostream write_stream(session->request_buffer.get());
            if(content_length>0)
                write_stream << "Content-Length: " << content_length << "\r\n";
            write_stream << "\r\n";
            if(content_length>0)
                write_stream << content.rdbuf();
            
            Client<socket_type>::connect(session);
        }
        
        void request(const std::string &method, const std::string &path, std::iostream& content,
                     std::function<void(std::shared_ptr<Response>, const error_code&)> &&request_callback) {
            request(method, path, content, Header(), std::move(request_callback));
        }
        
    protected:
        std::string host;
        unsigned short port;
        
        std::vector<std::shared_ptr<Connection>> connections;
        std::shared_ptr<std::mutex> connections_mutex;
        
        std::shared_ptr<bool> stop_request_callback;
        std::shared_ptr<boost::shared_mutex> stop_request_callback_mutex;
        
        ClientBase(const std::string& host_port, unsigned short default_port) : io_service(new asio::io_service()),
                connections_mutex(new std::mutex()), stop_request_callback(new bool(false)), stop_request_callback_mutex(new boost::shared_mutex()) {
            auto parsed_host_port=parse_host_port(host_port, default_port);
            host=parsed_host_port.first;
            port=parsed_host_port.second;
        }
        
        std::shared_ptr<Connection> get_connection() {
            std::shared_ptr<Connection> connection;
            std::lock_guard<std::mutex> lock(*connections_mutex);
            for(auto it=connections.begin();it!=connections.end();) {
                if((*it)->in_use)
                    ++it;
                else if(!connection) {
                    connection=*it;
                    ++it;
                }
                else
                    it=connections.erase(it);
            }
            if(!connection) {
                connection=create_connection();
                connections.emplace_back(connection);
            }
            connection->reconnecting=false;
            connection->in_use=true;
            return connection;
        }
        
        virtual std::shared_ptr<Connection> create_connection()=0;
        
        std::unique_ptr<asio::streambuf> create_request_header(const std::string& method, const std::string& path, const Header& header) const {
            auto corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            if(!config.proxy_server.empty() && std::is_same<socket_type, asio::ip::tcp::socket>::value)
                corrected_path="http://"+host+':'+std::to_string(port)+corrected_path;
            
            std::unique_ptr<asio::streambuf> request_buffer(new asio::streambuf());
            std::ostream write_stream(request_buffer.get());
            write_stream << method << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header)
                write_stream << h.first << ": " << h.second << "\r\n";
            return request_buffer;
        }
        
        static std::pair<std::string, unsigned short> parse_host_port(const std::string &host_port, unsigned short default_port) {
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
        
        static std::shared_ptr<asio::deadline_timer> get_timeout_timer(const std::shared_ptr<Session> &session, size_t timeout=0) {
            if(timeout==0)
                timeout=session->connection->config.timeout;
            if(timeout==0)
                return nullptr;
            
            auto timer=std::make_shared<asio::deadline_timer>(*session->io_service);
            timer->expires_from_now(boost::posix_time::seconds(timeout));
            timer->async_wait([session](const error_code& ec) {
                if(!ec)
                    close(session);
            });
            return timer;
        }
        
        static void parse_response_header(const std::shared_ptr<Response> &response) {
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
        
        static void write(const std::shared_ptr<Session> &session) {
            auto timer=get_timeout_timer(session);
            asio::async_write(*session->connection->socket, session->request_buffer->data(), [session, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                if(timer)
                    timer->cancel();
                if(!ec)
                    read(session);
                else {
                    close(session);
                    session->callback(ec);
                }
            });
        }
        
        static void read(const std::shared_ptr<Session> &session) {
            auto timer=get_timeout_timer(session);
            asio::async_read_until(*session->connection->socket, session->response->content_buffer, "\r\n\r\n",
                                   [session, timer](const error_code& ec, size_t bytes_transferred) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    session->connection->reconnecting=false;
                    
                    size_t num_additional_bytes=session->response->content_buffer.size()-bytes_transferred;
                    
                    parse_response_header(session->response);
                    
                    auto header_it=session->response->header.find("Content-Length");
                    if(header_it!=session->response->header.end()) {
                        auto content_length=stoull(header_it->second);
                        if(content_length>num_additional_bytes) {
                            auto timer=get_timeout_timer(session);
                            asio::async_read(*session->connection->socket, session->response->content_buffer, asio::transfer_exactly(content_length-num_additional_bytes),
                                             [session, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                                if(timer)
                                    timer->cancel();
                                if(!ec)
                                    session->callback(ec);
                                else {
                                    close(session);
                                    session->callback(ec);
                                }
                            });
                        }
                        else
                            session->callback(ec);
                    }
                    else if((header_it=session->response->header.find("Transfer-Encoding"))!=session->response->header.end() && header_it->second=="chunked") {
                        auto tmp_streambuf=std::make_shared<asio::streambuf>();
                        read_chunked(session, tmp_streambuf);
                    }
                    else if(session->response->http_version<"1.1" || ((header_it=session->response->header.find("Session"))!=session->response->header.end() && header_it->second=="close")) {
                        auto timer=get_timeout_timer(session);
                        asio::async_read(*session->connection->socket, session->response->content_buffer, [session, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                            if(timer)
                                timer->cancel();
                            if(!ec)
                                session->callback(ec);
                            else {
                                close(session);
                                if(ec==asio::error::eof) {
                                    error_code ec;
                                    session->callback(ec);
                                }
                                else
                                    session->callback(ec);
                            }
                        });
                    }
                    else 
                        session->callback(ec);
                }
                else {
                    if(!session->connection->reconnecting) {
                        session->connection->reconnecting=true;
                        close(session);
                        Client<socket_type>::connect(session);
                    }
                    else {
                        close(session);
                        session->callback(ec);
                    }
                }
            });
        }
        
        static void read_chunked(const std::shared_ptr<Session> &session, const std::shared_ptr<asio::streambuf> &tmp_streambuf) {
            auto timer=get_timeout_timer(session);
            asio::async_read_until(*session->connection->socket, session->response->content_buffer, "\r\n", [session, tmp_streambuf, timer](const error_code& ec, size_t bytes_transferred) {
                if(timer)
                    timer->cancel();
                if(!ec) {
                    std::string line;
                    getline(session->response->content, line);
                    bytes_transferred-=line.size()+1;
                    line.pop_back();
                    std::streamsize length=stol(line, 0, 16);
                    
                    auto num_additional_bytes=static_cast<std::streamsize>(session->response->content_buffer.size()-bytes_transferred);
                    
                    auto post_process=[session, tmp_streambuf, length] {
                        std::ostream tmp_stream(tmp_streambuf.get());
                        if(length>0) {
                            std::vector<char> buffer(static_cast<size_t>(length));
                            session->response->content.read(&buffer[0], length);
                            tmp_stream.write(&buffer[0], length);
                        }
                        
                        //Remove "\r\n"
                        session->response->content.get();
                        session->response->content.get();
                        
                        if(length>0)
                            read_chunked(session, tmp_streambuf);
                        else {
                            std::ostream response_stream(&session->response->content_buffer);
                            response_stream << tmp_stream.rdbuf();
                            error_code ec;
                            session->callback(ec);
                        }
                    };
                    
                    if((2+length)>num_additional_bytes) {
                        auto timer=get_timeout_timer(session);
                        asio::async_read(*session->connection->socket, session->response->content_buffer, asio::transfer_exactly(2+length-num_additional_bytes),
                                         [session, post_process, timer](const error_code& ec, size_t /*bytes_transferred*/) {
                            if(timer)
                                timer->cancel();
                            if(!ec)
                                post_process();
                            else {
                                close(session);
                                session->callback(ec);
                            }
                        });
                    }
                    else
                        post_process();
                }
                else {
                    close(session);
                    session->callback(ec);
                }
            });
        }
        
        static void close(const std::shared_ptr<Session> &session) {
            error_code ec;
            session->connection->socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            session->connection->socket->lowest_layer().close(ec);
        }
    };
    
    template<class socket_type>
    class Client : public ClientBase<socket_type> {};
    
    typedef asio::ip::tcp::socket HTTP;
    
    template<>
    class Client<HTTP> : public ClientBase<HTTP> {
    public:
        friend ClientBase<HTTP>;
        
        Client(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {}
        
    protected:
        std::shared_ptr<Connection> create_connection() override {
            return std::make_shared<Connection>(host, port, config, std::unique_ptr<HTTP>(new HTTP(*io_service)));
        }
        
        static void connect(const std::shared_ptr<Session> &session) {
            if(!session->connection->socket->lowest_layer().is_open()) {
                auto resolver=std::make_shared<asio::ip::tcp::resolver>(*session->io_service);
                auto timer=get_timeout_timer(session, session->connection->config.timeout_connect);
                resolver->async_resolve(*session->connection->query, [session, timer, resolver](const error_code &ec, asio::ip::tcp::resolver::iterator it){
                    if(timer)
                        timer->cancel();
                    if(!ec) {
                        auto timer=get_timeout_timer(session, session->connection->config.timeout_connect);
                        asio::async_connect(*session->connection->socket, it, [session, timer, resolver](const error_code &ec, asio::ip::tcp::resolver::iterator /*it*/){
                            if(timer)
                                timer->cancel();
                            if(!ec) {
                                asio::ip::tcp::no_delay option(true);
                                session->connection->socket->set_option(option);
                                write(session);
                            }
                            else {
                                close(session);
                                session->callback(ec);
                            }
                        });
                    }
                    else {
                        close(session);
                        session->callback(ec);
                    }
                });
            }
            else
                write(session);
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */
