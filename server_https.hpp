#ifndef SERVER_HTTPS_HPP
#define SERVER_HTTPS_HPP

#include "server_http.hpp"

#ifdef USE_STANDALONE_ASIO
#include <asio/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
#endif

#include <algorithm>
#include <openssl/ssl.h>

namespace SimpleWeb {
  using HTTPS = asio::ssl::stream<asio::ip::tcp::socket>;

  template <>
  class Server<HTTPS> : public ServerBase {
    std::string session_id_context;
    bool set_session_id_context = false;

  public:
    Server(const std::string &cert_file, const std::string &private_key_file, const std::string &verify_file = std::string())
        : ServerBase::ServerBase(443), context(asio::ssl::context::tlsv12) {
      context.use_certificate_chain_file(cert_file);
      context.use_private_key_file(private_key_file, asio::ssl::context::pem);

      if(verify_file.size() > 0) {
        context.load_verify_file(verify_file);
        context.set_verify_mode(asio::ssl::verify_peer | asio::ssl::verify_fail_if_no_peer_cert | asio::ssl::verify_client_once);
        set_session_id_context = true;
      }
    }

    void start() override {
      if(set_session_id_context) {
        // Creating session_id_context from address:port but reversed due to small SSL_MAX_SSL_SESSION_ID_LENGTH
        session_id_context = std::to_string(config.port) + ':';
        session_id_context.append(config.address.rbegin(), config.address.rend());
        SSL_CTX_set_session_id_context(context.native_handle(), reinterpret_cast<const unsigned char *>(session_id_context.data()),
                                       std::min<std::size_t>(session_id_context.size(), SSL_MAX_SSL_SESSION_ID_LENGTH));
      }
      ServerBase::start();
    }

  protected:
    asio::ssl::context context;


  private:
    template <class HTTPS>
    class HttpsConnection: public Connection {
      friend class Server<HTTPS>;
      friend class ServerBase;

      template <typename... Args>
      HttpsConnection(std::shared_ptr<ScopeRunner> handler_runner, Args &&... args) noexcept : Connection(handler_runner), socket(new HTTPS(std::forward<Args>(args)...)) {}

      ~HttpsConnection() {};

      std::unique_ptr<HTTPS> socket; // Socket must be unique_ptr since asio::ssl::stream<asio::ip::tcp::socket> is not movable

      asio::ip::tcp::socket::lowest_layer_type& lowest_layer() override { return socket->lowest_layer(); }
      asio::io_service& get_io_service() override { return socket->get_io_service(); }
    };

    class HttpsResponse: public Response {
      friend class Server<HTTPS>;

      HttpsResponse(std::shared_ptr<Session> session, long timeout_content) noexcept : Response(session, timeout_content) {}

      void async_write(const std::function<void(const error_code &)> &callback) override {
        auto session = this->session;

        asio::async_write(*static_cast<HttpsConnection<HTTPS>*>(session->connection.get())->socket
            , streambuf,
                          [session, callback](const error_code& ec, std::size_t /*bytes_transferred*/) {
                            session->connection->cancel_timeout();
                            auto lock = session->connection->handler_runner->continue_lock();
                            if (!lock)
                              return;
                            if (callback)
                              callback(ec);
                          });
      }
    };

    Response* create_response(std::shared_ptr<Session> session, long timeout_content) override {
      return new HttpsResponse(session, timeout_content);
    }

    template <typename... Args>
    std::shared_ptr<HttpsConnection<HTTPS>> create_connection(Args &&... args) noexcept {
      auto connections = this->connections;
      auto connections_mutex = this->connections_mutex;
      auto connection = std::shared_ptr<HttpsConnection<HTTPS>>(new HttpsConnection<HTTPS>(handler_runner, std::forward<Args>(args)...), [connections, connections_mutex](Connection *connection) {
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


    void accept() override {
      auto connection = create_connection(*io_service, context);

      acceptor->async_accept(connection->lowest_layer(), [this, connection](const error_code &ec) {
        auto lock = connection->handler_runner->continue_lock();
        if(!lock)
          return;

        if(ec != asio::error::operation_aborted)
          this->accept();

        auto session = std::make_shared<Session>(config.max_request_streambuf_size, connection);

        if(!ec) {
          asio::ip::tcp::no_delay option(true);
          error_code ec;
          session->connection->lowest_layer().set_option(option, ec);

          session->connection->set_timeout(config.timeout_request);
          static_cast<HttpsConnection<HTTPS>*>(connection.get())->socket->async_handshake(asio::ssl::stream_base::server, [this, session](const error_code &ec) {
            session->connection->cancel_timeout();
            auto lock = session->connection->handler_runner->continue_lock();
            if(!lock)
              return;
            if(!ec)
              this->read(session);
            else if(this->on_error)
              this->on_error(session->request, ec);
          });
        }
        else if(this->on_error)
          this->on_error(session->request, ec);
      });
    }

    std::function<void(std::unique_ptr<HTTPS> &, std::shared_ptr<typename ServerBase::Request>)> on_upgrade;

    void async_read(std::shared_ptr<Connection> connection, asio::streambuf& streambuf, std::size_t length,
                    std::function<void(const error_code &ec, size_t bytes_transferred)> lambda) {

      asio::async_read(*static_cast<HttpsConnection<HTTPS>*>(connection.get())->socket,
                       streambuf, asio::transfer_exactly(length), lambda);
    }

    void async_read_until(std::shared_ptr<Connection> connection, asio::streambuf& streambuf, const std::string& eol,
                          std::function<void(const error_code &ec, size_t bytes_transferred)> lambda) override {
      asio::async_read_until(*static_cast<HttpsConnection<HTTPS>*>(connection.get())->socket,
                             streambuf, eol, lambda);
    }

    void do_upgrade (const std::shared_ptr<Session>& session) {
      if(on_upgrade) {
        auto it = session->request->header.find("Upgrade");
        if(it != session->request->header.end()) {
          // remove connection from connections
          {
            std::unique_lock<std::mutex> lock(*connections_mutex);
            auto it = connections->find(session->connection.get());
            if(it != connections->end())
              connections->erase(it);
          }

          on_upgrade(static_cast<HttpsConnection<HTTPS>*>(session->connection.get())->socket, session->request);
          return;
        }
      }

    }
  };
} // namespace SimpleWeb

#endif /* SERVER_HTTPS_HPP */
