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
#ifndef RX_AIRSPY_H
#define RX_AIRSPY_H

#include <QObject>
#include <QTimer>
#include <string>

#include "libairspy/src/airspy.h"
#include "DSP/iq_correct.hh"
#include "DVB_T2/dvbt2_demodulator.h"

class rx_airspy : public QObject
{
    Q_OBJECT
public:
    explicit rx_airspy(QObject *parent = nullptr);
    ~rx_airspy();

    std::string error (int err);
    int get(std::string &_ser_no, std::string &_hw_ver);
    int init(uint32_t _rf_frequence_hz, int _gain);
    dvbt2_demodulator *demodulator;


signals:
    void execute(const int _len, complex* _in, signal_estimate* signal_);
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

    uint64_t serials[10];
    struct airspy_device* device = nullptr;

    complex *out_a, *out_b, *ptr_out;
    int  blocks = 1;
    const int max_blocks = 32;
    int len_buffer;
    bool swap_buffer;

    signal_estimate* signal;

    int gain;
    uint32_t rf_frequency;
    uint32_t ch_frequency;
    float sample_rate;
    bool agc = false;

    float frequency_offset;
    bool change_frequency;
    bool frequency_changed;
    clock_t start_wait_frequency_changed;
    clock_t end_wait_frequency_changed;
    int gain_offset;
    bool change_gain;
    bool gain_changed;
    clock_t start_wait_gain_changed;
    clock_t end_wait_gain_changed;

    void reset();
    void set_rf_frequency();
    void set_gain();
    static int rx_callback(airspy_transfer_t* transfer);
    void rx_execute(int _len_out_device, int16_t* _ptr_rx_buffer);
    const int bits = 12;
    const float level_max = 0.04f;
    const float level_min = 0.02f;
    iq_correct<int16_t> correct{bits, level_max, level_min};

};

#endif // RX_AIRSPY_H
