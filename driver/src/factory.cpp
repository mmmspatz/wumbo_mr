// Copyright Mark H. Spatz 2021-present
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE or copy at
// https://www.boost.org/LICENSE_1_0.txt)

#include <wmr/factory.hpp>

#include "hid_device.hpp"
#include "oasis_hid.hpp"

namespace wmr {

std::unique_ptr<OasisHidInterface> Factory::CreateOasisHid(unsigned short vendor_id,
                                                           unsigned short product_id,
                                                           const wchar_t *serial_number) {
  return std::make_unique<OasisHid>(
      std::make_unique<HidDevice>(vendor_id, product_id, serial_number));
}

}  // namespace wmr