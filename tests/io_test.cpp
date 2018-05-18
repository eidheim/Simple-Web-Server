#include "client_http.hpp"
#include "server_http.hpp"

#include <cassert>

using namespace std;

#ifndef USE_STANDALONE_ASIO
namespace asio = boost::asio;
#endif

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;
using HttpClient = SimpleWeb::Client<SimpleWeb::HTTP>;

int main() {
  // Test ScopeRunner
  {
    SimpleWeb::ScopeRunner scope_runner;
    std::thread cancel_thread;
    {
      assert(scope_runner.count == 0);
      auto lock = scope_runner.continue_lock();
      assert(lock);
      assert(scope_runner.count == 1);
      {
        auto lock = scope_runner.continue_lock();
        assert(lock);
        assert(scope_runner.count == 2);
      }
      assert(scope_runner.count == 1);
      cancel_thread = thread([&scope_runner] {
        scope_runner.stop();
        assert(scope_runner.count == -1);
      });
      this_thread::sleep_for(chrono::milliseconds(500));
      assert(scope_runner.count == 1);
    }
    cancel_thread.join();
    assert(scope_runner.count == -1);
    auto lock = scope_runner.continue_lock();
    assert(!lock);
    scope_runner.stop();
    assert(scope_runner.count == -1);

    scope_runner.count = 0;

    vector<thread> threads;
    for(size_t c = 0; c < 100; ++c) {
      threads.emplace_back([&scope_runner] {
        auto lock = scope_runner.continue_lock();
        assert(scope_runner.count > 0);
      });
    }
    for(auto &thread : threads)
      thread.join();
    assert(scope_runner.count == 0);
  }

  HttpServer server;
  server.config.port = 8080;

  server.resource["^/string$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->content.string();

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;

    assert(!request->remote_endpoint_address().empty());
    assert(request->remote_endpoint_port() != 0);
  };

  server.resource["^/string/dup$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->content.string();

    // Send content twice, before it has a chance to be written to the socket.
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << (content.length() * 2) << "\r\n\r\n"
              << content;
    response->send();
    *response << content;
    response->send();

    assert(!request->remote_endpoint_address().empty());
    assert(request->remote_endpoint_port() != 0);
  };

  server.resource["^/string2$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    response->write(request->content.string());
  };

  server.resource["^/string3$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream stream;
    stream << request->content.rdbuf();
    response->write(stream);
  };

  server.resource["^/string4$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    response->write(SimpleWeb::StatusCode::client_error_forbidden, {{"Test1", "test2"}, {"tesT3", "test4"}});
  };

  server.resource["^/info$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    stringstream content_stream;
    content_stream << request->method << " " << request->path << " " << request->http_version << " ";
    content_stream << request->header.find("test parameter")->second;

    content_stream.seekp(0, ios::end);

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n"
              << content_stream.rdbuf();
  };

  server.resource["^/work$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
    thread work_thread([response] {
      this_thread::sleep_for(chrono::seconds(5));
      response->write("Work done");
    });
    work_thread.detach();
  };

  server.resource["^/match/([0-9]+)$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    string number = request->path_match[1];
    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << number.length() << "\r\n\r\n"
              << number;
  };

  server.resource["^/header$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    auto content = request->header.find("test1")->second + request->header.find("test2")->second;

    *response << "HTTP/1.1 200 OK\r\nContent-Length: " << content.length() << "\r\n\r\n"
              << content;
  };

  server.resource["^/query_string$"]["GET"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    assert(request->path == "/query_string");
    assert(request->query_string == "testing");
    auto queries = request->parse_query_string();
    auto it = queries.find("Testing");
    assert(it != queries.end() && it->first == "testing" && it->second == "");
    response->write(request->query_string);
  };

  server.resource["^/chunked$"]["POST"] = [](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> request) {
    assert(request->path == "/chunked");

    assert(request->content.string() == "SimpleWeb in\r\n\r\nchunks.");

    response->write("6\r\nSimple\r\n3\r\nWeb\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
  };

  thread server_thread([&server]() {
    // Start server
    server.start();
  });

  this_thread::sleep_for(chrono::seconds(1));

  server.stop();
  server_thread.join();

  server_thread = thread([&server]() {
    // Start server
    server.start();
  });

  this_thread::sleep_for(chrono::seconds(1));

  // Test various request types
  {
    HttpClient client("localhost:8080");
    {
      stringstream output;
      auto r = client.request("POST", "/string", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      assert(output.str() == "A string");
    }

    {
      auto r = client.request("POST", "/string", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      assert(r->content.string() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string2", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      assert(output.str() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string3", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      output << r->content.rdbuf();
      assert(output.str() == "A string");
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string4", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::client_error_forbidden);
      assert(r->header.size() == 3);
      assert(r->header.find("test1")->second == "test2");
      assert(r->header.find("tEst3")->second == "test4");
      assert(r->header.find("content-length")->second == "0");
      output << r->content.rdbuf();
      assert(output.str() == "");
    }

    {
      stringstream output;
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      output << r->content.rdbuf();
      assert(output.str() == "A string");
    }

    {
      // Test rapid calls to Response::send
      stringstream output;
      stringstream content("A string\n");
      auto r = client.request("POST", "/string/dup", content);
      output << r->content.rdbuf();
      assert(output.str() == "A string\nA string\n");
    }

    {
      stringstream output;
      auto r = client.request("GET", "/info", "", {{"Test Parameter", "test value"}});
      output << r->content.rdbuf();
      assert(output.str() == "GET /info 1.1 test value");
    }

    {
      stringstream output;
      auto r = client.request("GET", "/match/123");
      output << r->content.rdbuf();
      assert(output.str() == "123");
    }
    {
      auto r = client.request("POST", "/chunked", "6\r\nSimple\r\n3\r\nWeb\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\n\r\n", {{"Transfer-Encoding", "chunked"}});
      assert(r->content.string() == "SimpleWeb in\r\n\r\nchunks.");
    }
  }
  {
    HttpClient client("localhost:8080");

    HttpClient::Connection *connection;
    {
      // test performing the stream version of the request methods first
      stringstream output;
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      output << r->content.rdbuf();
      assert(output.str() == "A string");
      assert(client.connections.size() == 1);
      connection = client.connections.begin()->get();
    }

    {
      stringstream output;
      auto r = client.request("POST", "/string", "A string");
      output << r->content.rdbuf();
      assert(output.str() == "A string");
      assert(client.connections.size() == 1);
      assert(connection == client.connections.begin()->get());
    }

    {
      stringstream output;
      auto r = client.request("GET", "/header", "", {{"test1", "test"}, {"test2", "ing"}});
      output << r->content.rdbuf();
      assert(output.str() == "testing");
      assert(client.connections.size() == 1);
      assert(connection == client.connections.begin()->get());
    }

    {
      stringstream output;
      auto r = client.request("GET", "/query_string?testing");
      assert(r->content.string() == "testing");
      assert(client.connections.size() == 1);
      assert(connection == client.connections.begin()->get());
    }
  }

  // Test asynchronous requests
  {
    HttpClient client("localhost:8080");
    bool call = false;
    client.request("GET", "/match/123", [&call](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
      assert(!ec);
      stringstream output;
      output << response->content.rdbuf();
      assert(output.str() == "123");
      call = true;
    });
    client.io_service->run();
    assert(call);

    {
      vector<int> calls(100, 0);
      vector<thread> threads;
      for(size_t c = 0; c < 100; ++c) {
        threads.emplace_back([c, &client, &calls] {
          client.request("GET", "/match/123", [c, &calls](shared_ptr<HttpClient::Response> response, const SimpleWeb::error_code &ec) {
            assert(!ec);
            stringstream output;
            output << response->content.rdbuf();
            assert(output.str() == "123");
            calls[c] = 1;
          });
        });
      }
      for(auto &thread : threads)
        thread.join();
      assert(client.connections.size() == 100);
      client.io_service->reset();
      client.io_service->run();
      assert(client.connections.size() == 1);
      for(auto call : calls)
        assert(call);
    }
  }

  // Test concurrent synchronous request calls
  {
    HttpClient client("localhost:8080");
    {
      vector<int> calls(2, 0);
      vector<thread> threads;
      for(size_t c = 0; c < 2; ++c) {
        threads.emplace_back([c, &client, &calls] {
          try {
            auto r = client.request("GET", "/match/123");
            assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
            assert(r->content.string() == "123");
            calls[c] = 1;
          }
          catch(...) {
            assert(false);
          }
        });
      }
      for(auto &thread : threads)
        thread.join();
      assert(client.connections.size() == 1);
      for(auto call : calls)
        assert(call);
    }
  }

  // Test multiple requests through a persistent connection
  {
    HttpClient client("localhost:8080");
    assert(client.connections.size() == 0);
    for(size_t c = 0; c < 5000; ++c) {
      auto r1 = client.request("POST", "/string", "A string");
      assert(SimpleWeb::status_code(r1->status_code) == SimpleWeb::StatusCode::success_ok);
      assert(r1->content.string() == "A string");
      assert(client.connections.size() == 1);

      stringstream content("A string");
      auto r2 = client.request("POST", "/string", content);
      assert(SimpleWeb::status_code(r2->status_code) == SimpleWeb::StatusCode::success_ok);
      assert(r2->content.string() == "A string");
      assert(client.connections.size() == 1);
    }
  }

  // Test multiple requests through new several client objects
  for(size_t c = 0; c < 100; ++c) {
    {
      HttpClient client("localhost:8080");
      auto r = client.request("POST", "/string", "A string");
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      assert(r->content.string() == "A string");
      assert(client.connections.size() == 1);
    }

    {
      HttpClient client("localhost:8080");
      stringstream content("A string");
      auto r = client.request("POST", "/string", content);
      assert(SimpleWeb::status_code(r->status_code) == SimpleWeb::StatusCode::success_ok);
      assert(r->content.string() == "A string");
      assert(client.connections.size() == 1);
    }
  }

  // Test Client client's stop()
  for(size_t c = 0; c < 40; ++c) {
    auto io_service = make_shared<asio::io_service>();
    bool call = false;
    HttpClient client("localhost:8080");
    client.io_service = io_service;
    client.request("GET", "/work", [&call](shared_ptr<HttpClient::Response> /*response*/, const SimpleWeb::error_code &ec) {
      call = true;
      assert(ec);
    });
    thread thread([io_service] {
      io_service->run();
    });
    this_thread::sleep_for(chrono::milliseconds(100));
    client.stop();
    this_thread::sleep_for(chrono::milliseconds(100));
    thread.join();
    assert(call);
  }

  // Test Client destructor that should cancel the client's request
  for(size_t c = 0; c < 40; ++c) {
    auto io_service = make_shared<asio::io_service>();
    {
      HttpClient client("localhost:8080");
      client.io_service = io_service;
      client.request("GET", "/work", [](shared_ptr<HttpClient::Response> /*response*/, const SimpleWeb::error_code & /*ec*/) {
        assert(false);
      });
      thread thread([io_service] {
        io_service->run();
      });
      thread.detach();
      this_thread::sleep_for(chrono::milliseconds(100));
    }
    this_thread::sleep_for(chrono::milliseconds(100));
  }

  server.stop();
  server_thread.join();

  // Test server destructor
  {
    auto io_service = make_shared<asio::io_service>();
    bool call = false;
    bool client_catch = false;
    {
      HttpServer server;
      server.config.port = 8081;
      server.io_service = io_service;
      server.resource["^/test$"]["GET"] = [&call](shared_ptr<HttpServer::Response> response, shared_ptr<HttpServer::Request> /*request*/) {
        call = true;
        thread sleep_thread([response] {
          this_thread::sleep_for(chrono::seconds(5));
          response->write(SimpleWeb::StatusCode::success_ok, "test");
          response->send([](const SimpleWeb::error_code & /*ec*/) {
            assert(false);
          });
        });
        sleep_thread.detach();
      };
      server.start();
      thread server_thread([io_service] {
        io_service->run();
      });
      server_thread.detach();
      this_thread::sleep_for(chrono::seconds(1));
      thread client_thread([&client_catch] {
        HttpClient client("localhost:8081");
        try {
          auto r = client.request("GET", "/test");
          assert(false);
        }
        catch(...) {
          client_catch = true;
        }
      });
      client_thread.detach();
      this_thread::sleep_for(chrono::seconds(1));
    }
    this_thread::sleep_for(chrono::seconds(5));
    assert(call);
    assert(client_catch);
    io_service->stop();
  }
}
