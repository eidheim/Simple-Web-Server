#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#include <boost/asio.hpp>

#include <regex>
#include <unordered_map>
#include <thread>
#include <iostream>

#include "request.hpp"
#include "request_parser.hpp"

namespace SimpleWeb {
	typedef std::function<void(std::ostream&, std::shared_ptr<Request>)> Function;

	enum class CallbackType {
		POST,
		GET,
		INVALID
	};

	struct Callback {
		CallbackType type;
		Function functions[2];

		inline Callback() : type(CallbackType::INVALID) {}
		inline Callback(CallbackType ct, Function func) : type(ct) {
			functions[static_cast<size_t>(ct)] = func;
		}
	};

    template <class socket_type>
    class ServerBase {
    public:
        //typedef std::map<std::string, std::unordered_map<std::string, Function> > resource_type;
        typedef std::map<std::string, Callback> ResourceMap;

		typedef std::vector<std::pair<std::string, Callback>> ResourceVec;

        ResourceMap resource_map;
        ResourceVec resource_vec;
        Callback default_resource;
        
        void start() {
            all_resources.clear();
            for(auto it = resource_map.begin(); it != resource_map.end(); ++it) {
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

		std::function<void(std::shared_ptr<Request>, std::istream&)> request_parser;

        //All resources with default_resource at the end of vector
        //Created in start()
        std::vector<typename ResourceMap::iterator> all_resources;
        
        ServerBase(unsigned short port, size_t num_threads, size_t timeout_request, size_t timeout_send_or_receive) : 
                endpoint(boost::asio::ip::tcp::v4(), port), acceptor(m_io_service, endpoint), num_threads(num_threads), 
                timeout_request(timeout_request), timeout_content(timeout_send_or_receive), 
				request_parser(&parser_handwritten) {} // change comment with next line for fast version
				//request_parser(&parser_regex) {} // change comment with previous line for slow version
        
        virtual void accept()=0;
        
        std::shared_ptr<boost::asio::deadline_timer> set_timeout_on_socket(std::shared_ptr<socket_type> socket, size_t seconds) {
            auto timer(std::make_shared<boost::asio::deadline_timer>(m_io_service));
 
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
			auto request(std::make_shared<Request>());

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
                    
                    request_parser(request, request->content);
                    
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

		template<typename Func>
		void write_response_impl(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request,
				Func& function) 
		{
        	auto write_buffer(std::make_shared<boost::asio::streambuf>());
        	std::ostream response(write_buffer.get());
        	//res_it->second.functions[static_cast<size_t>(type)](response, request);
        	//functions[static_cast<size_t>(type)](response, request);
        	function(response, request);

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
		}

        void write_response(std::shared_ptr<socket_type> socket, std::shared_ptr<Request> request) {
            //Find path- and method-match, and generate response
			const CallbackType type = (request->method == "POST" ? 
				CallbackType::POST : request->method == "GET" ? 
				CallbackType::GET : CallbackType::INVALID);

			if(type == CallbackType::INVALID) {
				std::cout<<"invalid http type"<<std::endl;
				return;
			}

			for(auto& res_it : resource_vec) {
				if(request->path.find(res_it.first) == 0u) {
					write_response_impl(socket, request,
						res_it.second.functions[static_cast<size_t>(type)]
					);
					return;
				}
			}

            for(auto& res_it : all_resources) {
                std::regex e(res_it->first);
                std::smatch sm_res;
                if(std::regex_match(request->path, sm_res, e)) {
					if(res_it->second.type == type) {
                        request->path_match=move(sm_res);
						write_response_impl(socket, request,
							res_it->second.functions[static_cast<size_t>(type)]
						);

                        return;
                    }
                }
            }

			// default resource
			if(this->default_resource.type == type) {
				// TODO do shit
				write_response_impl(socket, request, 
					default_resource.functions[static_cast<size_t>(type)]);
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
            auto socket(std::make_shared<HTTP>(m_io_service));
            
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
