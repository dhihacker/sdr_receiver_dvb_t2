#include "scan_usb_device.h"

#include <QDebug>

#if defined(_WIN32) || defined(__CYGWIN__)
#include "libusb.h"
#else
#include <libusb-1.0/libusb.h>
#endif

//------------------------------------------------------------------------------------------------
scan_usb_device::scan_usb_device(QObject *parent) : QObject(parent)
{

}
//------------------------------------------------------------------------------------------------
void scan_usb_device::scan()
{
    libusb_device **devs;
    int r = libusb_init(/*ctx=*/NULL);
    if(r < 0) {
        fprintf(stderr, "scan_usb_device: failed libusb_init");

        return;

    }
    ssize_t cnt = libusb_get_device_list(NULL, &devs);
    if(cnt < 0) {
        libusb_exit(NULL);
        fprintf(stderr, "scan_usb_device: failed libusb_get_device_list");

        return;

    }
    libusb_device *dev;
    int i = 0;
    while((dev = devs[i++]) != NULL) {
        struct libusb_device_descriptor desc;
        int r = libusb_get_device_descriptor(dev, &desc);
        if(r < 0) {
            fprintf(stderr, "scan_usb_device: failed libusb_get_device_descriptor");

            continue;

        }

        emit found(desc.idVendor, desc.idProduct);

    }
    libusb_free_device_list(devs, 1);
    libusb_exit(NULL);
}
//------------------------------------------------------------------------------------------------
