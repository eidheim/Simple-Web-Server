#ifndef CLIENT_HTTPS_HPP
#define	CLIENT_HTTPS_HPP

#include "client_http.hpp"
#include <boost/asio/ssl.hpp>

namespace SimpleWeb {
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> HTTPS;
    
    template<>
    class Client<HTTPS> : public ClientBase<HTTPS> {
    public:
        Client(const std::string& server_port_path, bool verify_certificate=true, 
                const std::string& cert_file=std::string(), const std::string& private_key_file=std::string(), 
                const std::string& verify_file=std::string()) : 
                ClientBase<HTTPS>::ClientBase(server_port_path, 443), context(boost::asio::ssl::context::tlsv12) {
            if(cert_file.size()>0 && private_key_file.size()>0) {
                context.use_certificate_chain_file(cert_file);
                context.use_private_key_file(private_key_file, boost::asio::ssl::context::pem);
            }
            
            if(verify_certificate)
                context.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
            else
                context.set_default_verify_paths();
            
            if(verify_file.size()>0 || verify_certificate)
                context.set_verify_mode(boost::asio::ssl::verify_peer);
            else
                context.set_verify_mode(boost::asio::ssl::verify_none);
        }

    protected:
        boost::asio::ssl::context context;
        
        void connect() {
            if(!socket || !socket->lowest_layer().is_open()) {
                std::unique_ptr<boost::asio::ip::tcp::resolver::query> query;
                if(config.proxy_server.empty())
                    query=std::unique_ptr<boost::asio::ip::tcp::resolver::query>(new boost::asio::ip::tcp::resolver::query(host, std::to_string(port)));
                else {
                    auto proxy_host_port=parse_host_port(config.proxy_server, 8080);
                    query=std::unique_ptr<boost::asio::ip::tcp::resolver::query>(new boost::asio::ip::tcp::resolver::query(proxy_host_port.first, std::to_string(proxy_host_port.second)));
                }
                resolver.async_resolve(*query, [this]
                                       (const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator it){
                    if(!ec) {
                        {
                            std::lock_guard<std::mutex> lock(socket_mutex);
                            socket=std::unique_ptr<HTTPS>(new HTTPS(io_service, context));
                        }
                        
                        auto timer=get_timeout_timer(config.timeout_connect);
                        boost::asio::async_connect(socket->lowest_layer(), it, [this, timer]
                                                   (const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator /*it*/){
                            if(timer)
                                timer->cancel();
                            if(!ec) {
                                boost::asio::ip::tcp::no_delay option(true);
                                this->socket->lowest_layer().set_option(option);
                            }
                            else {
                                std::lock_guard<std::mutex> lock(socket_mutex);
                                this->socket=nullptr;
                                throw boost::system::system_error(ec);
                            }
                        });
                    }
                    else {
                        std::lock_guard<std::mutex> lock(socket_mutex);
                        socket=nullptr;
                        throw boost::system::system_error(ec);
                    }
                });
                io_service.reset();
                io_service.run();
                
                if(!config.proxy_server.empty()) {
                    boost::asio::streambuf write_buffer;
                    std::ostream write_stream(&write_buffer);
                    auto host_port=host+':'+std::to_string(port);
                    write_stream << "CONNECT "+host_port+" HTTP/1.1\r\n" << "Host: " << host_port << "\r\n\r\n";
                    auto timer=get_timeout_timer();
                    boost::asio::async_write(socket->next_layer(), write_buffer,
                                             [this, timer](const boost::system::error_code &ec, size_t /*bytes_transferred*/) {
                        if(timer)
                            timer->cancel();
                        if(ec) {
                            std::lock_guard<std::mutex> lock(socket_mutex);
                            socket=nullptr;
                            throw boost::system::system_error(ec);
                        }
                    });
                    io_service.reset();
                    io_service.run();
                    
                    std::shared_ptr<Response> response(new Response());
                    timer=get_timeout_timer();
                    boost::asio::async_read_until(socket->next_layer(), response->content_buffer, "\r\n\r\n",
                                                  [this, timer](const boost::system::error_code& ec, size_t /*bytes_transferred*/) {
                        if(timer)
                            timer->cancel();
                        if(ec) {
                            std::lock_guard<std::mutex> lock(socket_mutex);
                            socket=nullptr;
                            throw boost::system::system_error(ec);
                        }
                    });
                    io_service.reset();
                    io_service.run();
                    parse_response_header(response);
                    if (response->status_code.empty() || response->status_code.compare(0, 3, "200") != 0) {
                        std::lock_guard<std::mutex> lock(socket_mutex);
                        socket=nullptr;
                        throw boost::system::system_error(boost::system::error_code(boost::system::errc::permission_denied, boost::system::generic_category()));
                    }
                }
                
                auto timer=get_timeout_timer();
                this->socket->async_handshake(boost::asio::ssl::stream_base::client,
                                              [this, timer](const boost::system::error_code& ec) {
                    if(timer)
                        timer->cancel();
                    if(ec) {
                        std::lock_guard<std::mutex> lock(socket_mutex);
                        socket=nullptr;
                        throw boost::system::system_error(ec);
                    }
                });
                io_service.reset();
                io_service.run();
            }
        }
    };
}

#endif	/* CLIENT_HTTPS_HPP */
