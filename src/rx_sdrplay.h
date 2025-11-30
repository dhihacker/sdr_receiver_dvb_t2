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
#ifndef RX_SDRPLAY_H
#define RX_SDRPLAY_H

#include <QObject>
#include <QTime>
#include <string>

#include "sdrplay/mir_sdr.h"
#include "DSP/iq_correct.hh"
#include "DVB_T2/dvbt2_demodulator.h"

class rx_sdrplay : public QObject
{
    Q_OBJECT
public:
    explicit rx_sdrplay(QObject* parent = nullptr);
    ~rx_sdrplay();

    std::string error (int err);
    mir_sdr_ErrT get(char *&_ser_no, unsigned char &_hw_ver);
    mir_sdr_ErrT init(double _rf_frequence, int _gain_db);
    dvbt2_demodulator* demodulator;

signals:
    void execute(int _len, complex* _in, signal_estimate* signal_);
    void status(int _err);
    void radio_frequency(double _rf);
    void level_gain(int _gain);

public slots:
    void start();
    void stop();

private:
    QThread thread;
    signal_estimate* signal;

    int gain_db;
    bool gain_changed;
    double rf_frequency;
    double ch_frequency;
    bool frequency_changed;

    float sample_rate;
    int len_out_device;
    const int max_symbol = FFT_32K + FFT_32K / 4 + P1_LEN;
    const int max_blocks = max_symbol / 384 * 32;
    const int norm_blocks = max_symbol / 384 * 4;
    int  blocks;
    int16_t *i_device_buffer, *q_device_buffer;
    int16_t *ptr_i_device_buffer, *ptr_q_device_buffer;
    complex* out_a, *out_b, *ptr_out;
    bool swap_buffer;
    bool agc;
    bool done;

    void reset();
    void set_rf_frequency();
    void set_gain();
    const int bits = 14;
    const float level_max = 0.04f;
    const float level_min = 0.02f;
    iq_correct<int16_t> correct{bits, level_max, level_min};

};

#endif // RX_SDRPLAY_H
