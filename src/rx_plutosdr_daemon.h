#ifndef RX_PLUTOSDR_DAEMON_H
#define RX_PLUTOSDR_DAEMON_H

#include <QObject>

#include <string>

#include "iio.h"

#include "libplutosdr/usb_plutosdr.h"
#include "DVB_T2/dvbt2_demodulator.h"

typedef std::string string;

class rx_plutosdr_daemon : public QObject
{
    Q_OBJECT
public:
    explicit rx_plutosdr_daemon(QObject* parent = nullptr);

    string error (int _err);
    int get(string _ip, string &_ser_no, string &_hw_ver);
    int init(uint32_t _rf_frequence_hz, int _gain);
    dvbt2_demodulator* demodulator;

signals:
    void execute(int _len_in, int16_t* _i_in, int16_t* _q_in, signal_estimate* signal_);
    void status(int _err);
    void radio_frequency(double _rf);
    void level_gain(int _gain);
    void stop_demodulator();
    void finished();

public slots:
    void start();
    void stop();

private:
    QThread* thread;

    int64_t rf_bandwidth_hz;
    int64_t sample_rate_hz;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    int64_t rf_frequency;
    int64_t ch_frequency;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;
    const char* port_name;
    struct iio_context* context = nullptr;
    struct iio_device* ad9361_phy = nullptr;
    struct iio_device* cf_ad9361_lpc = nullptr;
    struct iio_channel* altvoltage0 = nullptr;
    struct iio_channel* voltage0 = nullptr;
    struct iio_channel* i_channel = nullptr;
    struct iio_channel* q_channel = nullptr;
    struct iio_buffer* rx_buffer = nullptr;
    int err;
    double gain_db;
    bool agc = false;
    const int len_out_device = 32768 * 2;
    const int max_blocks = 32 * 8;
    uint32_t max_len_out;
    int blocks = 1;
    int len_buffer = 0;
    int16_t *i_buffer_a, *q_buffer_a;
    int16_t *i_buffer_b, *q_buffer_b;
    int16_t *ptr_i_buffer, *ptr_q_buffer;

    bool swap_buffer = true;

    signal_estimate* signal;

    bool done = true;

    void reset();
    void set_rf_frequency();
    void set_gain();
    void shutdown();
    usb_plutosdr *usb_direct;
    usb_plutosdr_transfer* transfer;
    static int rx_callback(usb_plutosdr_transfer* transfer);
    void work(uint32_t len_out_device, int16_t *p_dat);

};

#endif // RX_PLUTOSDR_DAEMON_H
