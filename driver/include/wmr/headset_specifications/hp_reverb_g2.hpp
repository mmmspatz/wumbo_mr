// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include "../headset_spec.hpp"

namespace wmr {
namespace headset_specifications {

static constexpr HeadsetSpec kHpReverbG2{
    "HP Reverb G2", {0x045e, 0x0659}, {0x045e, 0x0659}, {0x03f0, 0x0580}, 4,    640, 480,
    0x12D400,       0x12d07a,         0x12D060,         0x6000,           0x33,
};

}  // namespace headset_specifications
}  // namespace wmr
