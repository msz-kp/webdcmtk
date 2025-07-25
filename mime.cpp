/*

 forked again!

 ______________________________________________________________________
|            _                          _ _   _                  _     |
|           | |                        | | | (_)                | |    |
|   ___ _ __| |_   __   _ __ ___  _   _| | |_ _ _ __   __ _ _ __| |_   | Multipart parser library C++
|  / _ \ '__| __| |__| | '_ ` _ \| | | | | __| | '_ \ / _` | '__| __|  | Forked and modified from https://github.com/iafonov/multipart-parser-c
| |  __/ |  | |_       | | | | | | |_| | | |_| | |_) | (_| | |  | |_   | Version 1.0.z
|  \___|_|   \__|      |_| |_| |_|\__,_|_|\__|_| .__/ \__,_|_|   \__|  | https://github.com/testillano/multipart
|                                              | |                     |
|                                              |_|                     |
|______________________________________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2021 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <string.h>

#include "mime.h"

#define NOTIFY_CB(FOR)                     \
  do {                                     \
    if (p->settings->on_##FOR) {           \
      if (p->settings->on_##FOR(p) != 0) { \
        return i;                          \
      }                                    \
    }                                      \
  } while (0)

#define EMIT_DATA_CB(FOR, ptr, len)                  \
  do {                                               \
    if (p->settings->on_##FOR) {                     \
      if (p->settings->on_##FOR(p, ptr, len) != 0) { \
        return i;                                    \
      }                                              \
    }                                                \
  } while (0)

#define LF 10
#define CR 13

namespace multipart {

struct multipart_parser {
  void *data;

  size_t index;
  size_t boundary_length;

  unsigned char state;

  const multipart_parser_settings *settings;

  char *lookbehind;
  char multipart_boundary[1];
};

enum state {
  s_uninitialized = 1,
  s_start,
  s_start_boundary,
  s_header_field_start,
  s_header_field,
  s_headers_almost_done,
  s_header_value_start,
  s_header_value,
  s_header_value_almost_done,
  s_part_data_start,
  s_part_data,
  s_part_data_almost_boundary,
  s_part_data_boundary,
  s_part_data_almost_end,
  s_part_data_end,
  s_part_data_final_hyphen,
  s_end
};

multipart_parser *multipart_parser_init(const char *boundary, const multipart_parser_settings *settings) {

  size_t boundary_length = strlen(boundary) + 2; // boundary must be prefixed by "--"

  multipart_parser *p = (multipart_parser *)malloc(sizeof(multipart_parser) + 2 * boundary_length + 9);

  strcpy(p->multipart_boundary, "--");
  strcpy(p->multipart_boundary + 2, boundary);
  p->boundary_length = boundary_length;

  p->lookbehind = (p->multipart_boundary + p->boundary_length + 1);

  p->index = 0;
  p->state = s_start;
  p->settings = settings;

  return p;
}

void multipart_parser_free(multipart_parser *p) {
  free(p);
}

void multipart_parser_set_data(multipart_parser *p, void *data) {
  p->data = data;
}

void *multipart_parser_get_data(multipart_parser *p) {
  return p->data;
}

size_t multipart_parser_execute(multipart_parser *p, const char *buf, size_t len) {
  size_t i = 0;
  size_t mark = 0;
  char c, cl;
  int is_last = 0;

  while (i < len) {
    c = buf[i];
    is_last = (i == (len - 1));
    switch (p->state) {
    case s_start:
      p->index = 0;
      p->state = s_start_boundary;

    // fallthrough
    case s_start_boundary:
      if (p->index == p->boundary_length) {
        if (c != CR) {
          return i;
        }
        p->index++;
        break;
      } else if (p->index == (p->boundary_length + 1)) {
        if (c != LF) {
          return i;
        }
        p->index = 0;
        NOTIFY_CB(part_data_begin);
        p->state = s_header_field_start;
        break;
      }
      if (c != p->multipart_boundary[p->index]) {
        return i;
      }
      p->index++;
      break;

    case s_header_field_start:
      mark = i;
      p->state = s_header_field;

    // fallthrough
    case s_header_field:
      if (c == CR) {
        p->state = s_headers_almost_done;
        break;
      }

      if (c == ':') {
        EMIT_DATA_CB(header_field, buf + mark, i - mark);
        p->state = s_header_value_start;
        break;
      }

      cl = tolower(c);
      if ((c != '-') && (cl < 'a' || cl > 'z')) {
        return i;
      }
      if (is_last)
        EMIT_DATA_CB(header_field, buf + mark, (i - mark) + 1);
      break;

    case s_headers_almost_done:
      if (c != LF) {
        return i;
      }

      p->state = s_part_data_start;
      break;

    case s_header_value_start:
      if (c == ' ') {
        break;
      }

      mark = i;
      p->state = s_header_value;

    // fallthrough
    case s_header_value:
      if (c == CR) {
        EMIT_DATA_CB(header_value, buf + mark, i - mark);
        p->state = s_header_value_almost_done;
        break;
      }
      if (is_last)
        EMIT_DATA_CB(header_value, buf + mark, (i - mark) + 1);
      break;

    case s_header_value_almost_done:
      if (c != LF) {
        return i;
      }
      p->state = s_header_field_start;
      break;

    case s_part_data_start:
      NOTIFY_CB(headers_complete);
      mark = i;
      p->state = s_part_data;

    // fallthrough
    case s_part_data:
      if (c == CR) {
        EMIT_DATA_CB(part_data, buf + mark, i - mark);
        mark = i;
        p->state = s_part_data_almost_boundary;
        p->lookbehind[0] = CR;
        break;
      }
      if (is_last)
        EMIT_DATA_CB(part_data, buf + mark, (i - mark) + 1);
      break;

    case s_part_data_almost_boundary:
      if (c == LF) {
        p->state = s_part_data_boundary;
        p->lookbehind[1] = LF;
        p->index = 0;
        break;
      }
      EMIT_DATA_CB(part_data, p->lookbehind, 1);
      p->state = s_part_data;
      mark = i--;
      break;

    case s_part_data_boundary:
      if (p->multipart_boundary[p->index] != c) {
        EMIT_DATA_CB(part_data, p->lookbehind, 2 + p->index);
        p->state = s_part_data;
        mark = i--;
        break;
      }
      p->lookbehind[2 + p->index] = c;
      if ((++p->index) == p->boundary_length) {
        NOTIFY_CB(part_data_end);
        p->state = s_part_data_almost_end;
      }
      break;

    case s_part_data_almost_end:
      if (c == '-') {
        p->state = s_part_data_final_hyphen;
        break;
      }
      if (c == CR) {
        p->state = s_part_data_end;
        break;
      }
      return i;

    case s_part_data_final_hyphen:
      if (c == '-') {
        NOTIFY_CB(body_end);
        p->state = s_end;
        break;
      }
      return i;

    case s_part_data_end:
      if (c == LF) {
        p->state = s_header_field_start;
        NOTIFY_CB(part_data_begin);
        break;
      }
      return i;

    case s_end:
      break;

    default:
      return 0;
    }
    ++i;
  }

  return len;
}

// Consumer class:

std::string Consumer::last_header_name_ = "";

int Consumer::ReadHeaderName(multipart_parser *p, const char *at, size_t length) {
  Consumer *me = (Consumer *)multipart_parser_get_data(p);
  me->last_header_name_.assign(at, length);

  return 0;
}

int Consumer::ReadHeaderValue(multipart_parser *p, const char *at, size_t length) {
  Consumer *me = (Consumer *)multipart_parser_get_data(p);
  me->receiveHeader(last_header_name_, std::string(at, length));

  return 0;
}

int Consumer::ReadData(multipart_parser *p, const char *at, size_t length) {
  Consumer *me = (Consumer *)multipart_parser_get_data(p);
  me->receiveData(at, length);

  return 0;
}

int Consumer::ReadDataEnd(multipart_parser *p) {
  Consumer *me = (Consumer *)multipart_parser_get_data(p);
  me->receiveDataEnd();

  return 0;
}

Consumer::Consumer(const std::string &boundary) {
  memset(&callbacks_, 0, sizeof(multipart_parser_settings));
  callbacks_.on_header_field = ReadHeaderName;
  callbacks_.on_header_value = ReadHeaderValue;
  callbacks_.on_part_data = ReadData;
  callbacks_.on_part_data_end = ReadDataEnd;

  parser_ = multipart_parser_init(boundary.c_str(), &callbacks_);
  multipart_parser_set_data(parser_, this);
}

Consumer::~Consumer() {
  multipart_parser_free(parser_);
}

void Consumer::decode(const char *c, size_t len) {
  multipart_parser_execute(parser_, c, len);
}

} // namespace multipart
