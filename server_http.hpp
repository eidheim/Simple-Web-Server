#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>

#include <regex>
#include <unordered_map>
#include <thread>
#include <functional>

namespace SimpleWeb {
    template <class socket_type>
    class ServerBase {
    public:
        class Response {
            friend class ServerBase<socket_type>;
        private:
            std::shared_ptr<boost::asio::strand> strand;
            
            boost::asio::yield_context& yield;
            
            boost::asio::streambuf streambuf;

            std::shared_ptr<socket_type> socket;
            
            std::shared_ptr<boost::asio::deadline_timer> async_timer;
            
            std::shared_ptr<bool> async_writing;
            std::shared_ptr<bool> async_waiting;

            Response(boost::asio::io_service& io_service, std::shared_ptr<socket_type> socket, std::shared_ptr<boost::asio::strand> strand, 
                    boost::asio::yield_context& yield): 
                    strand(strand), yield(yield), socket(socket), async_timer(new boost::asio::deadline_timer(io_service)), 
                    async_writing(new bool(false)), async_waiting(new bool(false)), stream(&streambuf) {}

            void async_flush(std::function<void((const boost::system::error_code&))> callback=nullptr) {
                if(!callback && !socket->lowest_layer().is_open()) {
                    if(*async_waiting)
                        async_timer->cancel();
                    throw std::runtime_error("Broken pipe.");
                }
                
                std::shared_ptr<boost::asio::streambuf> write_buffer(new boost::asio::streambuf);
                std::ostream response(write_buffer.get());
                response << stream.rdbuf();
                                                    
                //Wait until previous async_flush is finished
                strand->dispatch([this](){
                    if(*async_writing) {
                        *async_waiting=true;
                        try {
                            async_timer->async_wait(yield);
                        }
                        catch(std::exception& e) {
                        }
                        *async_waiting=false;
                    }
                });

                *async_writing=true;
                
                auto socket_=this->socket;
                auto async_writing_=this->async_writing;
                auto async_timer_=this->async_timer;
                auto async_waiting_=this->async_waiting;
                
                boost::asio::async_write(*socket, *write_buffer, 
                        strand->wrap([socket_, write_buffer, callback, async_writing_, async_timer_, async_waiting_]
                        (const boost::system::error_code& ec, size_t bytes_transferred) {
                    *async_writing_=false;
                    if(*async_waiting_)
                        async_timer_->cancel();
                    if(callback)
                        callback(ec);
                }));
            }
            
            void flush() {
                boost::asio::streambuf write_buffer;
                std::ostream response(&write_buffer);
                response << stream.rdbuf();

                boost::asio::async_write(*socket, write_buffer, yield);
            }
                        
        public:
            std::ostream stream;
            
            template <class T>
            Response& operator<<(const T& t) {
                stream << t;
                return *this;
            }

            Response& operator<<(std::ostream& (*manip)(std::ostream&)) {
                stream << manip;
                return *this;
            }
            
            Response& operator<<(Response& (*manip)(Response&)) {
                return manip(*this);
            }
        };
        
        static Response& async_flush(Response& r) {
            r.async_flush();
            return r;
        }
        
        static Response& flush(Response& r) {
            r.flush();
            return r;
        }
        
        class Request {
            friend class ServerBase<socket_type>;
        public:
            std::string method, path, http_version;

            std::istream content;

            std::unordered_map<std::string, std::string> header;

            std::smatch path_match;
            
        private:
            Request(): content(&streambuf) {}
            
            boost::asio::streambuf streambuf;
        };
        
        std::unordered_map<std::string, std::unordered_map<std::string, 
            std::function<void(ServerBase<socket_type>::Response&, std::shared_ptr<ServerBase<socket_type>::Request>)> > >  resource;
        
        std::unordered_map<std::string, 
            std::function<void(ServerBase<socket_type>::Response&, std::shared_ptr<ServerBase<socket_type>::Request>)> > default_resource;

    private:
        std::vector<std::pair<std::string, std::vector<std::pair<std::regex, 
            std::function<void(ServerBase<socket_type>::Response&, std::shared_ptr<ServerBase<socket_type>::Request>)> > > > > opt_resource;
        
    public:
        void start() {
            //Copy the resources to opt_resource for more efficient request processing
            opt_resource.clear();
            for(auto& res: resource) {
                for(auto& res_method: res.second) {
                    auto it=opt_resource.end();
                    for(auto opt_it=opt_resource.begin();opt_it!=opt_resource.end();opt_it++) {
                        if(res_method.first==opt_it->first) {
                            it=opt_it;
                            break;
                        }
                    }
                    if(it==opt_resource.end()) {
                        opt_resource.emplace_back();
                        it=opt_resource.begin()+(opt_resource.size()-1);
                        it->first=res_method.first;
                    }
                    it->second.emplace_back(std::regex(res.first), res_method.second);
                }
            }
                        
            accept(); 
            
            //If num_threads>1, start m_io_service.run() in (num_threads-1) threads for thread-pooling
            threads.clear();
            for(size_t c=1;c<num_threads;c++) {
                threads.emplace_back([this](){
                    io_service.run();
                });
            }

            //Main thread
            io_service.run();

            //Wait for the rest of the threads, if any, to finish as well
            for(auto& t: threads) {
                t.join();
            }
        }
        
        void stop() {
            io_service.stop();
        }

    protected:
        boost::asio::io_service io_service;
        boost::asio::ip::tcp::endpoint endpoint;
        boost::asio::ip::tcp::acceptor acceptor;
        size_t num_threads;
        std::vector<std::thread> threads;
        
        size_t timeout_request;
        size_t timeout_content;
        
