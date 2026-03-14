/*
 * https://github.com/pgreenland/pluto-sdr-usb-gadget
 */
#ifndef USB_PLUTOSDR_H
#define USB_PLUTOSDR_H

#include <stdexcept>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#if defined(_WIN32) || defined(__CYGWIN__)
#include "libusb/lazy/libusb.h"
#else
#include <libusb-1.0/libusb.h>
#endif

#include "libplutosdr/lib_iio/iio.h"

typedef struct {
    void* ctx;
    void* samples;
    int num_bytes;
    bool ready;
    std::mutex mutex;
    std::condition_variable conditional;
} usb_plutosdr_transfer;

typedef int (*usb_plutosdr_cb_fn)(usb_plutosdr_transfer* transfer);

class usb_plutosdr
{
public:
    usb_plutosdr();
    void open_sdr_usb_gadget(iio_context *context);
    void start(unsigned int num_channels, uint32_t buffer_size_samples,
               usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback);
    void stop();

private:
    libusb_device_handle* usb_sdr_dev;
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

#endif // USB_PLUTOSDR_H
