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
                ClientBase<HTTPS>::ClientBase(server_port_path, 443), asio_context(boost::asio::ssl::context::sslv23) {
            if(verify_certificate)
                asio_context.set_verify_mode(boost::asio::ssl::verify_peer);
            else
                asio_context.set_verify_mode(boost::asio::ssl::verify_none);
            
            if(cert_file.size()>0 && private_key_file.size()>0) {
                asio_context.use_certificate_chain_file(cert_file);
                asio_context.use_private_key_file(private_key_file, boost::asio::ssl::context::pem);
            }
            
            if(verify_file.size()>0)
                asio_context.load_verify_file(verify_file);
            
            socket=std::make_shared<HTTPS>(asio_io_service, asio_context);
        }

    private:
        boost::asio::ssl::context asio_context;
        
        void connect() {
            if(socket_error || !socket->lowest_layer().is_open()) {
                boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
                boost::asio::connect(socket->lowest_layer(), asio_resolver.resolve(query));
                
                boost::asio::ip::tcp::no_delay option(true);
                socket->lowest_layer().set_option(option);
                
                socket->handshake(boost::asio::ssl::stream_base::client);
                
                socket_error=false;
            }
        }
    };
}

#endif	/* CLIENT_HTTPS_HPP */
