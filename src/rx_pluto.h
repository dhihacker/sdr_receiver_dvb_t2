/*
 *  Copyright 2020 Oleg Malyutin.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#ifndef RX_PLUTO_H
#define RX_PLUTO_H

#include <QObject>

#include <string>

#include "iio.h"

#include "libplutosdr/usb_plutosdr.h"
#include "DSP/iq_correct.hh"
#include "DVB_T2/dvbt2_demodulator.h"

typedef std::string string;

class rx_pluto : public QObject
{
    Q_OBJECT
public:
    explicit rx_pluto(QObject* parent = nullptr);
    ~rx_pluto();

    string error (int _err);
    int get(string _ip, string &_ser_no, string &_hw_ver);
    int init(uint32_t _rf_frequence_hz, int _gain);
    dvbt2_demodulator* demodulator;

signals:
    void execute(int _len, complex* _in, signal_estimate* signal_);
    void status(int _err);
    void radio_frequency(double _rf);
    void level_gain(int _gain);
    void stop_demodulator();
    void finished();

public slots:
    void start();
    void stop();

private:
    QThread thread;

    int64_t sample_rate_hz;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    int64_t rf_frequency;
    int64_t ch_frequency;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;

    struct iio_context* context = nullptr;
    struct iio_device* ad9361_phy = nullptr;
    struct iio_device* cf_ad9361_lpc = nullptr;
    struct iio_channel* rx_lo = nullptr;
    struct iio_channel* rx_channel = nullptr;

    double gain_db;
    bool agc;
    const int len_out_device = 32768 * 2;
    const int max_blocks = 32;
    int blocks;
    int len_buffer;

    complex *out_a, *out_b, *ptr_out;
    bool swap_buffer;

    signal_estimate* signal;
    void reset();
    void set_rf_frequency();
    void set_gain();

    usb_plutosdr *usb_direct;
    usb_plutosdr_transfer transfer;
    std::mutex mutex;
    std::condition_variable conditional;
    uint32_t num_samples;
    int16_t* ptr_device_buffer;
    bool done;

    static int rx_callback(usb_plutosdr_transfer* transfer);
    void work();
    const int bits = 11;
    const float level_max = 0.04f;
    const float level_min = 0.02f;
    iq_correct<int16_t> correct{bits, level_max, level_min};

};

#endif // RX_PLUTO_H
