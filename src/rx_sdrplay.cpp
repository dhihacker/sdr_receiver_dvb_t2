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
#include "rx_sdrplay.h"

//-------------------------------------------------------------------------------------------
rx_sdrplay::rx_sdrplay(QObject *parent) : QObject(parent)
{  
    sample_rate = 9200000.0f; // max for 10bit (10000000.0f for 8bit)

    demodulator = new dvbt2_demodulator(level_min, sample_rate);
    thread.setObjectName("dvbt2_demodulator");
    demodulator->moveToThread(&thread);
    connect(&thread, &QThread::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(this, &rx_sdrplay::execute, demodulator, &dvbt2_demodulator::execute);
    thread.start();

    signal = new signal_estimate;
}
//-------------------------------------------------------------------------------------------
rx_sdrplay::~rx_sdrplay()
{
    thread.quit();
    thread.wait();
    delete signal;
}
//-------------------------------------------------------------------------------------------
std::string rx_sdrplay::error (int err)
{
    switch (err) {
       case mir_sdr_Success:
          return "Success";
       case mir_sdr_Fail:
          return "Fail";
       case mir_sdr_InvalidParam:
          return "Invalid parameter";
       case mir_sdr_OutOfRange:
          return "Out of range";
       case mir_sdr_GainUpdateError:
          return "Gain update error";
       case mir_sdr_RfUpdateError:
          return "Rf update error";
       case mir_sdr_FsUpdateError:
          return "Fs update error";
       case mir_sdr_HwError:
          return "Hardware error";
       case mir_sdr_AliasingError:
          return "Aliasing error";
       case mir_sdr_AlreadyInitialised:
          return "Already initialised";
       case mir_sdr_NotInitialised:
          return "Not initialised";
       case mir_sdr_NotEnabled:
          return "Not enabled";
       case mir_sdr_HwVerError:
          return "Hardware Version error";
       case mir_sdr_OutOfMemError:
          return "Out of memory error";
       case mir_sdr_HwRemoved:
          return "Hardware removed";
       default:
          return "Unknown error";
    }
}
//-------------------------------------------------------------------------------------------
mir_sdr_ErrT rx_sdrplay::get(char* &_ser_no, unsigned char &_hw_ver)
{
    mir_sdr_ErrT err;
//    mir_sdr_DebugEnable(1);

    mir_sdr_DeviceT devices[4];
    unsigned int numDevs;
    err = mir_sdr_GetDevices(&devices[0], &numDevs, 4);

    if(err != 0) return err;

    _ser_no = devices[0].SerNo;
    _hw_ver = devices[0].hwVer;
    err = mir_sdr_SetDeviceIdx(0);

    return err;
}
//-------------------------------------------------------------------------------------------
mir_sdr_ErrT rx_sdrplay::init(double _rf_frequence, int _gain_db)
{
    mir_sdr_ErrT err;

    mir_sdr_Uninit();
    err = mir_sdr_DCoffsetIQimbalanceControl(0, 0);

    if(err != 0) return err;

    ch_frequency = _rf_frequence;
    rf_frequency = ch_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 78;
        agc = true;
    }

    double sample_rate_mhz = static_cast<double>(sample_rate) / 1.0e+6;
    double rf_chanel_mhz = static_cast<double>(rf_frequency) / 1.0e+6;
    err = mir_sdr_Init(gain_db, sample_rate_mhz, rf_chanel_mhz,
                                 mir_sdr_BW_8_000, mir_sdr_IF_Zero, &len_out_device);

    if(err != 0) return err;

    reset();

    return err;
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::reset()
{
    signal->reset = false;
    rf_frequency = ch_frequency;
    signal->coarse_freq_offset = 0.0;
    signal->change_frequency = true;
    signal->correct_resample = 0.0;
    if(agc) {
        gain_db = 78;
    }
    signal->gain_offset = 0;
    signal->change_gain = true;
    ptr_i_device_buffer = i_device_buffer;
    ptr_q_device_buffer = q_device_buffer;
    ptr_out = out_a;
    swap_buffer = true;
    blocks = norm_blocks;;
    set_rf_frequency();
    set_gain();

    qDebug() << "rx_sdrplay::reset";
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::set_rf_frequency()
{
    if(!signal->frequency_changed){
        signal->frequency_changed = frequency_changed;
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        frequency_changed = false;
        signal->frequency_changed = false;
        signal->correct_resample = signal->coarse_freq_offset / rf_frequency;
        rf_frequency += signal->coarse_freq_offset;
        mir_sdr_ErrT err = mir_sdr_SetRf(rf_frequency, 1, 0);
        if(err != 0) {
            emit status(err);
        }
        else{
            emit radio_frequency(rf_frequency);
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::set_gain()
{
    if(!signal->gain_changed){
        signal->gain_changed = gain_changed;
    }
    if(agc && signal->change_gain) {
        signal->change_gain = false;
        gain_changed = false;
        signal->gain_changed = false;
        gain_db -= signal->gain_offset;
        mir_sdr_ErrT err = mir_sdr_SetGr(gain_db, 1, 0);
        if(err != 0) {

            emit status(err);

        }
        else{

            emit level_gain(gain_db);

        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::start()
{
    int max_len_out = len_out_device * max_blocks;
    i_device_buffer = new int16_t[max_len_out];
    q_device_buffer = new int16_t[max_len_out];
    ptr_i_device_buffer = i_device_buffer;
    ptr_q_device_buffer = q_device_buffer;
    out_a = new complex[max_len_out];
    out_b = new complex[max_len_out];
    ptr_out = out_a;
    int len_buffer = 0;
    uint32_t first_sample_num;
    int gr_changed = 0;
    int rf_changed = 0;
    int fs_changed = 0;

    done = true;

    while(done) {

        int num_rx_samps = 0;
        int16_t *ptr_i = ptr_i_device_buffer;
        int16_t *ptr_q = ptr_q_device_buffer;

        for(int n = 0; n < blocks; ++n) {

            mir_sdr_ErrT err = mir_sdr_ReadPacket(ptr_i_device_buffer, ptr_q_device_buffer, &first_sample_num,
                                     &gr_changed, &rf_changed, &fs_changed);
            if(err != 0) {

                emit status(err);

            }
            if(rf_changed) {
                rf_changed = 0;
                frequency_changed = true;
            }
            if(gr_changed) {
                gr_changed = 0;
                gain_changed = true;
            }
            num_rx_samps += len_out_device;
            ptr_i_device_buffer += len_out_device;
            ptr_q_device_buffer += len_out_device;

        }

        int gain_offset;
        correct.execute(num_rx_samps, ptr_i, ptr_q, ptr_out, gain_offset);
        len_buffer += num_rx_samps;
        ptr_out += num_rx_samps;

        if(demodulator->mutex->try_lock()) {

            if(signal->reset){
                reset();

                demodulator->mutex->unlock();

                continue;

            }
            // coarse frequency setting
            set_rf_frequency();
            // AGC
            if(gain_offset != 0 && signal->gain_changed){
                signal->gain_offset = gain_offset;
                signal->change_gain = true;
            }
            set_gain();

            if(swap_buffer) {

                emit execute(len_buffer, out_a, signal);

                ptr_out = out_b;
            }
            else {

                emit execute(len_buffer, out_b, signal);

                ptr_out = out_a;
            }
            swap_buffer = !swap_buffer;
            ptr_i_device_buffer = i_device_buffer;
            ptr_q_device_buffer = q_device_buffer;
            len_buffer = 0;
            blocks = norm_blocks;

            demodulator->mutex->unlock();

        }
        else {
            ++blocks;
            int remain = max_len_out - len_buffer;
            int need = len_out_device * blocks;
            if(need > remain){
                len_buffer = 0;
                blocks = norm_blocks;;
                fprintf(stderr, "reset buffer blocks: %d\n", blocks);
                ptr_i_device_buffer = i_device_buffer;
                ptr_q_device_buffer = q_device_buffer;
                if(swap_buffer) {
                    ptr_out = out_a;
                }
                else {
                    ptr_out = out_b;
                }
            }
        }
    }

    mir_sdr_Uninit();
    mir_sdr_ReleaseDeviceIdx();
    delete [] i_device_buffer;
    delete [] q_device_buffer;
    delete [] out_a;
    delete [] out_b;

}
//-------------------------------------------------------------------------------------------
void rx_sdrplay::stop()
{
    done = false;
}
//-------------------------------------------------------------------------------------------
