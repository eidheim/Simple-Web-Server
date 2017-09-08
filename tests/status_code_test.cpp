#include "status_code.hpp"
#include <cassert>

using namespace SimpleWeb;


int main() {
  assert(status_code("000 Error") == StatusCode::unknown);
  assert(status_code(StatusCode::unknown) == "");
  assert(status_code("100 Continue") == StatusCode::information_continue);
  assert(status_code(StatusCode::information_continue) == "100 Continue");
  assert(status_code("200 OK") == StatusCode::success_ok);
  assert(status_code(StatusCode::success_ok) == "200 OK");
  assert(status_code("208 Already Reported") == StatusCode::success_already_reported);
  assert(status_code(StatusCode::success_already_reported) == "208 Already Reported");
  assert(status_code("308 Permanent Redirect") == StatusCode::redirection_permanent_redirect);
  assert(status_code(StatusCode::redirection_permanent_redirect) == "308 Permanent Redirect");
  assert(status_code("404 Not Found") == StatusCode::client_error_not_found);
  assert(status_code(StatusCode::client_error_not_found) == "404 Not Found");
  assert(status_code("502 Bad Gateway") == StatusCode::server_error_bad_gateway);
  assert(status_code(StatusCode::server_error_bad_gateway) == "502 Bad Gateway");
  assert(status_code("504 Gateway Timeout") == StatusCode::server_error_gateway_timeout);
  assert(status_code(StatusCode::server_error_gateway_timeout) == "504 Gateway Timeout");
  assert(status_code("511 Network Authentication Required") == StatusCode::server_error_network_authentication_required);
  assert(status_code(StatusCode::server_error_network_authentication_required) == "511 Network Authentication Required");
}
