#include "rx_usrp.h"

//-------------------------------------------------------------------------------------------
rx_usrp::rx_usrp(QObject *parent) : QObject(parent)
{
    uint64_t max_len_out = len_out_device * max_blocks / 2;
    device_buffer = new int16_t[len_out_device];
    out_a = new complex[max_len_out];
    out_b = new complex[max_len_out];

    sample_rate = 10000000.0;

    demodulator = new dvbt2_demodulator(level_min, sample_rate);
    thread.setObjectName("dvbt2_demodulator");
    demodulator->moveToThread(&thread);
    connect(&thread, &QThread::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(this, &rx_usrp::execute, demodulator, &dvbt2_demodulator::execute);
    thread.start();

    signal = new signal_estimate;

    is_start = false;
}
//-------------------------------------------------------------------------------------------
rx_usrp::~rx_usrp()
{
    stop();
    thread.quit();
    thread.wait();
    delete signal;
    delete[] device_buffer;
    delete[] out_a;
    delete[] out_b;
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

    ch_frequency = _rf_frequency;
    rf_frequency = ch_frequency;
    gain_db = _gain_db;
    if(gain_db < 0) {
        gain_db = 0;
        agc = true;
    }
    else{
        agc = false;
    }

    uhd_error err;
    err = uhd_usrp_make(&device, "uhd,num_recv_frames=128");

    if(err != 0) {
        if(device != nullptr) uhd_usrp_free(&device);

        return err;

    }

    err = uhd_usrp_set_rx_rate(device, sample_rate, 0);

    if(err != 0) return err;

    tune_request.args = (char*)"";
    tune_request.rf_freq_policy  = UHD_TUNE_REQUEST_POLICY_AUTO;
    tune_request.dsp_freq_policy = UHD_TUNE_REQUEST_POLICY_AUTO;
    tune_request.target_freq = rf_frequency;
    err = uhd_usrp_set_rx_freq(device, &tune_request, 0, &tune_result);

    if(err != 0) return err;

    err = uhd_usrp_set_rx_bandwidth(device, 8000000.0, 0);

    if(err != 0) return err;

    err = uhd_usrp_set_rx_gain(device, gain_db, 0, "");

    if(err != 0) return err;

    err = uhd_usrp_set_rx_agc(device, false, 0);

    if(err != 0) return err;

    err = uhd_usrp_set_rx_dc_offset_enabled(device, false, 0);

    if(err != 0) return err;

    err = uhd_usrp_set_rx_iq_balance_enabled(device, false, 0);

    if(err != 0) return err;

    return err;
}
//-------------------------------------------------------------------------------------------
void rx_usrp::start()
{
    is_start = true;
    reset();
    work(len_out_device);
}
//-------------------------------------------------------------------------------------------
void rx_usrp::stop()
{
    if(is_start){
        is_start = false;
        done = false;
    }
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
    ptr_out = out_a;
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;

    // future.waitForFinished();
    // future = QtConcurrent::run(this, &rx_usrp::update, gain_db);

    emit level_gain(gain_db);
    emit radio_frequency(rf_frequency);

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
            double freq;
            uhd_usrp_get_rx_freq(device, 0, &freq);
            uint32_t fq = freq;
            signal->frequency_changed = true;

            emit radio_frequency(fq);

        }
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        signal->correct_resample = signal->coarse_freq_offset / static_cast<float>(rf_frequency);
        rf_frequency += static_cast<uint32_t>(signal->coarse_freq_offset);
        tune_request.target_freq = rf_frequency;
        int err = uhd_usrp_set_rx_freq(device, &tune_request, 0, &tune_result);
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
        if(mseconds > 10) {
            signal->gain_changed = true;

            emit level_gain(gain_db);

        }
    }
    if(agc && signal->change_gain) {
        signal->change_gain = false;

        gain_db += signal->gain_offset;
        if(gain_db > 100) {
            gain_db = 0;
        }
        double gain = gain_db;
        int err =  uhd_usrp_set_rx_gain(device, gain, 0, "");
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
void rx_usrp::update(int _gain_offset)
{
    // coarse frequency setting
    set_rf_frequency();
    // AGC
    if(_gain_offset != 0 && signal->gain_changed){
        signal->gain_offset = _gain_offset;
        signal->change_gain = true;
    }
    // AGC
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_usrp::work(uint32_t len_out_device)
{
    int err = 0;
    uhd_rx_streamer_handle rx_streamer;
    err = uhd_rx_streamer_make(&rx_streamer);
    if(err != 0){
        fprintf(stderr, "rx_usrp::work uhd_rx_streamer_make error %d\n", err);

        return;

    }
    size_t channel = 0;
    uhd_stream_args_t stream_args = {
        .cpu_format = (char*)"sc16",
        .otw_format = (char*)"sc16",
        .args = (char*)"",
        .channel_list = &channel,
        .n_channels = 1
    };
    err = uhd_usrp_get_rx_stream(device, &stream_args, rx_streamer);
    if(err != 0){
        fprintf(stderr, "rx_usrp::work uhd_usrp_get_rx_stream error %d\n", err);

        return;

    }

    uhd_stream_cmd_t stream_cmd;
    stream_cmd.stream_mode = UHD_STREAM_MODE_START_CONTINUOUS;
    stream_cmd.stream_now = true;
    err = uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
    if(err != 0){
        fprintf(stderr, "rx_usrp::work uhd_rx_streamer_issue_stream_cmd error %d\n", err);

        return;

    }
    uhd_rx_metadata_handle metadata;
    err = uhd_rx_metadata_make(&metadata);
    if(err != 0){
        fprintf(stderr, "rx_usrp::work uhd_rx_metadata_make error %d\n", err);

        return;

    }

    size_t num_rx_samps = 0;
    int len_request = len_out_device / 2;
    done  = true;

    while(done){

        void *buffer = device_buffer;

        uhd_rx_streamer_recv(rx_streamer, &buffer, len_request, &metadata, 0.1, false, &num_rx_samps);

        int gain_offset = 0;
        correct.execute(num_rx_samps, device_buffer, ptr_out, gain_offset);
        len_buffer += num_rx_samps;
        // AGC
        if(gain_offset != 0 && signal->gain_changed){
            signal->gain_offset = gain_offset;
            signal->change_gain = true;
        }
        set_gain();

        if(demodulator->mutex->try_lock()) {

            if(signal->reset){
                reset();

                demodulator->mutex->unlock();

                continue;

            }
            // coarse frequency setting
            set_rf_frequency();
            // if(future.isFinished()){
            //     future = QtConcurrent::run(this, &rx_usrp::update, gain_offset);
            // }

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
            if(++blocks > max_blocks) {
                len_buffer = 0;
                blocks = 1;
                fprintf(stderr, "rx_usrp::work reset buffers\n");
                if(swap_buffer) {
                    ptr_out = out_a;
                }
                else {
                    ptr_out = out_b;
                }
            }
            else{
                ptr_out += num_rx_samps;
            }
        }

    }

    stream_cmd.stream_mode = UHD_STREAM_MODE_STOP_CONTINUOUS;
    uhd_rx_streamer_issue_stream_cmd(rx_streamer, &stream_cmd);
    uhd_rx_streamer_free(&rx_streamer);
    uhd_rx_metadata_free(&metadata);
    uhd_usrp_free(&device);

}
//-------------------------------------------------------------------------------------------

