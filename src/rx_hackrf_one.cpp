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
#include "rx_hackrf_one.h"

#include <iomanip>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>

//----------------------------------------------------------------------------------------------------------------------------
rx_hackrf_one::rx_hackrf_one(QObject *parent) : QObject(parent)
{
    int max_len_out = len_out_device * max_blocks;
    out_a = new complex[max_len_out];
    out_b = new complex[max_len_out];

    sample_rate = 10000000.0f;

    demodulator = new dvbt2_demodulator(level_min, sample_rate);
    thread.setObjectName("dvbt2_demodulator");
    demodulator->moveToThread(&thread);
    connect(&thread, &QThread::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(this, &rx_hackrf_one::execute, demodulator, &dvbt2_demodulator::execute);
    thread.start();

    signal = new signal_estimate;

    is_start = false;

}
//-------------------------------------------------------------------------------------------
rx_hackrf_one::~rx_hackrf_one()
{
    stop();
    thread.quit();
    thread.wait();
    delete signal;
    delete[] out_a;
    delete[] out_b;
}
//-------------------------------------------------------------------------------------------
std::string rx_hackrf_one::error (int err)
{
    switch (err) {
    case HACKRF_SUCCESS:
        return "Success";
    case HACKRF_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case HACKRF_ERROR_NOT_FOUND:
        return "HackRF not found";
    case HACKRF_ERROR_BUSY:
        return "Device busy";
    case HACKRF_ERROR_NO_MEM:
        return "Out of memory error";
    case HACKRF_ERROR_LIBUSB:
        return "Libusb error";
    case HACKRF_ERROR_THREAD:
        return "HackRF thread error";
    case HACKRF_ERROR_STREAMING_THREAD_ERR:
        return "HackRF streaming thread error";
    case HACKRF_ERROR_STREAMING_STOPPED:
        return "HackRF error: streaming stopped";
    case HACKRF_ERROR_STREAMING_EXIT_CALLED:
        return "HackRF error: exit called";
    case HACKRF_ERROR_USB_API_VERSION:
        return "HackRF wrong USB api version";
    case HACKRF_ERROR_OTHER:
        return "HackRF error: other";
    default:
        return "Unknown error";
    }
}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf_one::get(std::string &_ser_no, std::string &_hw_ver)
{
    int err;

    err = hackrf_init();
    if(err != HACKRF_SUCCESS){
        fprintf(stderr, "hackrf_init() failed: %s (%d)\n", hackrf_error_name(static_cast<hackrf_error>(err)), err);

        return err;

    }

    hackrf_device_list_t *list = hackrf_device_list();
    if(list->devicecount < 1){
        fprintf(stderr, "No HackRF boards found.\n");

        return err;

    }

    std::string serial("");
    if(list->serial_numbers[0]){
        serial = list->serial_numbers[0];
        if(serial.length() > 6){
            serial = serial.substr(serial.length() - 6, 6);
        }
    }

    device = NULL;
    err = hackrf_device_list_open(list, 0, &device);
    if(err != HACKRF_SUCCESS){
        fprintf(stderr, "hackrf_open() failed: %s (%d)\n", hackrf_error_name(static_cast<hackrf_error>(err)), err);

        return err;

    }

    uint8_t board_id = BOARD_ID_UNDETECTED;
    uint8_t board_rev = BOARD_REV_UNDETECTED;
    uint32_t supported_platform = 0;
    char version[255 + 1];
    err = hackrf_board_id_read(device, &board_id);
    if(err != HACKRF_SUCCESS){
        fprintf(stderr, "hackrf_board_id_read() failed: %s (%d)\n", hackrf_error_name(static_cast<hackrf_error>(err)), err);

        return err;

    }
    fprintf(stderr,"Board ID Number: %d (%s)\n", board_id, hackrf_board_id_name(static_cast<hackrf_board_id>(board_id)));
    fprintf(stderr,"Board rev: %d (%s)\n", board_id, hackrf_board_rev_name(static_cast<hackrf_board_rev>(board_id)));

    read_partid_serialno_t read_partid_serialno;
    err = hackrf_board_partid_serialno_read(device, &read_partid_serialno);
    if(err != HACKRF_SUCCESS){
        fprintf(stderr, "hackrf_board_partid_serialno_read() failed: %s (%d)\n", hackrf_error_name(static_cast<hackrf_error>(err)), err);

        return err;

    }
    fprintf(stderr, "Part ID Number: 0x%08x 0x%08x\n", read_partid_serialno.part_id[0], read_partid_serialno.part_id[1]);

    err = hackrf_version_string_read(device, &version[0], 255);
    if(err != HACKRF_SUCCESS){
        fprintf(stderr, "hackrf_version_string_read() failed: %s (%d)\n", hackrf_error_name(static_cast<hackrf_error>(err)), err);

        return err;

    }
    fprintf(stderr, "Firmware Version: %s\n", version);

    std::stringstream stream;
    stream << std::hex << read_partid_serialno.part_id[0];
    stream << std::hex << read_partid_serialno.part_id[1];
    _ser_no.append(stream.str());
    _hw_ver.append(hackrf_board_rev_name(static_cast<hackrf_board_rev>(board_id)));

    hackrf_device_list_free(list);

    hackrf_close(device);

    return err;

}
//----------------------------------------------------------------------------------------------------------------------------
int rx_hackrf_one::init(double _rf_frequency, int _gain_db)
{
    rf_frequency = _rf_frequency;
    ch_frequency = _rf_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 0;
        agc = true;
    }
    else{
        agc = false;
    }

    int err = 0;
    err = hackrf_open(&device);

    if(err != HACKRF_SUCCESS) return err;

    err = hackrf_set_sample_rate(device, sample_rate);

    if(err != HACKRF_SUCCESS) return err;

    err = hackrf_set_freq(device, uint64_t(rf_frequency));

    if(err != HACKRF_SUCCESS) return err;

    uint32_t bw = hackrf_compute_baseband_filter_bw(uint32_t(8000000));
    err = hackrf_set_baseband_filter_bandwidth(device, bw);

    if(err != HACKRF_SUCCESS) return err;

    reset();

    return err;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::start()
{
    reset();

    int err = hackrf_start_rx(device, callback, (void*)this);
    if(err == HACKRF_SUCCESS){
        is_start = true;
    }
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::stop()
{
    if(is_start == true){
        hackrf_stop_rx(device);
        future.cancel();
        future.waitForFinished();
    }
    hackrf_close(device);
    hackrf_exit();
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::reset()
{
    signal->reset = false;
    rf_frequency = ch_frequency;
    signal->coarse_freq_offset = 0.0;
    signal->change_frequency = true;
    signal->correct_resample = 0.0;
    if(agc){
        gain_db = 0;
    }
    signal->gain_offset = 0;
    signal->change_gain = true;
    ptr_out = out_a;
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;

    future.waitForFinished();
    future = QtConcurrent::run(this, &rx_hackrf_one::update, gain_db);

    emit level_gain(gain_db);
    emit radio_frequency(rf_frequency);

    qDebug() << "rx_hackrf::reset";
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::set_rf_frequency()
{
    if(!signal->frequency_changed){
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 100) {
            signal->frequency_changed = true;
            emit radio_frequency(rf_frequency);
        }
    }
    if(signal->change_frequency){
        signal->change_frequency = false;
        frequency_changed = false;
        signal->frequency_changed = false;
        signal->correct_resample = signal->coarse_freq_offset / rf_frequency;
        rf_frequency += signal->coarse_freq_offset;
        int err = hackrf_set_freq(device, uint64_t(rf_frequency));
        if(err != 0){
            emit status(err);
        }
        else{
            signal->frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::set_gain(bool force)
{
    if(!signal->gain_changed){
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 20){
            signal->gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if((agc && signal->change_gain) || force){
        signal->change_gain = false;
        gain_changed = false;
        signal->gain_changed = false;
        gain_db += signal->gain_offset;
        int gain = gain_db;
        uint32_t clip_gain = 0;
        int err = 0;
        int amp_enable = 0;
        if(gain >= 12){
            amp_enable = 1;
            gain -= 12;
        }
        err |= hackrf_set_amp_enable(device, amp_enable);
//        fprintf(stderr, "AMP=%d\n", amp_enable);
        if(gain >= 40){
            clip_gain = 40;
        }
        else{
            clip_gain = gain - gain % 8;
        }
        err |= hackrf_set_lna_gain(device, clip_gain);
        gain -= clip_gain;
//        fprintf(stderr, "LNA=%d\n", clip_gain);
        if(gain >= 62){
            clip_gain = 62;
        }
        else{
            clip_gain = gain - gain % 2;
        }
        err |= hackrf_set_vga_gain(device, uint32_t(clip_gain));
//        fprintf(stderr, "VGA=%d\n", clip_gain);
        if(err != 0){

            emit status(err);

        }
        else{
            signal->gain_changed = false;
            start_wait_gain_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::update(int _gain_offset)
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    if(_gain_offset != 0 && signal->gain_changed){
        signal->gain_offset = _gain_offset;
        signal->change_gain = true;
    }
    set_gain();
}
//-------------------------------------------------------------------------------------------
int rx_hackrf_one::callback(hackrf_transfer* transfer)
{
    if(!transfer) return 0;

    uint8_t *ptr = transfer->buffer;
    rx_hackrf_one *ctx = static_cast<rx_hackrf_one*>(transfer->rx_ctx);

    ctx->rx_execute(ptr, transfer->valid_length);

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_hackrf_one::rx_execute(void *in_ptr, int nsamples)
{
    int8_t * ptr_device_buffer = (int8_t*)in_ptr;
    int num_samples = nsamples / 2;
    int gain_offset;
    correct.execute(num_samples, ptr_device_buffer, ptr_out, gain_offset);
    len_buffer += num_samples;

    if(demodulator->mutex->try_lock()) {

        if(signal->reset){
            reset();

            demodulator->mutex->unlock();

            return;

        }

        if(future.isFinished()){
            future = QtConcurrent::run(this, &rx_hackrf_one::update, gain_offset);
        }

        if(swap_buffer){

            emit execute(len_buffer, out_a, signal);

            ptr_out = out_b;
        }
        else{

            emit execute(len_buffer, out_b, signal);

            ptr_out = out_a;
        }
        swap_buffer = !swap_buffer;
        len_buffer = 0;
        blocks = 1;

        demodulator->mutex->unlock();
    }
    else {
        ++blocks;
        if(blocks > max_blocks){
            fprintf(stderr, "reset buffer blocks: %d\n", blocks);
            blocks = 1;
            len_buffer = 0;
            if(swap_buffer) {
                ptr_out = out_a;
            }
            else {
                ptr_out = out_b;
            }
        }
        else{
            ptr_out += num_samples;
        }
    }
}
//-------------------------------------------------------------------------------------------

