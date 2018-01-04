#pragma once

#include <functional>
#include <vector>
#include <ATen/ATen.h>

namespace torch {

// NOTE: hash_combine is based on implementation from Boost
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

inline std::size_t hash_combine(std::size_t seed, std::size_t value) {
  return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
}

////////////////////////////////////////////////////////////////////////////////
// torch::hash implementation
////////////////////////////////////////////////////////////////////////////////

namespace _hash_detail {

// Use template argument deduction to shorten calls to torch::hash
template<typename T>
std::size_t simple_get_hash(const T& o);

template<typename T, typename V>
using type_if_not_enum = typename std::enable_if<!std::is_enum<T>::value, V>::type;

// Use SFINAE to dispatch to std::hash if possible, cast enum types to int automatically,
// and fall back to T::hash otherwise.
// NOTE: C++14 added support for hashing enum types to the standard, and some compilers
// implement it even when C++14 flags aren't specified. This is why we have to disable
// this overload if T is an enum type (and use the one below in this case).
template<typename T>
auto dispatch_hash(const T& o) -> decltype(std::hash<T>()(o), type_if_not_enum<T, std::size_t>()) {
  return std::hash<T>()(o);
}

template<typename T>
typename std::enable_if<std::is_enum<T>::value, std::size_t>::type dispatch_hash(const T& o) {
  using R = typename std::underlying_type<T>::type;
  return std::hash<R>()(static_cast<R>(o));
}

template<typename T>
auto dispatch_hash(const T& o) -> decltype(T::hash(o), std::size_t()) {
  return T::hash(o);
}

} // namespace _hash_detail

// Hasher struct
template<typename T>
struct hash {
  std::size_t operator()(const T& o) const {
    return _hash_detail::dispatch_hash(o);
  };
};

// Specialization for std::tuple
template<typename... Types>
struct hash<std::tuple<Types...>> {
  template<std::size_t idx, typename... Ts>
  struct tuple_hash {
    std::size_t operator()(const std::tuple<Ts...>& t) const {
      return hash_combine(_hash_detail::simple_get_hash(std::get<idx>(t)),
                          tuple_hash<idx-1, Ts...>()(t));
    }
  };

  template<typename... Ts>
  struct tuple_hash<0, Ts...> {
    std::size_t operator()(const std::tuple<Ts...>& t) const {
      return _hash_detail::simple_get_hash(std::get<0>(t));
    }
  };

  std::size_t operator()(const std::tuple<Types...>& t) const {
    return tuple_hash<sizeof...(Types)-1, Types...>()(t);
  }
};

// Specialization for std::vector
template<typename T>
struct hash<std::vector<T>> {
  std::size_t operator()(const std::vector<T>& v) const {
    std::size_t seed = 0;
    for (const auto & elem : v) {
      seed = hash_combine(seed, _hash_detail::simple_get_hash(elem));
    }
    return seed;
  }
};

namespace _hash_detail {

template<typename T>
std::size_t simple_get_hash(const T& o) {
  return torch::hash<T>()(o);
}

} // namespace _hash_detail

// Use this function to actually hash multiple things in one line.
// Dispatches to torch::hash, so it can hash containers.
// Example:
//
// static std::size_t hash(const MyStruct& s) {
//   return get_hash(s.member1, s.member2, s.member3);
// }
template<typename... Types>
std::size_t get_hash(const Types&... args) {
  return torch::hash<decltype(std::tie(args...))>()(std::tie(args...));
}

} // namespace torch
