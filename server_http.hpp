#ifndef SERVER_HTTP_HPP
#define SERVER_HTTP_HPP

#include "utility.hpp"
#include <condition_variable>
#include <functional>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>
#include <unordered_set>

#ifdef USE_STANDALONE_ASIO
#include <asio.hpp>
namespace SimpleWeb {
  using error_code = std::error_code;
  using errc = std::errc;
  namespace make_error_code = std;
} // namespace SimpleWeb
#else
#include <boost/asio.hpp>
namespace SimpleWeb {
  namespace asio = boost::asio;
  using error_code = boost::system::error_code;
  namespace errc = boost::system::errc;
  namespace make_error_code = boost::system::errc;
} // namespace SimpleWeb
#endif

// Late 2017 TODO: remove the following checks and always use std::regex
#ifdef USE_BOOST_REGEX
#include <boost/regex.hpp>
namespace SimpleWeb {
  namespace regex = boost;
}
#else
#include <regex>
namespace SimpleWeb {
  namespace regex = std;
}
#endif

namespace SimpleWeb {
  template <class socket_type>
  class Server;

  template <class socket_type>
  class ServerBase {
    ServerBase(const ServerBase &) = delete;
    ServerBase &operator=(const ServerBase &) = delete;

  protected:
    class Session;

  public:
    class Response : public std::enable_shared_from_this<Response>, public std::ostream {
      friend class ServerBase<socket_type>;
      friend class Server<socket_type>;

      asio::streambuf streambuf;

      std::shared_ptr<Session> session;

      Response(const std::shared_ptr<Session> &session) : std::ostream(&streambuf), session(session) {}

      template <class size_type>
      void write_header(const CaseInsensitiveMultimap &header, size_type size) {
        bool content_length_written = false;
        bool chunked_transfer_encoding = false;
        for(auto &field : header) {
          if(!content_length_written && case_insensitive_equal(field.first, "content-length"))
            content_length_written = true;
          else if(!chunked_transfer_encoding && case_insensitive_equal(field.first, "transfer-encoding") && case_insensitive_equal(field.second, "chunked"))
            chunked_transfer_encoding = true;

          *this << field.first << ": " << field.second << "\r\n";
        }
        if(!content_length_written && !chunked_transfer_encoding && !close_connection_after_response)
          *this << "Content-Length: " << size << "\r\n\r\n";
        else
          *this << "\r\n";
      }

    public:
      size_t size() {
        return streambuf.size();
      }

      /// Use this function if you need to recursively send parts of a longer message
      void send(const std::function<void(const error_code &)> &callback = nullptr) {
        auto lock = session->cancel_callbacks_mutex->shared_lock();
        if(*session->cancel_callbacks)
          return;
        auto self = this->shared_from_this();
        session->set_timeout(session->server->config.timeout_content);
        asio::async_write(*session->connection->socket, streambuf, [self, callback](const error_code &ec, size_t /*bytes_transferred*/) {
          self->session->cancel_timeout();
          auto lock = self->session->cancel_callbacks_mutex->shared_lock();
          if(*self->session->cancel_callbacks)
            return;
          if(callback)
            callback(ec);
        });
      }

      /// Write directly to stream buffer using std::ostream::write
      void write(const char_type *ptr, std::streamsize n) {
        std::ostream::write(ptr, n);
      }

      /// Convenience function for writing status line, potential header fields, and empty content
      void write(StatusCode status_code = StatusCode::success_ok, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        write_header(header, 0);
      }

      /// Convenience function for writing status line, header fields, and content
      void write(StatusCode status_code, const std::string &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        write_header(header, content.size());
        if(!content.empty())
          *this << content;
      }

      /// Convenience function for writing status line, header fields, and content
      void write(StatusCode status_code, std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        *this << "HTTP/1.1 " << SimpleWeb::status_code(status_code) << "\r\n";
        content.seekg(0, std::ios::end);
        auto size = content.tellg();
        content.seekg(0, std::ios::beg);
        write_header(header, size);
        if(size)
          *this << content.rdbuf();
      }

