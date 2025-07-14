#pragma once

#include <string>
#include <vector>

bool stringCaseCmp(const std::string &str1, const std::string &str2);
std::vector<std::string> strsplit(const std::string &str, char delim);
inline std::string trim(std::string& str);

struct headerAccept
{
  bool multipart_related = false;
  std::string type;
  std::string transfer_syntax;
  std::string boundary;
};
headerAccept parseHeaderAccept(std::string header_accept);
headerAccept defaultHeaderAccept(std::string header_accept);
