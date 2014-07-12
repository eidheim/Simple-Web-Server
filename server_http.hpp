#ifndef SERVER_HTTP_HPP
#define	SERVER_HTTP_HPP

#include <boost/asio.hpp>

#include <regex>
#include <unordered_map>
#include <thread>

using namespace std;
using namespace boost::asio;

namespace SimpleWeb {    
    struct Request {
        string method, path, http_version;

        shared_ptr<istream> content;

        unordered_map<string, string> header;
    };

    typedef map<string, unordered_map<string, function<void(ostream&, const Request&, const smatch&)> > > resource_type;
    
    template <class socket_type>
    class ServerBase {
    public:
        resource_type resources;

        resource_type default_resource;
        
        ServerBase(unsigned short port, size_t num_threads=1) : endpoint(ip::tcp::v4(), port), 
            acceptor(m_io_service, endpoint), num_threads(num_threads) {}        
        
        void start() {
            //All resources with default_resource at the end of vector
            //Used in the respond-method
            for(auto it=resources.begin(); it!=resources.end();it++) {
                all_resources.push_back(it);
            }
            for(auto it=default_resource.begin(); it!=default_resource.end();it++) {
                all_resources.push_back(it);
            }
            
            accept();            
            
            //If num_threads>1, start m_io_service.run() in (num_threads-1) threads for thread-pooling
            for(size_t c=1;c<num_threads;c++) {
                threads.emplace_back([this](){
                    m_io_service.run();
                });
            }

            //Main thread
            m_io_service.run();

            //Wait for the rest of the threads, if any, to finish as well
            for(thread& t: threads) {
                t.join();
            }
        }

    protected:
        io_service m_io_service;
        ip::tcp::endpoint endpoint;
        ip::tcp::acceptor acceptor;
        //shared_ptr<ssl::context> context;
        size_t num_threads;
        vector<thread> threads;

        //All resources with default_resource at the end of vector
        //Created in start()
        vector<resource_type::iterator> all_resources;
        
        virtual void accept() {}

        void process_request_and_respond(shared_ptr<socket_type> socket) {
            //Create new read_buffer for async_read_until()
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            shared_ptr<boost::asio::streambuf> read_buffer(new boost::asio::streambuf);

            async_read_until(*socket, *read_buffer, "\r\n\r\n",
            [this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
                if(!ec) {
                    //read_buffer->size() is not necessarily the same as bytes_transferred, from Boost-docs:
                    //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
                    //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
                    //read_buffer (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
                    size_t total=read_buffer->size();

                    //Convert to istream to extract string-lines
                    istream stream(read_buffer.get());

                    shared_ptr<Request> request(new Request());
                    *request=parse_request(stream);

                    size_t num_additional_bytes=total-bytes_transferred;

                    //If content, read that as well
                    if(request->header.count("Content-Length")>0) {
                        async_read(*socket, *read_buffer, transfer_exactly(stoull(request->header["Content-Length"])-num_additional_bytes), 
                        [this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes_transferred) {
                            if(!ec) {
                                //Store pointer to read_buffer as istream object
                                request->content=shared_ptr<istream>(new istream(read_buffer.get()));

                                respond(socket, request);
                            }
                        });
                    }
                    else {                   
                        respond(socket, request);
                    }
                }
            });
        }

        Request parse_request(istream& stream) {
            Request request;

            regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

            smatch sm;

            //First parse request method, path, and HTTP-version from the first line
            string line;
            getline(stream, line);
            line.pop_back();
            if(regex_match(line, sm, e)) {        
                request.method=sm[1];
                request.path=sm[2];
                request.http_version=sm[3];

                bool matched;
                e="^([^:]*): ?(.*)$";
                //Parse the rest of the header
                do {
                    getline(stream, line);
                    line.pop_back();
                    matched=regex_match(line, sm, e);
                    if(matched) {
                        request.header[sm[1]]=sm[2];
                    }

                } while(matched==true);
            }

            return request;
        }

        void respond(shared_ptr<socket_type> socket, shared_ptr<Request> request) {
            //Find path- and method-match, and generate response
            for(auto res_it: all_resources) {
                regex e(res_it->first);
                smatch sm_res;
                if(regex_match(request->path, sm_res, e)) {
                    if(res_it->second.count(request->method)>0) {
                        shared_ptr<boost::asio::streambuf> write_buffer(new boost::asio::streambuf);
                        ostream response(write_buffer.get());
                        res_it->second[request->method](response, *request, sm_res);

                        //Capture write_buffer in lambda so it is not destroyed before async_write is finished
                        async_write(*socket, *write_buffer, [this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
                            //HTTP persistent connection (HTTP 1.1):
                            if(!ec && stof(request->http_version)>1.05)
                                process_request_and_respond(socket);
                        });
                        return;
                    }
                }
            }
        }
    };
    
    template<class socket_type>
    class Server : public ServerBase<socket_type> {};
    
    typedef ip::tcp::socket HTTP;
    
    template<>
    class Server<HTTP> : public ServerBase<HTTP> {
    public:
        Server(unsigned short port, size_t num_threads=1) : ServerBase<HTTP>::ServerBase(port, num_threads) {};
        
    private:
        void accept() {
            //Create new socket for this connection
            //Shared_ptr is used to pass temporary objects to the asynchronous functions
            shared_ptr<HTTP> socket(new HTTP(m_io_service));

            acceptor.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
                //Immediately start accepting a new connection
                accept();

                if(!ec) {
                    process_request_and_respond(socket);
                }
            });
        }
    };
}
#endif	/* SERVER_HTTP_HPP */