      /// Convenience function for writing success status line, header fields, and content
      void write(const std::string &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        write(StatusCode::success_ok, content, header);
      }

      /// Convenience function for writing success status line, header fields, and content
      void write(std::istream &content, const CaseInsensitiveMultimap &header = CaseInsensitiveMultimap()) {
        write(StatusCode::success_ok, content, header);
      }

      /// Convenience function for writing success status line, and header fields
      void write(const CaseInsensitiveMultimap &header) {
        write(StatusCode::success_ok, std::string(), header);
      }

      /// If true, force server to close the connection after the response have been sent.
      ///
      /// This is useful when implementing a HTTP/1.0-server sending content
      /// without specifying the content length.
      bool close_connection_after_response = false;
    };

    class Content : public std::istream {
      friend class ServerBase<socket_type>;

    public:
      size_t size() {
        return streambuf.size();
      }
      /// Convenience function to return std::string. Note that the stream buffer is emptied when this functions is used.
      std::string string() {
        std::stringstream ss;
        ss << rdbuf();
        return ss.str();
      }

    private:
      asio::streambuf &streambuf;
      Content(asio::streambuf &streambuf) : std::istream(&streambuf), streambuf(streambuf) {}
    };

    class Request {
      friend class ServerBase<socket_type>;
      friend class Server<socket_type>;
      friend class Session;

    public:
      std::string method, path, query_string, http_version;

      Content content;

      CaseInsensitiveMultimap header;

      regex::smatch path_match;

      std::string remote_endpoint_address;
      unsigned short remote_endpoint_port;

      /// Returns query keys with percent-decoded values.
      CaseInsensitiveMultimap parse_query_string() {
        return SimpleWeb::QueryString::parse(query_string);
      }

    private:
      asio::streambuf streambuf;

      Request(const socket_type &socket) : content(streambuf) {
        try {
          remote_endpoint_address = socket.lowest_layer().remote_endpoint().address().to_string();
          remote_endpoint_port = socket.lowest_layer().remote_endpoint().port();
        }
        catch(...) {
        }
      }

      bool parse() {
        std::string line;
        getline(content, line);
        size_t method_end;
        if((method_end = line.find(' ')) != std::string::npos) {
          method = line.substr(0, method_end);

          size_t query_start = std::string::npos;
          size_t path_and_query_string_end = std::string::npos;
          for(size_t i = method_end + 1; i < line.size(); ++i) {
            if(line[i] == '?' && (i + 1) < line.size())
              query_start = i + 1;
            else if(line[i] == ' ') {
              path_and_query_string_end = i;
              break;
            }
          }
          if(path_and_query_string_end != std::string::npos) {
            if(query_start != std::string::npos) {
              path = line.substr(method_end + 1, query_start - method_end - 2);
              query_string = line.substr(query_start, path_and_query_string_end - query_start);
            }
            else
              path = line.substr(method_end + 1, path_and_query_string_end - method_end - 1);

            size_t protocol_end;
            if((protocol_end = line.find('/', path_and_query_string_end + 1)) != std::string::npos) {
              if(line.compare(path_and_query_string_end + 1, protocol_end - path_and_query_string_end - 1, "HTTP") != 0)
                return false;
              http_version = line.substr(protocol_end + 1, line.size() - protocol_end - 2);
            }
            else
              return false;

            getline(content, line);
            size_t param_end;
            while((param_end = line.find(':')) != std::string::npos) {
              size_t value_start = param_end + 1;
              if(value_start < line.size()) {
                if(line[value_start] == ' ')
                  value_start++;
                if(value_start < line.size())
                  header.emplace(line.substr(0, param_end), line.substr(value_start, line.size() - value_start - 1));
              }

              getline(content, line);
            }
          }
          else
            return false;
        }
        else
          return false;
        return true;
      }
    };

