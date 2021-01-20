# libusbcpp
A thin header-only C++ binding for libusb.

## WIP
I wrote most of this at the outset of a different project ([Wumbo MR](https://github.com/mmmspatz/wumbo_mr)) after deciding against using any existing options. As such, only one use case (bulk transfers to/from a single endpoint) has really been tested, and I'm still making changes as I go along. So, while I hope this binding will end up being useful on its own, it's going to live as a subproject in the WMR repo for now.

## Features

### Implements RAII around various resources
For example, in the C API, once you call `libusb_get_device_list` you must remember to eventually call `libusb_free_device_list`. `libusbcpp::DeviceList` does that for you in its destructor.

### Doesn't invent abstractions
Almost all member functions defined in libusbcpp are just wrappers that forward their arguments to C API methods with similar names. The C API methods are grouped into classes by the type of their first argument. For example, `libusb_set_configuration` is exposed by `DeviceHandle::SetConfiguration`, because its first argument is `libusb_device_handle*`.

Beyond this basic organization into classes, there's no further abstraction.

### Allows fallback to the C API
The core classes each have a `ptr()` method that returns the raw pointer they are managing, and the entire C API is available in namespace `libusbcpp::c`.

## Structure
`core.hpp` forward declares all the main classes and defines a short base class for each of them. Each of these base classes is responsible for managing a raw pointer of some kind. For example, `ContextBase` holds a `libusb_context*` upon which its destructor eventually calls `libusb_exit`. The base classes also declare (but don't define) factory member functions for the main classes.

## Quick Example (Opening a Device)
```
#include <libusbcpp/libusbcpp.hpp>

using namespace libusbcpp;

std::shared_ptr<Context> ctx = ContextBase::Create();
std::shared_ptr<DeviceList> device_list = ctx->GetDeviceList();

std::shared_ptr<DeviceHandle> device_handle;
for(std::shared_ptr<Device> device : device_list){
    c::libusb_device_descriptor desc = dev->GetDeviceDescriptor();
    if(desc->idVendor = 0x1234 && desc->idProduct == 0x5678){
        device_handle = device->Open();
        break;
    }
}
```
