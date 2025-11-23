/*
 *  Copyright 2025 vladisslav2011 vladisslav2011@gmail.com.
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
#ifndef RX_HACKRF_ONE_H
#define RX_HACKRF_ONE_H

#include <QObject>
#include <QTime>
#include <QtConcurrent/QtConcurrent>
#include <string>
#include <libhackrf/hackrf.h>

#include "DVB_T2/dvbt2_demodulator.h"

class rx_hackrf_one : public QObject
{
    Q_OBJECT
public:
    explicit rx_hackrf_one(QObject* parent = nullptr);
    ~rx_hackrf_one();

    std::string error (int err);
    int get(std::string &_ser_no, std::string &_hw_ver);
    int init(double _rf_frequency, int _gain_db);
    dvbt2_demodulator* demodulator;

signals:
    void execute(int _len_in, short* _i_in, short* _q_in, signal_estimate* signal_);
    void status(int _err);
    void radio_frequency(double _rf);
    void level_gain(int _gain);
    void stop_demodulator();
    void finished();

public slots:
    void start();
    void stop();
    void set_rf_frequency();
    void set_gain(bool force=false);
    void set_gain_2(bool force=false);

private:
    void rx_execute(void *ptr, int nsamples);
    static int callback(hackrf_transfer* transfer);

private:
    QThread* thread;
    signal_estimate* signal;

    int gain_db;
    bool gain_changed;
    double rf_frequency;
    double ch_frequency;
    bool frequency_changed;

    float sample_rate;
    int max_len_out;

    int16_t* buffer_a;
    int16_t* buffer_b;
    int16_t* ptr_buffer;
    int  blocks = 1;
    const int len_out_device = 128*1024;
    const int max_blocks = 256 * 4;
    int len_buffer = 0;
    bool swap_buffer = true;
    
    int64_t rf_bandwidth_hz;
    int64_t sample_rate_hz;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    float frequency_offset = 0.0f;
    bool change_frequency = false;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;
    bool agc = false;
    bool change_gain = false;
    bool done = true;
    int gain_offset = 0;
    uint32_t lan_gain;
    uint32_t vga_gain;
    QFuture<void> future;
    void update();
    
    hackrf_device *device;
    int err;
//    float sample_rate;
//    const int len_out_device = 128*1024;
//    const int max_blocks = 24576 * 2;//12288;//768
//    const int max_blocks = 2048;//12288;//768
//    int blocks = 1;
//    int max_len_out = 0;
//    int len_buffer = 0;

    void reset();
};

#endif // RX_HACKRF_ONE_H
