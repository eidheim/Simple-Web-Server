#ifndef CLIENT_HTTPS_HPP
#define CLIENT_HTTPS_HPP

#include "client_http.hpp"

#ifdef USE_STANDALONE_ASIO
#include <asio/ssl.hpp>
#else
#include <boost/asio/ssl.hpp>
#endif

namespace SimpleWeb {
  typedef asio::ssl::stream<asio::ip::tcp::socket> HTTPS;

  template <>
  class Client<HTTPS> : public ClientBase<HTTPS> {
  public:
    friend ClientBase<HTTPS>;

    Client(const std::string &server_port_path, bool verify_certificate = true, const std::string &cert_file = std::string(),
           const std::string &private_key_file = std::string(), const std::string &verify_file = std::string())
        : ClientBase<HTTPS>::ClientBase(server_port_path, 443), context(asio::ssl::context::tlsv12) {
      if(cert_file.size() > 0 && private_key_file.size() > 0) {
        context.use_certificate_chain_file(cert_file);
        context.use_private_key_file(private_key_file, asio::ssl::context::pem);
      }

      if(verify_certificate)
        context.set_verify_callback(asio::ssl::rfc2818_verification(host));

      if(verify_file.size() > 0)
        context.load_verify_file(verify_file);
      else
        context.set_default_verify_paths();

      if(verify_file.size() > 0 || verify_certificate)
        context.set_verify_mode(asio::ssl::verify_peer);
      else
        context.set_verify_mode(asio::ssl::verify_none);
    }

  protected:
    asio::ssl::context context;

    std::shared_ptr<Connection> create_connection() override {
      return std::make_shared<Connection>(host, port, config, std::unique_ptr<HTTPS>(new HTTPS(*io_service, context)));
    }

    static void connect(const std::shared_ptr<Session> &session) {
      if(!session->connection->socket->lowest_layer().is_open()) {
        auto resolver = std::make_shared<asio::ip::tcp::resolver>(*session->io_service);
        resolver->async_resolve(*session->connection->query, [session, resolver](const error_code &ec, asio::ip::tcp::resolver::iterator it) {
          if(!ec) {
            auto timer = get_timeout_timer(session, session->connection->config.timeout_connect);
            asio::async_connect(session->connection->socket->lowest_layer(), it, [session, resolver, timer](const error_code &ec, asio::ip::tcp::resolver::iterator /*it*/) {
              if(timer)
                timer->cancel();
              if(!ec) {
                asio::ip::tcp::no_delay option(true);
                session->connection->socket->lowest_layer().set_option(option);

                if(!session->connection->config.proxy_server.empty()) {
                  auto write_buffer = std::make_shared<asio::streambuf>();
                  std::ostream write_stream(write_buffer.get());
                  auto host_port = session->connection->host + ':' + std::to_string(session->connection->port);
                  write_stream << "CONNECT " + host_port + " HTTP/1.1\r\n"
                               << "Host: " << host_port << "\r\n\r\n";
                  auto timer = get_timeout_timer(session, session->connection->config.timeout_connect);
                  asio::async_write(session->connection->socket->next_layer(), *write_buffer, [session, write_buffer, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                    if(timer)
                      timer->cancel();
                    if(!ec) {
                      std::shared_ptr<Response> response(new Response());
                      auto timer = get_timeout_timer(session, session->connection->config.timeout_connect);
                      asio::async_read_until(session->connection->socket->next_layer(), response->content_buffer, "\r\n\r\n", [session, response, timer](const error_code &ec, size_t /*bytes_transferred*/) {
                        if(timer)
                          timer->cancel();
                        if(!ec) {
                          parse_response_header(response);
                          if(response->status_code.empty() || response->status_code.compare(0, 3, "200") != 0) {
                            close(session);
                            session->callback(make_error_code::make_error_code(errc::permission_denied));
                          }
                          else
                            handshake(session);
                        }
                        else {
                          close(session);
                          session->callback(ec);
                        }
                      });
                    }
                    else {
                      close(session);
                      session->callback(ec);
                    }
                  });
                }
                else
                  handshake(session);
              }
              else {
                close(session);
                session->callback(ec);
              }
            });
          }
          else {
            close(session);
            session->callback(ec);
          }
        });
      }
      else
        write(session);
    }

    static void handshake(const std::shared_ptr<Session> &session) {
      auto timer = get_timeout_timer(session, session->connection->config.timeout_connect);
      session->connection->socket->async_handshake(asio::ssl::stream_base::client, [session, timer](const error_code &ec) {
        if(timer)
          timer->cancel();
        if(!ec)
          write(session);
        else {
          close(session);
          session->callback(ec);
        }
      });
    }
  };
} // namespace SimpleWeb

#endif /* CLIENT_HTTPS_HPP */
