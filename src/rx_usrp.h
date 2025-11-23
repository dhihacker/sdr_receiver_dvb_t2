#ifndef RX_USRP_H
#define RX_USRP_H

#include <QObject>
#include <QtConcurrent/QtConcurrent>
#include <uhd.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/convert.hpp>

#include "DVB_T2/dvbt2_demodulator.h"

class rx_usrp : public QObject
{
    Q_OBJECT
public:
    explicit rx_usrp(QObject *parent = nullptr);
    ~rx_usrp();

    std::string error(int err);
    int get(std::string &_ser_no, std::string &_hw_ver);
    int init(uint32_t _rf_frequence_hz, int _gain);
    dvbt2_demodulator *demodulator;
    void rx_execute(int16_t *_ptr_rx_buffer, int _len_out_device);

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

    uhd::usrp::multi_usrp::sptr device;
    uhd::rx_streamer::sptr rx_streamer;
    uhd::rx_metadata_t metadata;
    int chan = 0;

    int gain_db;
    uint32_t rf_frequency;
    uint32_t ch_frequency;
    double sample_rate;
    bool agc = false;
    bool done = true;

    int16_t* buffer_a;
    int16_t* buffer_b;
    int16_t* ptr_buffer;
    int  blocks = 1;
    const uint len_out_device = 64 * 1024;
    const int max_blocks = 64;
    uint len_buffer = 0;
    bool swap_buffer = true;

    uint64_t max_len_out;

    signal_estimate* signal;

    float frequency_offset = 0.0f;
    bool change_frequency = false;
    bool frequency_changed = true;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;

    int gain_offset = 0;
    bool change_gain = false;
    bool gain_changed = true;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;

    void work(uint32_t len_out_device);
    QFuture<void> future;
    void update();

    void reset();
    void set_rf_frequency();
    void set_gain();


};

#endif // RX_USRP_H