        ServerBase(unsigned short port, size_t num_threads, size_t timeout_request, size_t timeout_send_or_receive) : 
                endpoint(boost::asio::ip::tcp::v4(), port), acceptor(io_service, endpoint), num_threads(num_threads), 
                timeout_request(timeout_request), timeout_content(timeout_send_or_receive) {}
        
        virtual void accept()=0;
        
        std::shared_ptr<boost::asio::deadline_timer> set_timeout_on_socket(std::shared_ptr<socket_type> socket, size_t seconds) {
            std::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(io_service));
            timer->expires_from_now(boost::posix_time::seconds(seconds));
            timer->async_wait([socket](const boost::system::error_code& ec){
                if(!ec) {
                    boost::system::error_code ec;
                    socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    socket->lowest_layer().close();
                }
            });
            return timer;
        }       
        
        std::shared_ptr<boost::asio::deadline_timer> set_timeout_on_socket(std::shared_ptr<socket_type> socket, std::shared_ptr<boost::asio::strand> strand, size_t seconds) {
            std::shared_ptr<boost::asio::deadline_timer> timer(new boost::asio::deadline_timer(io_service));
            timer->expires_from_now(boost::posix_time::seconds(seconds));
            timer->async_wait(strand->wrap([socket](const boost::system::error_code& ec){
                if(!ec) {
                    boost::system::error_code ec;
                    socket->lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
                    socket->lowest_layer().close();
                }
            }));
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
                        
            boost::asio::async_read_until(*socket, request->streambuf, "\r\n\r\n",
                    [this, socket, request, timer](const boost::system::error_code& ec, size_t bytes_transferred) {
                if(timeout_request>0)
                    timer->cancel();
                if(!ec) {
                    //request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
                    //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                    //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                    //streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                    size_t num_additional_bytes=request->streambuf.size()-bytes_transferred;
                    
                    parse_request(request, request->content);
                    
                    //If content, read that as well
                    if(request->header.count("Content-Length")>0) {
                        //Set timeout on the following boost::asio::async-read or write function
                        std::shared_ptr<boost::asio::deadline_timer> timer;
                        if(timeout_content>0)
                            timer=set_timeout_on_socket(socket, timeout_content);
                        
                        boost::asio::async_read(*socket, request->streambuf, 
                                boost::asio::transfer_exactly(stoull(request->header["Content-Length"])-num_additional_bytes), 
                                [this, socket, request, timer]
                                (const boost::system::error_code& ec, size_t bytes_transferred) {
                            if(timeout_content>0)
                                timer->cancel();
                            if(!ec)
                                find_resource(socket, request);
                        });
                    }
                    else {
                        find_resource(socket, request);
                    }
                }
            });
        }

        void parse_request(std::shared_ptr<Request> request, std::istream& stream) const {
            std::string line;
            getline(stream, line);
            size_t method_end=line.find(' ');
            size_t path_end=line.find(' ', method_end+1);
            if(method_end!=std::string::npos && path_end!=std::string::npos) {
                request->method=line.substr(0, method_end);
                request->path=line.substr(method_end+1, path_end-method_end-1);
                request->http_version=line.substr(path_end+6, line.size()-path_end-7);

                getline(stream, line);
                size_t param_end=line.find(':');
                while(param_end!=std::string::npos) {                
                    size_t value_start=param_end+1;
                    if(line[value_start]==' ')
                        value_start++;

                    request->header[line.substr(0, param_end)]=line.substr(value_start, line.size()-value_start-1);

                    getline(stream, line);
                    param_end=line.find(':');
                }
            }
        }

        void find_resource(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) {
            //Find path- and method-match, and call write_response
            for(auto& res: opt_resource) {
                if(request->method==res.first) {
                    for(auto& res_path: res.second) {
                        std::smatch sm_res;
                        if(std::regex_match(request->path, sm_res, res_path.first)) {
                            request->path_match=std::move(sm_res);
                            write_response(socket, request, res_path.second);
                            return;
                        }
                    }
                }
            }
            auto it_method=default_resource.find(request->method);
            if(it_method!=default_resource.end()) {
                write_response(socket, request, it_method->second);
            }
        }
        
        void write_response(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request, 
                std::function<void(ServerBase<socket_type>::Response&, std::shared_ptr<ServerBase<socket_type>::Request>)>& resource_function) {
            std::shared_ptr<boost::asio::strand> strand(new boost::asio::strand(io_service));

            //Set timeout on the following boost::asio::async-read or write function
            std::shared_ptr<boost::asio::deadline_timer> timer;
            if(timeout_content>0)
                timer=set_timeout_on_socket(socket, strand, timeout_content);

            boost::asio::spawn(*strand, [this, strand, &resource_function, socket, request, timer](boost::asio::yield_context yield) {
                Response response(io_service, socket, strand, yield);

                try {
                    resource_function(response, request);
                }
                catch(std::exception& e) {
                    return;
                }
                
                response.async_flush([this, socket, request, timer](const boost::system::error_code& ec) {
                    if(timeout_content>0)
                        timer->cancel();
                    if(!ec && stof(request->http_version)>1.05)
                        read_request_and_content(socket);
                });
            });
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
            std::shared_ptr<HTTP> socket(new HTTP(io_service));
                        
            acceptor.async_accept(*socket, [this, socket](const boost::system::error_code& ec){
                //Immediately start accepting a new connection
                accept();
                                
                if(!ec) {
                    boost::asio::ip::tcp::no_delay option(true);
                    socket->set_option(option);
                    
                    read_request_and_content(socket);
                }
            });
        }
    };
}
#endif	/* SERVER_HTTP_HPP */