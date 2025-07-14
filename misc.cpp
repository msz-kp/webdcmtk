#include <sstream>
#include "misc.h"

bool stringCaseCmp(const std::string& str1, const std::string& str2) {
    return str1.size() == str2.size() &&
           std::equal(str1.begin(), str1.end(), str2.begin(),
                      [](unsigned char c1, unsigned char c2) {
                          return std::tolower(c1) == std::tolower(c2);
                      });
}

std::vector<std::string> strsplit(const std::string &str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;

    while (getline (ss, item, delim)) {
        result.push_back (item);
    }

    return result;
}

inline std::string trim(std::string& str)
{
    str.erase(str.find_last_not_of(' ') + 1);
    str.erase(0, str.find_first_not_of(' '));
    return str;
}

headerAccept parseHeaderAccept(std::string header_accept) {
  headerAccept ha;
  auto haparts = strsplit(header_accept, ';');
  // multiupart
  if (haparts.size() > 1) {
    for (auto &part : haparts) {
      auto keyval = strsplit(part, '=');
      std::string key = trim(keyval[0]);

      if (key == "multipart/related" && keyval.size() == 1)
        ha.multipart_related = true;
      else if (key == "transfer-syntax" && keyval.size() == 2)
        ha.transfer_syntax = keyval[1];
      else if (key == "boundary" && keyval.size() == 2) {
        std::string str = (trim(keyval[1]));
	str.erase(str.find_last_not_of('"') + 1);
	str.erase(0, str.find_first_not_of('"'));
        ha.boundary = str;
      } else if (key == "type" && keyval.size() == 2) {
        std::string str = (trim(keyval[1]));
	str.erase(str.find_last_not_of('"') + 1);
	str.erase(0, str.find_first_not_of('"'));
        ha.type = str;
      }
    }
  } else {
    ha.type = header_accept;
  }

  return ha;
}

headerAccept defaultHeaderAccept(std::string header_accept) {
  auto hacs = header_accept;
  auto ha = strsplit(hacs, ',').at(0);

  return parseHeaderAccept(ha);
}
