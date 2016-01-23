#ifndef SERVER_HTTPS_HPP
#define	SERVER_HTTPS_HPP

#include "server_http.hpp"
#include <boost/asio/ssl.hpp>

namespace SimpleWeb {
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> HTTPS;    
    
    template<>
    class Server<HTTPS> : public ServerBase<HTTPS> {
    public:
        Server(unsigned short port, size_t num_threads, const std::string& cert_file, const std::string& private_key_file,
                size_t timeout_request=5, size_t timeout_content=300, 
                const std::string& verify_file=std::string()) : 
                ServerBase<HTTPS>::ServerBase(port, num_threads, timeout_request, timeout_content), 
                context(boost::asio::ssl::context::sslv23) {
            context.use_certificate_chain_file(cert_file);
            context.use_private_key_file(private_key_file, boost::asio::ssl::context::pem);
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
        }

    private:
        boost::asio::ssl::context context;
        
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<HTTPS> socket(new HTTPS(io_service, context));

            acceptor.async_accept((*socket).lowest_layer(), [this, socket](const boost::system::error_code& ec) {
                //Immediately start accepting a new connection
                accept();
                
                if(!ec) {
                    boost::asio::ip::tcp::no_delay option(true);
                    socket->lowest_layer().set_option(option);
                    
                    //Set timeout on the following boost::asio::ssl::stream::async_handshake
                    std::shared_ptr<boost::asio::deadline_timer> timer;
                    if(timeout_request>0)
                        timer=set_timeout_on_socket(socket, timeout_request);
                    (*socket).async_handshake(boost::asio::ssl::stream_base::server, [this, socket, timer]
                            (const boost::system::error_code& ec) {
                        if(timeout_request>0)
                            timer->cancel();
                        if(!ec)
                            read_request_and_content(socket);
                    });
                }
            });
        }
    };
}


#endif	/* SERVER_HTTPS_HPP */

