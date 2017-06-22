#ifndef SIMPLE_WEB_SERVER_UTILITY_HPP
#define SIMPLE_WEB_SERVER_UTILITY_HPP

#include <iostream>
#include <string>
#include <unordered_map>

// TODO when switching to c++14, use [[deprecated]] instead
#ifndef DEPRECATED
#ifdef __GNUC__
#define DEPRECATED __attribute__((deprecated))
#elif defined(_MSC_VER)
#define DEPRECATED __declspec(deprecated)
#else
#define DEPRECATED
#endif
#endif

namespace SimpleWeb {
#ifndef CASE_INSENSITIVE_EQUAL_AND_HASH
#define CASE_INSENSITIVE_EQUAL_AND_HASH
inline bool case_insensitive_equal(const std::string &str1, const std::string &str2) {
  return str1.size() == str2.size() &&
         std::equal(str1.begin(), str1.end(), str2.begin(), [](char a, char b) {
           return tolower(a) == tolower(b);
         });
}
class CaseInsensitiveEqual {
public:
  bool operator()(const std::string &str1, const std::string &str2) const {
    return case_insensitive_equal(str1, str2);
  }
};
// Based on https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x/2595226#2595226
class CaseInsensitiveHash {
public:
  size_t operator()(const std::string &str) const {
    size_t h = 0;
    std::hash<int> hash;
    for(auto c : str)
      h ^= hash(tolower(c)) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};
#endif

typedef std::unordered_multimap<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> CaseInsensitiveMultimap;

/// Percent encoding and decoding
class Percent {
public:
  static std::string decode(const std::string &value) {
    std::string result;

    for(size_t i = 0; i < value.size(); ++i) {
      auto &chr = value[i];
      if(chr == '%' && i + 2 < value.size()) {
        auto hex = value.substr(i + 1, 2);
        auto decoded_chr = static_cast<char>(std::strtol(hex.c_str(), nullptr, 16));
        result += decoded_chr;
        i += 2;
      }
      else if(chr == '+')
        result += ' ';
      else
        result += chr;
    }

    return result;
  }
};

/// Returns query keys with percent-decoded values.
inline CaseInsensitiveMultimap parse_query_string(const std::string &query_string) {
  CaseInsensitiveMultimap result;

  if(query_string.empty())
    return result;

  size_t parameter_pos = 0;
  size_t parameter_end_pos = -1;
  size_t value_pos = -1;
  for(size_t c = 0; c < query_string.size(); ++c) {
    if(query_string[c] == '&') {
      auto parameter = query_string.substr(parameter_pos, (parameter_end_pos == std::string::npos ? c : parameter_end_pos) - parameter_pos);
      if(!parameter.empty()) {
        auto value = value_pos == std::string::npos ? std::string() : query_string.substr(value_pos, c - value_pos);
        result.emplace(std::move(parameter), Percent::decode(value));
      }
      parameter_pos = c + 1;
      parameter_end_pos = -1;
      value_pos = -1;
    }
    else if(query_string[c] == '=') {
      parameter_end_pos = c;
      value_pos = c + 1;
    }
  }
  if(parameter_pos < query_string.size()) {
    auto parameter = query_string.substr(parameter_pos, parameter_end_pos - parameter_pos);
    if(!parameter.empty()) {
      auto value = value_pos >= query_string.size() ? std::string() : query_string.substr(value_pos);
      result.emplace(std::move(parameter), Percent::decode(value));
    }
  }

  return result;
}
} // namespace SimpleWeb

#endif // SIMPLE_WEB_SERVER_UTILITY_HPP
