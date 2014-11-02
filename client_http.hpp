#ifndef CLIENT_HTTP_HPP
#define	CLIENT_HTTP_HPP

#include <boost/asio.hpp>

#include <unordered_map>
#include <map>
#include <iostream>
#include <regex>
#include <random>

namespace SimpleWeb {
    template <class socket_type>
    class ClientBase {
    public:
        class Response {
            friend class ClientBase<socket_type>;
            
        public:
            std::string http_version, status_code;

            std::istream content;

            std::unordered_map<std::string, std::string> header;
            
        private:
            boost::asio::streambuf content_buffer;
            
            Response(): content(&content_buffer) {};
        };
    
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path="/", 
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            std::stringstream empty_ss;
            return request(request_type, path, empty_ss, header);
        }
        
        std::shared_ptr<Response> request(const std::string& request_type, const std::string& path, std::ostream& content, 
                const std::map<std::string, std::string>& header=std::map<std::string, std::string>()) {
            std::string corrected_path=path;
            if(corrected_path=="")
                corrected_path="/";
            
            content.seekp(0, std::ios::end);
            size_t content_length=content.tellp();
            content.seekp(0, std::ios::beg);
            
            boost::asio::streambuf write_buffer;
            std::ostream write_stream(&write_buffer);
            write_stream << request_type << " " << corrected_path << " HTTP/1.1\r\n";
            write_stream << "Host: " << host << "\r\n";
            for(auto& h: header) {
                write_stream << h.first << ": " << h.second << "\r\n";
            }
            if(content_length>0)
                write_stream << "Content-Length: " << std::to_string(content_length) << "\r\n";
            write_stream << "\r\n";
            if(content_length>0)
                write_stream << content.rdbuf();
            
            std::shared_ptr<Response> response(new Response());
            
            try {
                connect();
                
                boost::asio::write(*socket, write_buffer);

                size_t bytes_transferred = boost::asio::read_until(*socket, response->content_buffer, "\r\n\r\n");

                size_t num_additional_bytes=response->content_buffer.size()-bytes_transferred;

                parse_response_header(response, response->content);
                                
                if(response->header.count("Content-Length")>0) {
                    boost::asio::read(*socket, response->content_buffer, 
                            boost::asio::transfer_exactly(stoull(response->header["Content-Length"])-num_additional_bytes));
                }
                else if(response->header.count("Transfer-Encoding")>0 && response->header["Transfer-Encoding"]=="chunked") {
                    boost::asio::streambuf streambuf;
                    std::ostream content(&streambuf);
                    
                    size_t length;
                    std::string buffer;
                    do {
                        size_t bytes_transferred = boost::asio::read_until(*socket, response->content_buffer, "\r\n");
                        std::string line;
                        getline(response->content, line);
                        bytes_transferred-=line.size()+1;
                        line.pop_back();
                        length=stoull(line, 0, 16);

                        size_t num_additional_bytes=response->content_buffer.size()-bytes_transferred;
                    
                        if((2+length)>num_additional_bytes) {
                            boost::asio::read(*socket, response->content_buffer, 
                                boost::asio::transfer_exactly(2+length-num_additional_bytes));
                        }
                                                
                        buffer.resize(length);
                        response->content.read(&buffer[0], length);
                        content.write(&buffer[0], length);

                        //Remove "\r\n"
                        response->content.get();
                        response->content.get();
                    } while(length>0);
                    
                    std::ostream response_content_output_stream(&response->content_buffer);
                    response_content_output_stream << content.rdbuf();
                }
            }
            catch(const std::exception& e) {
                socket_error=true;
                throw std::invalid_argument(e.what());
            }
            
            return response;
        }
        
    protected:
        boost::asio::io_service asio_io_service;
        boost::asio::ip::tcp::endpoint asio_endpoint;
        boost::asio::ip::tcp::resolver asio_resolver;
        
        std::shared_ptr<socket_type> socket;
        bool socket_error;
        
        std::string host;
        unsigned short port;
                
        ClientBase(const std::string& host_port, unsigned short default_port) : 
                asio_resolver(asio_io_service), socket_error(false) {
            std::regex e("^([^:/]+):?([0-9]*)$");

            std::smatch sm;

            if(std::regex_match(host_port, sm, e)) {
                host=sm[1];
                port=default_port;
                if(sm[2]!="")
                    port=(unsigned short)std::stoul(sm[2]);
                asio_endpoint=boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);
            }
            else {
                throw std::invalid_argument("Error parsing host_port");
            }
        }
        
        virtual void connect()=0;
        
        void parse_response_header(std::shared_ptr<Response> response, std::istream& stream) const {            
            std::smatch sm;

            //Parse the first line
            std::string line;
            getline(stream, line);
            line.pop_back();

            std::regex e("^HTTP/([^ ]*) (.*)$");
            if(std::regex_match(line, sm, e)) {
                response->http_version=sm[1];
                response->status_code=sm[2];
                
                e="^([^:]*): ?(.*)$";
                //Parse the rest of the header
                bool matched;
                do {
                    getline(stream, line);
                    line.pop_back();
                    matched=std::regex_match(line, sm, e);
                    if(matched) {
                        response->header[sm[1]]=sm[2];
                    }

                } while(matched==true);
            }
        }
    };
    
    template<class socket_type>
    class Client : public ClientBase<socket_type> {};
    
    typedef boost::asio::ip::tcp::socket HTTP;
    
    template<>
    class Client<HTTP> : public ClientBase<HTTP> {
    public:
        Client(const std::string& server_port_path) : ClientBase<HTTP>::ClientBase(server_port_path, 80) {
            socket=std::make_shared<HTTP>(asio_io_service);
        };
        
    private:
        void connect() {
            if(socket_error || !socket->is_open()) {
                boost::asio::ip::tcp::resolver::query query(host, std::to_string(port));
                boost::asio::connect(*socket, asio_resolver.resolve(query));
                socket_error=false;
            }
        }
    };
}

#endif	/* CLIENT_HTTP_HPP */