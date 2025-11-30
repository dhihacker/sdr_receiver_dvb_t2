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
#ifndef RX_USRP_H
#define RX_USRP_H

#include <QObject>
#include <QtConcurrent/QtConcurrent>
#include <uhd.h>
#include <uhd/usrp/multi_usrp.hpp>
#include <uhd/convert.hpp>

#include "DSP/iq_correct.hh"
#include "DVB_T2/dvbt2_demodulator.h"

class rx_usrp : public QObject
{
    Q_OBJECT
public:
    explicit rx_usrp(QObject *parent=nullptr);
    ~rx_usrp();

    std::string error(int err);
    int get(std::string &_ser_no, std::string &_hw_ver);
    int init(uint32_t _rf_frequence_hz, int _gain);
    dvbt2_demodulator *demodulator;

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

    signal_estimate* signal;

    bool is_start;
    bool done;
    uhd_usrp_handle device;
    uhd_tune_request_t tune_request;
    uhd_tune_result_t tune_result;

    int gain_db;
    uint32_t rf_frequency;
    uint32_t ch_frequency;
    double sample_rate;
    bool agc = false;


    int16_t* device_buffer;
    complex* out_a;
    complex* out_b;
    complex* ptr_out;
    int  blocks = 1;
    const int len_out_device = 64 * 1024;// * 4;
    const int max_blocks = 8;
    int len_buffer = 0;
    bool swap_buffer = true;

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
    void reset();
    void set_rf_frequency();
    void set_gain();
    const int bits = 15;
    const float level_max = 0.04f;
    const float level_min = 0.02f;
    iq_correct<int16_t> correct{bits, level_max, level_min};

};

#endif // RX_USRP_H
