// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <memory>

#include "headset_interface.hpp"
#include "headset_spec.hpp"

#ifndef WUMBO_PUBLIC
#if defined _WIN32 || defined __CYGWIN__
#define WUMBO_PUBLIC __declspec(dllimport)
#else
#define WUMBO_PUBLIC
#endif
#endif

namespace wmr {

WUMBO_PUBLIC std::shared_ptr<HeadsetInterface> CreateHeadset(const HeadsetSpec& spec);

}  // namespace wmr
