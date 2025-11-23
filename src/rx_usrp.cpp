#include "rx_usrp.h"

//-------------------------------------------------------------------------------------------
rx_usrp::rx_usrp(QObject *parent) : QObject(parent)
{
    max_len_out = len_out_device * max_blocks;
    buffer_a = new int16_t[max_len_out];
    buffer_b = new int16_t[max_len_out];
}
//-------------------------------------------------------------------------------------------
rx_usrp::~rx_usrp()
{
    delete[] buffer_a;
    delete[] buffer_b;
}
//-------------------------------------------------------------------------------------------
std::string rx_usrp::error(int err)
{
    switch (err) {
    case UHD_ERROR_NONE:
        return "Success";
    case UHD_ERROR_INVALID_DEVICE:
        return "Invalid device";
    case UHD_ERROR_INDEX:
        return "Error index";
    case UHD_ERROR_KEY:
        return "Error key";
    case UHD_ERROR_NOT_IMPLEMENTED:
        return "Not implemented";
    case UHD_ERROR_USB:
        return "Error USB";
    case UHD_ERROR_IO:
        return "Error IO";
    case UHD_ERROR_OS:
        return "Error OS";
    case UHD_ERROR_ASSERTION:
        return "Error assertion";
    case UHD_ERROR_LOOKUP:
        return "Error lookup";
    case UHD_ERROR_TYPE:
        return "Error type";
    case UHD_ERROR_VALUE:
        return "Error value";
    case UHD_ERROR_RUNTIME:
        return "Error runtime";
    case UHD_ERROR_ENVIRONMENT:
        return "Error environment";
    case UHD_ERROR_SYSTEM:
        return "Error system";
    case UHD_ERROR_EXCEPT:
        return "Error exept";
    case UHD_ERROR_BOOSTEXCEPT:
        return "Error boostexept";
    case UHD_ERROR_STDEXCEPT:
        return "Error stdexept";
    case UHD_ERROR_UNKNOWN:
        return "Error unknow";
    default:
        return "Unknown error";
    }
}
//-------------------------------------------------------------------------------------------
int rx_usrp::get(std::string &_ser_no, std::string &_hw_ver)
{
    uhd::device_addr_t hint;
    for (const uhd::device_addr_t &dev : uhd::device::find(hint)){
        _hw_ver = dev.cast< std::string >("type", "");
        _ser_no = dev.cast< std::string >("serial", "");
    }

    if(_hw_ver == "") return -1;

    return 0;
}
//-------------------------------------------------------------------------------------------
int rx_usrp::init(uint32_t _rf_frequency, int _gain_db)
{
    sample_rate = 10000000.0;
    ch_frequency = _rf_frequency;
    rf_frequency = ch_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 0;
        agc = true;
    }

    uhd::device_addr_t device_addr{"uhd"};
    device = uhd::usrp::multi_usrp::make(device_addr);
    device->set_rx_rate(sample_rate, chan);
    device->set_rx_freq(uhd::tune_request_t(rf_frequency), chan);
    device->set_rx_bandwidth(8000000, chan);
    device->set_rx_agc(false, chan);
    device->set_rx_dc_offset(0, chan);
    device->set_rx_iq_balance(0, chan);
    device->set_rx_gain(gain_db, chan);

    float sr = sample_rate;
    demodulator = new dvbt2_demodulator(id_usrp, sr);
    thread = new QThread;
    demodulator->moveToThread(thread);
    connect(this, &rx_usrp::execute, demodulator, &dvbt2_demodulator::execute);
    connect(this, &rx_usrp::stop_demodulator, demodulator, &dvbt2_demodulator::stop);
    connect(demodulator, &dvbt2_demodulator::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(demodulator, &dvbt2_demodulator::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    signal = new signal_estimate;

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::start()
{
    reset();

    work(len_out_device);
}
//-------------------------------------------------------------------------------------------
void rx_usrp::reset()
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
    ptr_buffer = buffer_a;
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;

    future.waitForFinished();
    future = QtConcurrent::run(this, &rx_usrp::update);

    emit level_gain(gain_db);

    qDebug() << "rx_usrp::reset";
}
//-------------------------------------------------------------------------------------------
void rx_usrp::set_rf_frequency()
{
    if(!signal->frequency_changed) {
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 100) {
            signal->frequency_changed = true;
            rf_frequency =device->get_rx_freq(chan);

            emit radio_frequency(rf_frequency);
        }
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        signal->correct_resample = signal->coarse_freq_offset / static_cast<float>(rf_frequency);
        rf_frequency += static_cast<uint32_t>(signal->coarse_freq_offset);
        int err = 0;
        device->set_rx_freq(uhd::tune_request_t(rf_frequency), chan);
        if(err != 0) {
            emit status(err);
        }
        else{
            signal->frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
    }

}
//-------------------------------------------------------------------------------------------
void rx_usrp::set_gain()
{
    if(!signal->gain_changed) {
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                          (CLOCKS_PER_SEC / 1000);
        if(mseconds > 50) {
            signal->gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if(agc && signal->change_gain) {
        signal->change_gain = false;

        gain_db += signal->gain_offset;

        if(gain_db > 78) {
            gain_db = 0;
        }
        double gain = gain_db;
        int err = 0;
        device->set_rx_gain(gain, chan);
        if(err != 0) {
            emit status(err);
        }
        else{
            signal->gain_changed = false;
            start_wait_gain_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_usrp::update()
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_usrp::work(uint32_t len_out_device)
{
    uhd::stream_args_t stream_args("sc16","sc16");
    rx_streamer = device->get_rx_stream(stream_args);

    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    rx_streamer->issue_stream_cmd(stream_cmd);

    const double timeout = 1.0;
    const bool one_packet = false;
    const int len_request = len_out_device / 2;

    while(done){

        size_t num_samples = rx_streamer->recv(ptr_buffer, len_request, metadata, timeout, one_packet);

        len_buffer += num_samples;
        ptr_buffer += num_samples * 2;

        if(demodulator->mutex->try_lock()) {

            if(signal->reset){
                reset();

                demodulator->mutex->unlock();

                continue;

            }
            if(future.isFinished()){
                future = QtConcurrent::run(this, &rx_usrp::update);
            }

            if(swap_buffer) {
                emit execute(len_buffer, &buffer_a[0], &buffer_a[1], signal);
                ptr_buffer = buffer_b;
            }
            else {
                emit execute(len_buffer, &buffer_b[0], &buffer_b[1], signal);
                ptr_buffer = buffer_a;
            }
            swap_buffer = !swap_buffer;
            len_buffer = 0;
            blocks = 1;

            demodulator->mutex->unlock();
        }
        else {
            ++blocks;
            if(blocks > max_blocks) {
                fprintf(stderr, "reset buffer blocks: %d\n", blocks);
                blocks = 1;
                len_buffer = 0;
                if(swap_buffer) {
                    ptr_buffer = buffer_a;
                }
                else {
                    ptr_buffer = buffer_b;
                }
            }
        }
    }

    rx_streamer->issue_stream_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);

}
//-------------------------------------------------------------------------------------------
void rx_usrp::stop()
{
    done = false;

    emit finished();
}
//-------------------------------------------------------------------------------------------
