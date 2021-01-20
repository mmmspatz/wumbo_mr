// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

namespace libusbcpp {
namespace detail {

/** Make a functor out of a unary function with void return. */
template <auto del>
struct DeleteFunctor;

template <class T, void(del)(T)>
struct DeleteFunctor<del> {
  void operator()(T t) { del(t); }
};

}  // namespace detail
}  // namespace libusbcpp
