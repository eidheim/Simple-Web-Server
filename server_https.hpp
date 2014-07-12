#ifndef HTTPSSERVER_HPP
#define	HTTPSSERVER_HPP

#include "server_http.hpp"
#include <boost/asio/ssl.hpp>

namespace SimpleWeb {
    typedef ssl::stream<ip::tcp::socket> HTTPS;    
    
    template<>
    class Server<HTTPS> : public ServerBase<HTTPS> {
    public:
        Server(unsigned short port, size_t num_threads, const string& cert_file, const string& private_key_file) : 
        ServerBase<HTTPS>::ServerBase(port, num_threads), context(ssl::context::sslv23) {
            context.use_certificate_chain_file(cert_file);
            context.use_private_key_file(private_key_file, ssl::context::pem);
        }

    private:
        ssl::context context;
        
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            shared_ptr<HTTPS> socket(new HTTPS(m_io_service, context));

            acceptor.async_accept((*socket).lowest_layer(), [this, socket](const boost::system::error_code& ec) {
                //Immediately start accepting a new connection
                accept();

                if(!ec) {
                    (*socket).async_handshake(ssl::stream_base::server, [this, socket](const boost::system::error_code& ec) {
                        if(!ec) {
                            process_request_and_respond(socket);
                        }
                    });
                }
            });
        }
    };
}


#endif	/* HTTPSSERVER_HPP */