  protected:
    class Connection {
    public:
      Connection(std::unique_ptr<socket_type> &&socket) : socket(std::move(socket)) {}

      std::unique_ptr<socket_type> socket; // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable
      std::mutex socket_close_mutex;

      void close() {
        error_code ec;
        std::unique_lock<std::mutex> lock(socket_close_mutex); // the following operations seems to be needed to run sequentially
        socket->lowest_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        socket->lowest_layer().close(ec);
      }
    };

    class Session {
    public:
      Session(ServerBase<socket_type> *server, const std::shared_ptr<Connection> &connection)
          : server(server), cancel_callbacks(server->cancel_callbacks), cancel_callbacks_mutex(server->cancel_callbacks_mutex),
            connection(connection), request(new Request(*connection->socket)) {}

      ServerBase<socket_type> *server;
      std::shared_ptr<bool> cancel_callbacks;
      std::shared_ptr<SharedMutex> cancel_callbacks_mutex;
      std::shared_ptr<Connection> connection;
      std::shared_ptr<Request> request;

      std::unique_ptr<asio::deadline_timer> timer;

      void set_timeout(long seconds) {
        if(seconds == 0) {
          timer = nullptr;
          return;
        }

        timer = std::unique_ptr<asio::deadline_timer>(new asio::deadline_timer(connection->socket->get_io_service()));
        timer->expires_from_now(boost::posix_time::seconds(seconds));
        auto connection = this->connection;
        timer->async_wait([connection](const error_code &ec) {
          if(!ec)
            connection->close();
        });
      }

      void cancel_timeout() {
        if(timer)
          timer->cancel();
      }
    };

  public:
    class Config {
      friend class ServerBase<socket_type>;

      Config(unsigned short port) : port(port) {}

    public:
      /// Port number to use. Defaults to 80 for HTTP and 443 for HTTPS.
      unsigned short port;
      /// If io_service is not set, number of threads that the server will use when start() is called.
      /// Defaults to 1 thread.
      size_t thread_pool_size = 1;
      /// Timeout on request handling. Defaults to 5 seconds.
      long timeout_request = 5;
      /// Timeout on content handling. Defaults to 300 seconds.
      long timeout_content = 300;
      /// IPv4 address in dotted decimal form or IPv6 address in hexadecimal notation.
      /// If empty, the address will be any address.
      std::string address;
      /// Set to false to avoid binding the socket to an address that is already in use. Defaults to true.
      bool reuse_address = true;
    };
    ///Set before calling start().
    Config config;

  private:
    class regex_orderable : public regex::regex {
      std::string str;

    public:
      regex_orderable(const char *regex_cstr) : regex::regex(regex_cstr), str(regex_cstr) {}
      regex_orderable(const std::string &regex_str) : regex::regex(regex_str), str(regex_str) {}
      bool operator<(const regex_orderable &rhs) const {
        return str < rhs.str;
      }
    };

  public:
    /// Warning: do not add or remove resources after start() is called
    std::map<regex_orderable, std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>>> resource;

    std::map<std::string, std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)>> default_resource;

