#ifndef GCC_12_COMPAT_H
#define GCC_12_COMPAT_H

#ifdef __clang__
#include <format> // IWYU pragma: keep
#else
#if __GNUC__ >= 13
#include <format>
#else
/**
    Compatibility implementation of std::format for GCC 12 and below
*/

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace std {
template <typename... Args>
std::string format(const std::string &fmt, std::string arg, Args... args) {
    auto bracket = std::find(fmt.begin(), fmt.end(), '{');
    if (bracket == fmt.end()) {
        return fmt;
    }
    auto lastFmt = fmt.begin();

    const std::vector<std::decay_t<std::string>> toAppend = {
        std::forward<std::string>(arg), std::forward<std::string>(args)...};

    std::string result;
    auto idx = 0;
    for (; bracket != fmt.end();
         bracket = std::find(bracket + 1, fmt.end(), '{')) {
        if (bracket + 1 == fmt.end()) {
            throw std::runtime_error("Invalid format string");
        }
        if (*(bracket + 1) != '}') {
            throw std::runtime_error("Invalid format string");
        }

        result.append(lastFmt, bracket);
        result.append(toAppend[idx]);

        lastFmt = bracket + 2;
    }

    if (lastFmt != fmt.end()) {
        result.append(lastFmt, fmt.end());
    }
    return result;
}

inline std::string format(const std::string &fmt) { return fmt; }
} // namespace std
#endif

#endif
#endif
