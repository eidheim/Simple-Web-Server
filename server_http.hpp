#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#include <boost/asio.hpp>

#include <regex>
#include <unordered_map>
#include <thread>

namespace SimpleWeb {
    template <class socket_type>
    class ServerBase {
    public:
        class Request {
            friend class ServerBase<socket_type>;
        public:
            std::string method, path, http_version;

            std::istream content;

            std::unordered_map<std::string, std::string> header;

            std::smatch path_match;
            
        private:
            Request(): content(&content_buffer) {}
            
            boost::asio::streambuf content_buffer;
        };
        
        typedef std::map<std::string, std::unordered_map<std::string, 
                std::function<void(std::ostream&, std::shared_ptr<ServerBase<socket_type>::Request>)> > > resource_type;
        
        resource_type resource;

        resource_type default_resource;
        
        void start() {
            //All resources with default_resource at the end of vector
            //Used in the respond-method
            all_resources.clear();
            for(auto it=resource.begin(); it!=resource.end();it++) {
                all_resources.push_back(it);
            }
            for(auto it=default_resource.begin(); it!=default_resource.end();it++) {
                all_resources.push_back(it);
            }
            
            accept();            
            
            //If num_threads>1, start m_io_service.run() in (num_threads-1) threads for thread-pooling
            threads.clear();
            for(size_t c=1;c<num_threads;c++) {
                threads.emplace_back([this](){
                    m_io_service.run();
                });
            }

            //Main thread
            m_io_service.run();

            //Wait for the rest of the threads, if any, to finish as well
            for(auto& t: threads) {
                t.join();
            }
        }
        
        void stop() {
            m_io_service.stop();
        }

    protected:
        boost::asio::io_service m_io_service;
        boost::asio::ip::tcp::endpoint endpoint;
        boost::asio::ip::tcp::acceptor acceptor;
        size_t num_threads;
        std::vector<std::thread> threads;
        
        size_t timeout_request;
        size_t timeout_content;

        //All resources with default_resource at the end of vector
        //Created in start()
        std::vector<typename resource_type::iterator> all_resources;
        
        ServerBase(unsigned short port, size_t num_threads, size_t timeout_request, size_t timeout_send_or_receive) : 
                endpoint(boost::asio::ip::tcp::v4(), port), acceptor(m_io_service, endpoint), num_threads(num_threads), 
                timeout_request(timeout_request), timeout_content(timeout_send_or_receive) {}
        
        virtual void accept()=0;
        
        std::shared_ptr<boost::asio::deadline_timer> set_timeout_on_socket(std::shared_ptr<socket_type> socket, size_t seconds) {
            std::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(m_io_service));
            timer->expires_from_now(boost::posix_time::seconds(seconds));
            timer->async_wait([socket](const boost::system::error_code& ec){
                if(!ec) {
                    socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                    socket->lowest_layer().close();
                }
            });
            return timer;
        }
        
        void read_request_and_content(std::shared_ptr<socket_type> socket) {
            //Create new streambuf (Request::streambuf) for async_read_until()
            //shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<Request> request(new Request());

            //Set timeout on the following boost::asio::async-read or write function
            std::shared_ptr<boost::asio::deadline_timer> timer;
            if(timeout_request>0)
                timer=set_timeout_on_socket(socket, timeout_request);
            
            boost::asio::async_read_until(*socket, request->content_buffer, "\r\n\r\n",
                    [this, socket, request, timer](const boost::system::error_code& ec, size_t bytes_transferred) {
                if(timeout_request>0)
                    timer->cancel();
                if(!ec) {
                    //request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                    //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                    //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                    //streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                    size_t num_additional_bytes=request->content_buffer.size()-bytes_transferred;
                    
                    parse_request(request, request->content);
                    
                    //If content, read that as well
                    if(request->header.count("Content-Length")>0) {
                        //Set timeout on the following boost::asio::async-read or write function
                        std::shared_ptr<boost::asio::deadline_timer> timer;
                        if(timeout_content>0)
                            timer=set_timeout_on_socket(socket, timeout_content);
                        
                        boost::asio::async_read(*socket, request->content_buffer, 
                                boost::asio::transfer_exactly(stoull(request->header["Content-Length"])-num_additional_bytes), 
                                [this, socket, request, timer]
                                (const boost::system::error_code& ec, size_t bytes_transferred) {
                            if(timeout_content>0)
                                timer->cancel();
                            if(!ec)
                                write_response(socket, request);
                        });
                    }
                    else {                   
                        write_response(socket, request);
                    }
                }
            });
        }

        void parse_request(std::shared_ptr<Request> request, std::istream& stream) const {
            std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

            std::smatch sm;

            //First parse request method, path, and HTTP-version from the first line
            std::string line;
            getline(stream, line);
            line.pop_back();
            if(std::regex_match(line, sm, e)) {        
                request->method=sm[1];
                request->path=sm[2];
                request->http_version=sm[3];

                bool matched;
                e="^([^:]*): ?(.*)$";
                //Parse the rest of the header
                do {
                    getline(stream, line);
                    line.pop_back();
                    matched=std::regex_match(line, sm, e);
                    if(matched) {
                        request->header[sm[1]]=sm[2];
                    }

                } while(matched==true);
            }
        }

        void write_response(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) {
            //Find path- and method-match, and generate response
            for(auto res_it: all_resources) {
                std::regex e(res_it->first);
                std::smatch sm_res;
                if(std::regex_match(request->path, sm_res, e)) {
                    if(res_it->second.count(request->method)>0) {
                        request->path_match=move(sm_res);
                        
                        std::shared_ptr<boost::asio::streambuf> write_buffer(new boost::asio::streambuf);
                        std::ostream response(write_buffer.get());
                        res_it->second[request->method](response, request);

                        //Set timeout on the following boost::asio::async-read or write function
                        std::shared_ptr<boost::asio::deadline_timer> timer;
                        if(timeout_content>0)
                            timer=set_timeout_on_socket(socket, timeout_content);
                        
                        //Capture write_buffer in lambda so it is not destroyed before async_write is finished
                        boost::asio::async_write(*socket, *write_buffer, 
                                [this, socket, request, write_buffer, timer]
                                (const boost::system::error_code& ec, size_t bytes_transferred) {
                            if(timeout_content>0)
                                timer->cancel();
                            //HTTP persistent connection (HTTP 1.1):
                            if(!ec && stof(request->http_version)>1.05)
                                read_request_and_content(socket);
                        });
                        return;
                    }
                }
            }
        }
    };
    
    template<class socket_type>
    class Server : public ServerBase<socket_type> {};
    
    typedef boost::asio::ip::tcp::socket HTTP;
    
    template<>
    class Server<HTTP> : public ServerBase<HTTP> {
    public:
        Server(unsigned short port, size_t num_threads=1, size_t timeout_request=5, size_t timeout_content=300) : 
                ServerBase<HTTP>::ServerBase(port, num_threads, timeout_request, timeout_content) {};
        
    private:
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            std::shared_ptr<HTTP> socket(new HTTP(m_io_service));
            
            acceptor.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
                //Immediately start accepting a new connection
                accept();

                if(!ec) {
                    read_request_and_content(socket);
                }
            });
        }
    };
}
#endif	/* SERVER_HTTP_HPP */