#ifndef WIN_USB_PLUTOSDR_H
#define WIN_USB_PLUTOSDR_H

#include <cstdio>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <vector>

#include "libplutosdr/libusbp/libusbp.hpp"
#include <iostream>
#include <iomanip>

#include "iio.h"

typedef struct {
    void* ctx;
    void* samples;
    int num_samples;
} usb_plutosdr_transfer;

typedef int (*usb_plutosdr_cb_fn)(usb_plutosdr_transfer* transfer);

class win_usb_plutosdr
{
public:
    win_usb_plutosdr();
    ~win_usb_plutosdr();

    void open_sdr_usb_gadget(iio_context *context);
    void start(unsigned int num_channels, uint32_t buffer_size_samples,
               usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback);
    void stop();

private:
    libusbp::generic_handle *usb_sdr_dev;
    libusbp::async_in_pipe *pipe;
    uint8_t usb_sdr_interface_num, usb_sdr_ep_in, usb_sdr_ep_out;
    // channel bitmask
    uint32_t enabled_channels;
    std::thread thread;
    std::atomic<bool> thread_stop;
    usb_plutosdr_transfer *tr;
    usb_plutosdr_cb_fn *cb;

    void thread_func(uint32_t curr_enabled_channels, uint32_t curr_buffer_size_samples,
                     usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback);

};

#endif // WIN_USB_PLUTOSDR_H
