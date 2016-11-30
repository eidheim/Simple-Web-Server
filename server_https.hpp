#ifndef SERVER_HTTPS_HPP
#define	SERVER_HTTPS_HPP

#include "server_http.hpp"
#include <boost/asio/ssl.hpp>
#include <boost/lexical_cast.hpp>
#include <openssl/ssl.h>


namespace SimpleWeb {
    typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> HTTPS;    
    static const unsigned char ssl_session_ctx_id = 1;

    template<>
    class Server<HTTPS> : public ServerBase<HTTPS> {
    public:
        Server(unsigned short port, size_t num_threads, const std::string& cert_file, const std::string& private_key_file,
                long timeout_request=5, long timeout_content=300,
                const std::string& verify_file=std::string(), bool verify_certificate=false) : 
                ServerBase<HTTPS>::ServerBase(port, num_threads, timeout_request, timeout_content), 
                context(boost::asio::ssl::context::tlsv12) { // 2016/08/13 only use tls12, see https://www.ssllabs.com/ssltest
            context.use_certificate_chain_file(cert_file);
            context.use_private_key_file(private_key_file, boost::asio::ssl::context::pem);
            
            if(verify_file.size()>0)
                context.load_verify_file(verify_file);
            if(verify_certificate) {
		SSL_CTX_set_session_id_context(context.native_handle(), &ssl_session_ctx_id, sizeof(ssl_session_ctx_id));
                context.set_verify_mode(boost::asio::ssl::verify_peer|boost::asio::ssl::verify_fail_if_no_peer_cert);
	    }
        }

    protected:
        boost::asio::ssl::context context;
        
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            auto socket=std::make_shared<HTTPS>(*io_service, context);

            acceptor->async_accept((*socket).lowest_layer(), [this, socket](const boost::system::error_code& ec) {
                //Immediately start accepting a new connection (if io_service hasn't been stopped)
                if (ec != boost::asio::error::operation_aborted)
                    accept();

                
                if(!ec) {
                    boost::asio::ip::tcp::no_delay option(true);
                    socket->lowest_layer().set_option(option);
                    
                    //Set timeout on the following boost::asio::ssl::stream::async_handshake
                    auto timer=get_timeout_timer(socket, timeout_request);
                    (*socket).async_handshake(boost::asio::ssl::stream_base::server, [this, socket, timer]
                            (const boost::system::error_code& ec) {
                        if(timer)
                            timer->cancel();
                        if(!ec)
                            read_request_and_content(socket);
			else {
				std::string err = ec.message();
				if (ec.category() == boost::asio::error::get_ssl_category()) {
					err = std::string(" (")
						+boost::lexical_cast<std::string>(ERR_GET_LIB(ec.value()))+","
						+boost::lexical_cast<std::string>(ERR_GET_FUNC(ec.value()))+","
						+boost::lexical_cast<std::string>(ERR_GET_REASON(ec.value()))+") ";
					char buf[128];
					::ERR_error_string_n(ec.value(), buf, sizeof(buf));
					err += buf;
					std::cerr << "Some error on accept: " << err << std::endl;
				}
			}
                    });
                }
            });
        }
    };
}


#endif	/* SERVER_HTTPS_HPP */