    std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Request>, const error_code &)> on_error;

    std::function<void(std::unique_ptr<socket_type> &, std::shared_ptr<typename ServerBase<socket_type>::Request>)> on_upgrade;

    /// If you have your own asio::io_service, store its pointer here before running start().
    std::shared_ptr<asio::io_service> io_service;

    virtual void start() {
      if(!io_service) {
        io_service = std::make_shared<asio::io_service>();
        internal_io_service = true;
      }

      if(io_service->stopped())
        io_service->reset();

      asio::ip::tcp::endpoint endpoint;
      if(config.address.size() > 0)
        endpoint = asio::ip::tcp::endpoint(asio::ip::address::from_string(config.address), config.port);
      else
        endpoint = asio::ip::tcp::endpoint(asio::ip::tcp::v4(), config.port);

      if(!acceptor)
        acceptor = std::unique_ptr<asio::ip::tcp::acceptor>(new asio::ip::tcp::acceptor(*io_service));
      acceptor->open(endpoint.protocol());
      acceptor->set_option(asio::socket_base::reuse_address(config.reuse_address));
      acceptor->bind(endpoint);
      acceptor->listen();

      accept();

      if(internal_io_service) {
        //If thread_pool_size>1, start m_io_service.run() in (thread_pool_size-1) threads for thread-pooling
        threads.clear();
        for(size_t c = 1; c < config.thread_pool_size; c++) {
          threads.emplace_back([this]() {
            this->io_service->run();
          });
        }

        //Main thread
        if(config.thread_pool_size > 0)
          io_service->run();

        //Wait for the rest of the threads, if any, to finish as well
        for(auto &t : threads)
          t.join();
      }
    }

    /// Stop accepting new requests, and close current connections.
    void stop() {
      if(acceptor) {
        acceptor->close();
        if(internal_io_service)
          io_service->stop();

        std::unique_lock<std::mutex> lock(*connections_mutex);
        if(!internal_io_service) {
          for(auto &connection : *connections)
            connection->close();
        }
        connections->clear();
      }
    }

    virtual ~ServerBase() {
      {
        auto lock = cancel_callbacks_mutex->unique_lock();
        *cancel_callbacks = true;
      }
      stop();
    }

  protected:
    bool internal_io_service = false;

    std::unique_ptr<asio::ip::tcp::acceptor> acceptor;
    std::vector<std::thread> threads;

    std::shared_ptr<std::unordered_set<Connection *>> connections;
    std::shared_ptr<std::mutex> connections_mutex;

    std::shared_ptr<bool> cancel_callbacks;
    std::shared_ptr<SharedMutex> cancel_callbacks_mutex;

    ServerBase(unsigned short port) : config(port), connections(new std::unordered_set<Connection *>()), connections_mutex(new std::mutex()),
                                      cancel_callbacks(new bool(false)), cancel_callbacks_mutex(new SharedMutex()) {}

    virtual void accept() = 0;

    std::shared_ptr<Connection> create_connection(socket_type *socket) {
      auto connections = this->connections;
      auto connections_mutex = this->connections_mutex;
      auto connection = std::shared_ptr<Connection>(new Connection(std::unique_ptr<socket_type>(socket)), [connections, connections_mutex](Connection *connection) {
        {
          std::unique_lock<std::mutex> lock(*connections_mutex);
          auto it = connections->find(connection);
          if(it != connections->end())
            connections->erase(it);
        }
        delete connection;
      });
      {
        std::unique_lock<std::mutex> lock(*connections_mutex);
        connections->emplace(connection.get());
      }
      return connection;
    }

    void read_request_and_content(const std::shared_ptr<Session> &session) {
      session->set_timeout(config.timeout_request);
      asio::async_read_until(*session->connection->socket, session->request->streambuf, "\r\n\r\n", [this, session](const error_code &ec, size_t bytes_transferred) {
        session->cancel_timeout();
        auto lock = session->cancel_callbacks_mutex->shared_lock();
        if(*session->cancel_callbacks)
          return;
        if(!ec) {
          //request->streambuf.size() is not necessarily the same as bytes_transferred, from Boost-docs:
          //"After a successful async_read_until operation, the streambuf may contain additional data beyond the delimiter"
          //The chosen solution is to extract lines from the stream directly when parsing the header. What is left of the
          //streambuf (maybe some bytes of the content) is appended to in the async_read-function below (for retrieving content).
          size_t num_additional_bytes = session->request->streambuf.size() - bytes_transferred;

          if(!session->request->parse())
            return;

          //If content, read that as well
          auto it = session->request->header.find("Content-Length");
          if(it != session->request->header.end()) {
            unsigned long long content_length;
            try {
              content_length = stoull(it->second);
            }
            catch(const std::exception &e) {
              if(this->on_error)
                this->on_error(session->request, make_error_code::make_error_code(errc::protocol_error));
              return;
            }
            if(content_length > num_additional_bytes) {
              session->set_timeout(config.timeout_content);
              asio::async_read(*session->connection->socket, session->request->streambuf, asio::transfer_exactly(content_length - num_additional_bytes), [this, session](const error_code &ec, size_t /*bytes_transferred*/) {
                session->cancel_timeout();
                auto lock = session->cancel_callbacks_mutex->shared_lock();
                if(*session->cancel_callbacks)
                  return;
                if(!ec)
                  this->find_resource(session);
                else if(this->on_error)
                  this->on_error(session->request, ec);
              });
            }
            else
              this->find_resource(session);
          }
          else
            this->find_resource(session);
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }

    void find_resource(const std::shared_ptr<Session> &session) {
      //Upgrade connection
      if(on_upgrade) {
        auto it = session->request->header.find("Upgrade");
        if(it != session->request->header.end()) {
          on_upgrade(session->connection->socket, session->request);
          return;
        }
      }
      //Find path- and method-match, and call write_response
      for(auto &regex_method : resource) {
        auto it = regex_method.second.find(session->request->method);
        if(it != regex_method.second.end()) {
          regex::smatch sm_res;
          if(regex::regex_match(session->request->path, sm_res, regex_method.first)) {
            session->request->path_match = std::move(sm_res);
            write_response(session, it->second);
            return;
          }
        }
      }
      auto it = default_resource.find(session->request->method);
      if(it != default_resource.end())
        write_response(session, it->second);
    }

    void write_response(const std::shared_ptr<Session> &session,
                        std::function<void(std::shared_ptr<typename ServerBase<socket_type>::Response>, std::shared_ptr<typename ServerBase<socket_type>::Request>)> &resource_function) {
      session->set_timeout(config.timeout_content);
      auto response = std::shared_ptr<Response>(new Response(session), [this](Response *response_ptr) {
        auto response = std::shared_ptr<Response>(response_ptr);
        response->send([this, response](const error_code &ec) {
          if(!ec) {
            if(response->close_connection_after_response)
              return;

            auto range = response->session->request->header.equal_range("Connection");
            for(auto it = range.first; it != range.second; it++) {
              if(case_insensitive_equal(it->second, "close"))
                return;
              else if(case_insensitive_equal(it->second, "keep-alive")) {
                auto new_session = std::make_shared<Session>(response->session->server, response->session->connection);
                this->read_request_and_content(new_session);
                return;
              }
            }
            if(response->session->request->http_version >= "1.1") {
              auto new_session = std::make_shared<Session>(response->session->server, response->session->connection);
              this->read_request_and_content(new_session);
              return;
            }
          }
          else if(this->on_error)
            this->on_error(response->session->request, ec);
        });
      });

      try {
        resource_function(response, session->request);
      }
      catch(const std::exception &e) {
        if(on_error)
          on_error(session->request, make_error_code::make_error_code(errc::operation_canceled));
        return;
      }
    }
  };

  template <class socket_type>
  class Server : public ServerBase<socket_type> {};

  typedef asio::ip::tcp::socket HTTP;

  template <>
  class Server<HTTP> : public ServerBase<HTTP> {
    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

  public:
    Server() : ServerBase<HTTP>::ServerBase(80) {}

  protected:
    void accept() override {
      auto session = std::make_shared<Session>(this, create_connection(new HTTP(*io_service)));

      acceptor->async_accept(*session->connection->socket, [this, session](const error_code &ec) {
        auto lock = session->cancel_callbacks_mutex->shared_lock();
        if(*session->cancel_callbacks)
          return;

        //Immediately start accepting a new connection (unless io_service has been stopped)
        if(ec != asio::error::operation_aborted)
          this->accept();

        if(!ec) {
          asio::ip::tcp::no_delay option(true);
          error_code ec;
          session->connection->socket->set_option(option, ec);

          this->read_request_and_content(session);
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }
  };
} // namespace SimpleWeb

#endif /* SERVER_HTTP_HPP */
