#include "usb_plutosdr.h"

#include <QDebug>

#define SDR_USB_GADGET_COMMAND_START (0x10)
#define SDR_USB_GADGET_COMMAND_STOP (0x11)
#define SDR_USB_GADGET_COMMAND_TARGET_RX (0x0)
#define SDR_USB_GADGET_COMMAND_TARGET_TX (0x1)

typedef struct{
    uint32_t enabled_channels;
    uint32_t buffer_size;
} cmd_usb_start_request_t;

//------------------------------------------------------------------------------------------------
usb_plutosdr::usb_plutosdr()
{

}
//------------------------------------------------------------------------------------------------
void usb_plutosdr::open_sdr_usb_gadget(iio_context* context)
{
    // retrieve url and separate bus / device
    const char *uri = iio_context_get_attr_value(context, "uri");
    if(!uri) {
        fprintf(stderr, "failed to retrieve uri from iio");
        throw std::runtime_error("failed to retrieve uri from iio");
    }
    // retrieve bus and device number from uri
    unsigned short int bus_num, dev_addr;
    if(2 != std::sscanf(uri, "usb:%hu.%hu", &bus_num, &dev_addr)) {
        fprintf(stderr, "failed to extract usb bus and device address from uri");
        throw std::runtime_error("failed to extract usb bus and device address from uri");
    }

    int rc;
    // init libusb
    libusb_context *usb_ctx = nullptr;
    rc = libusb_init(&usb_ctx);
    if (rc < 0) {
        fprintf(stderr, "libusb init error (%d)", rc);
        throw std::runtime_error("libusb init error");
    }
    // retrieve device list
    struct libusb_device **devs;
    int dev_count = libusb_get_device_list(usb_ctx, &devs);
    if(dev_count < 0) {
        fprintf(stderr, "libusb get device list error (%d)", dev_count);
        throw std::runtime_error("libusb get device list error");
    }
    // iterate over devices
    for(int i = 0; i < dev_count; i++) {
        struct libusb_device *dev = devs[i];
        // check device bus and address
        if((libusb_get_bus_number(dev) == bus_num) && (libusb_get_device_address(dev) == dev_addr)) {
                // found device, open it
                int rc = libusb_open(dev, &this->usb_sdr_dev);
                if(rc < 0) {
                    // Failed to open device
                    fprintf(stderr, "libusb failed to open device (%d)\n", rc);
                    this->usb_sdr_dev = nullptr;
                }

                break;

        }
    }
    // free list, reducing device reference counts
    libusb_free_device_list(devs, 1);
    // check handle
    if(!this->usb_sdr_dev) {
        fprintf(stderr, "failed to open sdr_usb_gadget\n");
        throw std::runtime_error("failed to open sdr_usb_gadget");
    }
    // retrieve active config descriptor
    struct libusb_config_descriptor *config;
    rc = libusb_get_active_config_descriptor(libusb_get_device(this->usb_sdr_dev), &config);
    if(rc < 0) {
        fprintf(stderr, "failed to get usb device descriptor (%d)", rc);
        throw std::runtime_error("failed to get usb device descriptor");
    }

    // loop through interfaces and find one with the desired name
    int interface_num = -1;
    for (int i = 0; i < config->bNumInterfaces; i++) {
        const struct libusb_interface *iface = &config->interface[i];
        for (int j = 0; j < iface->num_altsetting; j++) {
            const struct libusb_interface_descriptor *desc = &iface->altsetting[j];
            // get the interface name
            char name[128];
            rc = libusb_get_string_descriptor_ascii(this->usb_sdr_dev, desc->iInterface, (unsigned char*)name, sizeof(name));
            if (rc < 0) {
                fprintf(stderr, "failed to get usb device interface name (%d)", rc);
                throw std::runtime_error("failed to get usb device interface name");
            }

            if (0 == strcmp(name, "sdrgadget")) {
                // capture interface number
                interface_num = desc->bInterfaceNumber;
                // Capture endpoint addresses
                for (int k = 0; k < desc->bNumEndpoints; k++) {
                    const struct libusb_endpoint_descriptor *ep_desc = &desc->endpoint[k];
                    if(ep_desc->bEndpointAddress & 0x80) {
                        this->usb_sdr_ep_in = ep_desc->bEndpointAddress;
                    }
                    else {
                        this->usb_sdr_ep_out = ep_desc->bEndpointAddress;
                    }
                }
                // all done
                break;

            }
        }
    }

    // free the configuration descriptor
    libusb_free_config_descriptor(config);
    config = nullptr;
    if(interface_num < 0) {
        fprintf(stderr, "failed to find usb device interface");
        throw std::runtime_error("failed to find usb device interface");
    }
    // store interface number
    this->usb_sdr_interface_num = (uint8_t)interface_num;
    // claim the interface
    rc = libusb_claim_interface(this->usb_sdr_dev, this->usb_sdr_interface_num);
    if(rc < 0) {
        fprintf(stderr, "failed to claim usb device interface (%d)", rc);
        throw std::runtime_error("failed to claim usb device interface");
    }
}
//------------------------------------------------------------------------------------------------
void usb_plutosdr:: start(unsigned int num_channels, uint32_t buffer_size_samples,
                          usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback)
{
    if(!thread.joinable()) {

        enabled_channels = 0;
        for (unsigned int i = 0; i < num_channels * 2; i++) {
            // add bit to enabled channels
            enabled_channels |= (1 << i);
        }

        thread_stop = false;
        thread = std::thread(&usb_plutosdr::thread_func, this, enabled_channels, buffer_size_samples,
                             transfer, callback);

        // // attempt to increase thread priority
        // int max_prio = sched_get_priority_max(SCHED_RR);
        // if(max_prio >= 0) {
        //     sched_param sch;
        //     sch.sched_priority = max_prio;
        //     if(int rc = pthread_setschedparam(thread.native_handle(), SCHED_RR, &sch)) {
        //         fprintf(stderr, "Failed to set RX thread priority (%d)\n", rc);
        //     }
        // }
        // else {
        //     fprintf(stderr, "Failed to query thread schedular priorities\n");
        // }
    }
}
//------------------------------------------------------------------------------------------------
void usb_plutosdr::thread_func(uint32_t curr_enabled_channels, uint32_t curr_buffer_size_samples,
                               usb_plutosdr_transfer *transfer, usb_plutosdr_cb_fn callback)
{
    cmd_usb_start_request_t cmd;
    size_t curr_buffer_size_bytes;

    // start stream
    cmd.enabled_channels = curr_enabled_channels;
    cmd.buffer_size = curr_buffer_size_samples;

    int rc = libusb_control_transfer(usb_sdr_dev,
                                     LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
                                     SDR_USB_GADGET_COMMAND_START,
                                     SDR_USB_GADGET_COMMAND_TARGET_RX,
                                     usb_sdr_interface_num,
                                     (unsigned char*)&cmd,
                                     sizeof(cmd),
                                     1000);
    if(rc < 0) {
        fprintf(stderr, "Failed to start RX stream (%d)", rc);
        return;
    }

    // calculate buffer size in bytes
    uint32_t sample_size_bytes = /*curr_enabled_channels * */sizeof(uint16_t);
    curr_buffer_size_bytes = curr_buffer_size_samples * sample_size_bytes;

    // allocate buffer
    std::shared_ptr<std::vector<uint8_t>> buffer = std::make_shared<std::vector<uint8_t>>();
    buffer->resize(curr_buffer_size_bytes);

    // keep running until told to stop
    size_t buffers_to_drop = 32; // ensure anything kicking around in the queues and buffers is dropped
    while(!thread_stop.load()) {
        // read data with 1s timeout
        int bytes_transferred = 0;
        int rc = libusb_bulk_transfer(usb_sdr_dev, usb_sdr_ep_in, buffer->data(), buffer->size(),
                                      &bytes_transferred, 3000);

        if(LIBUSB_SUCCESS == rc && ((size_t)bytes_transferred == buffer->size())) {
            // transfer complete with expected size
            if(0 == buffers_to_drop) {
                // push buffer into fifo without blocking
                transfer->samples = buffer->data();
                transfer->num_samples = buffer->size();
                callback(transfer);
            }
            else {
                // still within initial drop range, discard buffer
                buffers_to_drop--;
            }


            // create new buffer
            buffer = std::make_shared<std::vector<uint8_t>>();
            buffer->resize(curr_buffer_size_bytes);
        }
    }

    // stop stream
    rc = libusb_control_transfer(usb_sdr_dev,
                                 LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_INTERFACE,
                                 SDR_USB_GADGET_COMMAND_STOP,
                                 SDR_USB_GADGET_COMMAND_TARGET_RX,
                                 usb_sdr_interface_num,
                                 nullptr,
                                 0,
                                 1000);
    if(rc < 0) {
        fprintf(stderr, "Failed to stop RX stream (%d)", rc);
        return;
    }
}
//------------------------------------------------------------------------------------------------
void usb_plutosdr::stop()
{
    if(thread.joinable()) {
        thread_stop = true;
        thread.join();
    }
}
//------------------------------------------------------------------------------------------------
