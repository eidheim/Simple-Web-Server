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
            if(verify_certificate) {
                context.set_verify_mode(boost::asio::ssl::verify_peer);
                context.set_default_verify_paths();
            }
            else
                context.set_verify_mode(boost::asio::ssl::verify_none);
            
            if(cert_file.size()>0 && private_key_file.size()>0) {
                context.use_certificate_chain_file(cert_file);
                context.use_private_key_file(private_key_file, boost::asio::ssl::context::pem);
            }
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
        }

    protected:
        boost::asio::ssl::context context;
        
        void connect() {
            if(!socket || !socket->lowest_layer().is_open()) {
                boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
                
                resolver.async_resolve(query, [this]
                                       (const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator it){
                    if(!ec) {
                        socket=std::unique_ptr<HTTPS>(new HTTPS(io_service, context));
                        
                        boost::asio::async_connect(socket->lowest_layer(), it, [this]
                                                   (const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator /*it*/){
                            if(!ec) {
                                boost::asio::ip::tcp::no_delay option(true);
                                this->socket->lowest_layer().set_option(option);
                                
                                auto timer=get_timeout_timer();
                                this->socket->async_handshake(boost::asio::ssl::stream_base::client,
                                                              [this, timer](const boost::system::error_code& ec) {
                                    if(timer)
                                        timer->cancel();
                                    if(ec) {
                                        this->socket=nullptr;
                                        throw boost::system::system_error(ec);
                                    }
                                });
                            }
                            else {
                                this->socket=nullptr;
                                throw boost::system::system_error(ec);
                            }
                        });
                    }
                    else {
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
