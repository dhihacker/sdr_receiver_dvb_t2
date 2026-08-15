#include "win_usb_plutosdr.h"

#include <QDebug>
#include <chrono>
#include <thread>

// Minimal Windows USB helper skeleton.
// This file provides a small, safe implementation so the project builds on Windows.
// It stores the provided iio_context pointer (so libiio-based operations work)
// and runs a worker thread that can be expanded later to perform real libusb transfers.

static iio_context *g_win_iio_context = nullptr;

win_usb_plutosdr::win_usb_plutosdr()
{
    thread_stop.store(true);
    usb_sdr_dev = nullptr;
    pipe = nullptr;
    usb_sdr_interface_num = 0;
    usb_sdr_ep_in = 0;
    usb_sdr_ep_out = 0;
}

win_usb_plutosdr::~win_usb_plutosdr()
{
    stop();
}

void win_usb_plutosdr::open_sdr_usb_gadget(iio_context* context)
{
    // Store the iio context so other components can use it.
    g_win_iio_context = context;
    qDebug() << "win_usb_plutosdr: open_sdr_usb_gadget called - context stored";

    // A full implementation would locate the device using libusbp/libusb, claim the interface
    // and prepare async endpoints. For now we just store the context as a safe noop.
}

void win_usb_plutosdr::start(unsigned int num_channels, uint32_t buffer_size_samples,
                             usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback)
{
    if (!thread.joinable()) {
        enabled_channels = 0;
        for (unsigned int i = 0; i < num_channels * 2; i++) {
            enabled_channels |= (1 << i);
        }

        tr = transfer;
        cb = &callback;

        thread_stop.store(false);
        thread = std::thread(&win_usb_plutosdr::thread_func, this, enabled_channels, buffer_size_samples,
                             transfer, callback);
    }
}

void win_usb_plutosdr::thread_func(uint32_t curr_enabled_channels, uint32_t curr_buffer_size_samples,
                                   usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback)
{
    qDebug() << "win_usb_plutosdr: worker thread started (skeleton)";

    // This skeleton simply waits until stop() is called. Replace this loop with a real
    // libusbp/libusb transfer loop to stream data from the Pluto device.
    while (!thread_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Optional: For local testing you could simulate samples and call the callback:
        // if (transfer && callback) {
        //     // Example: allocate a temporary buffer and mark transfer fields.
        //     // uint16_t *tmp = new uint16_t[curr_buffer_size_samples * 2];
        //     // transfer->samples = tmp;
        //     // transfer->num_samples = curr_buffer_size_samples;
        //     // callback(transfer);
        //     // delete[] tmp;
        // }
    }

    qDebug() << "win_usb_plutosdr: worker thread exiting (skeleton)";
}

void win_usb_plutosdr::stop()
{
    if (thread.joinable()) {
        thread_stop.store(true);
        thread.join();
    }
}
