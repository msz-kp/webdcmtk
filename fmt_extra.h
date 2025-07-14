#pragma once

#include <ctime>
#include <optional>

#include "fmt/chrono.h"
#include "fmt/core.h"
#include "fmt/format.h"
#include "fmt/ranges.h"
#include "fmt/std.h"

namespace fmt {
template <typename T>
struct formatter<std::optional<T>> : fmt::formatter<T> {
  template <typename FormatContext>
  auto format(const std::optional<T> &opt, FormatContext &ctx) {
    if (opt) {
      fmt::formatter<T>::format(*opt, ctx);
      return ctx.out();
    }
    return fmt::format_to(ctx.out(), "NULL");
  }
};

template <>
struct formatter<std::tm> : formatter<std::string> {
  auto format(const std::tm &tm, format_context &ctx) const -> decltype(ctx.out()) {
    std::string s;
    s = std::asctime(&tm);
    s.erase(s.find_last_not_of("\n") + 1);
    return formatter<std::string>::format(s, ctx);
  }
};
} // namespace fmt
