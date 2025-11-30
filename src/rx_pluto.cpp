#include "rx_pluto.h"

#include "libplutosdr/upload_sdrusbgadget.h"

//-----------------------------------------------------------------------------------------------
rx_pluto::rx_pluto(QObject *parent) : QObject(parent)
{
    usb_direct = new usb_plutosdr;

    sample_rate_hz  = 9200000;

    demodulator = new dvbt2_demodulator(level_min, static_cast<float>(sample_rate_hz));
    thread.setObjectName("dvbt2_demodulator");
    demodulator->moveToThread(&thread);
    connect(&thread, &QThread::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(this, &rx_pluto::execute, demodulator, &dvbt2_demodulator::execute);
    thread.start();

    signal = new signal_estimate;

    int max_len_out = len_out_device * max_blocks;

    out_a = new complex[max_len_out];
    out_b = new complex[max_len_out];
}
//-----------------------------------------------------------------------------------------------
rx_pluto::~rx_pluto()
{
    stop();
    thread.quit();
    thread.wait();
    delete signal;
    delete usb_direct;
    delete[] out_a;
    delete[] out_b;
}
//-----------------------------------------------------------------------------------------------
string	rx_pluto::error (int _err)
{
    char err_str[256];
    iio_strerror(_err, err_str, sizeof(err_str));
    return static_cast<string>(err_str);
}
//-----------------------------------------------------------------------------------------------
int rx_pluto::get(string _ip,string &_ser_no, string &_hw_ver)
{
    upload_sdrusbgadget *sdrusbgadget = new upload_sdrusbgadget;
    sdrusbgadget->upload(_ip);
    delete sdrusbgadget;

    int probe = 5;
    while(probe > 0 && context == nullptr){
        --probe;
        QThread::sleep(1);
        //        context = iio_create_context_from_uri("ip:192.168.2.1");
        context = iio_create_context_from_uri("usb:");
    }

    if(context == nullptr) return -1;
    
    ad9361_phy = iio_context_find_device(context, "ad9361-phy");

    if(ad9361_phy == nullptr) return -1;

    const char* key;
    const char* value;
    int c = 0;
    while(iio_context_get_attr(context, c++, &key, &value) == 0){
        qDebug() << key << value;
    }
    int err;
    err = iio_context_get_attr(context, 0, &key, &value);

    if(err !=0) return err;

    _hw_ver = static_cast<string>(value);
    err = iio_context_get_attr(context, 2, &key, &value);

    if(err !=0) return err;

    _ser_no = static_cast<string>(value);

    return 0;
}
//-----------------------------------------------------------------------------------------------
int rx_pluto::init(uint32_t _rf_frequence_hz, int _gain)
{
    int err;

    err = iio_channel_attr_write_longlong(iio_device_find_channel(ad9361_phy, "altvoltage1", true), "powerdown", 1);

    if(err !=0) return err;

    ch_frequency = _rf_frequence_hz;
    rf_frequency = ch_frequency;

    rx_lo = iio_device_find_channel(ad9361_phy, "altvoltage0", true);
    err = iio_channel_attr_write_longlong(rx_lo, "frequency", rf_frequency);

    if(err !=0) return err;

    rx_channel = iio_device_find_channel(ad9361_phy, "voltage0", false);

    err = iio_channel_attr_write_longlong(rx_channel, "sampling_frequency", sample_rate_hz);

    if(err !=0) return err;

    iio_channel_attr_write(rx_channel, "rf_port_select", "A_BALANCED");

    err = iio_channel_attr_write_longlong(rx_channel, "rf_bandwidth", 8000000);

    if(err !=0) return err;

    gain_db = _gain;
    if(gain_db < 0) {
        gain_db = 0;
        agc = true;
    }
    else{
        agc = false;
    }

    err = iio_channel_attr_write_double(rx_channel, "hardwaregain", gain_db);

    if(err !=0) return err;

    err = iio_channel_attr_write(rx_channel, "gain_control_mode", "manual");

    //    if(err !=0) return err;

    err = iio_channel_attr_write_bool(rx_channel, "bb_dc_offset_tracking_en", false);

    if(err !=0) return err;

    err = iio_channel_attr_write_bool(rx_channel, "quadrature_tracking_en", false);

    if(err !=0) return err;

    err = iio_channel_attr_write_bool(rx_channel, "rf_dc_offset_tracking_en", false);

    if(err !=0) return err;

    /* TX streaming device */
    struct iio_device *cf_ad9361_dds_core_lpc = iio_context_find_device(context, "cf-ad9361-dds-core-lpc");
    if (!cf_ad9361_dds_core_lpc) {
        fprintf(stderr, "Failed to open iio tx dev\n");
    }
    /* Disable all channels TX*/
    uint num_channels_tx = iio_device_get_channels_count(cf_ad9361_dds_core_lpc);
    for (uint i = 0; i < num_channels_tx; i++) {
        iio_channel_disable(iio_device_get_channel(cf_ad9361_dds_core_lpc, i));
    }
    /* RX streaming device */
    cf_ad9361_lpc = iio_context_find_device(context, "cf-ad9361-lpc");
    /* Disable all channels RX*/
    uint num_channels_rx = iio_device_get_channels_count(cf_ad9361_lpc);
    for (uint i = 0; i < num_channels_rx; i++) {
        iio_channel_disable(iio_device_get_channel(cf_ad9361_lpc, i));
    }

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_pluto::start()
{
    reset();
    usb_direct->open_sdr_usb_gadget(context);
    transfer.ctx = this;
    transfer.num_bytes = len_out_device;
    usb_direct->start(1, len_out_device, &transfer, rx_callback);
    work();
}
//-------------------------------------------------------------------------------------------
void rx_pluto::stop()
{
    usb_direct->stop();
    done = false;
    transfer.conditional.notify_all();
    if (context != nullptr) iio_context_destroy(context);
}
//-----------------------------------------------------------------------------------------------
void rx_pluto::reset()
{
    signal->reset = false;
    rf_frequency = ch_frequency;
    signal->coarse_freq_offset = 0.0f;
    signal->change_frequency = true;
    signal->correct_resample = 0.0f;
    if(agc) {
        gain_db = 0;
    }
    signal->gain_offset = 0;
    signal->change_gain = true;
    ptr_out = out_a;
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;
    set_rf_frequency();
    set_gain();

    emit level_gain(gain_db);
    emit radio_frequency(rf_frequency);

    qDebug() << "rx_plutosdr_daemon::reset";
}
//-------------------------------------------------------------------------------------------
void rx_pluto::set_rf_frequency()
{
    if(!signal->frequency_changed) {
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                (CLOCKS_PER_SEC / 1000);
        if(mseconds > 100) {
            signal->frequency_changed = true;

            emit radio_frequency(rf_frequency);

        }
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        signal->correct_resample = signal->coarse_freq_offset / static_cast<double>(rf_frequency);
        rf_frequency += static_cast<uint64_t>(signal->coarse_freq_offset);
        int err = iio_channel_attr_write_longlong(rx_lo, "frequency", rf_frequency);
        if(err < 0) {

            emit status(err);

        }
        else{
            signal->frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
//        qDebug() << "rx_plutosdr_daemon::set_rf_frequency" << rf_frequency;
    }
}
//-------------------------------------------------------------------------------------------
void rx_pluto::set_gain()
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
        if(gain_db > 73) {
            gain_db = 0;
        }
        int err = iio_channel_attr_write_double(rx_channel, "hardwaregain", gain_db);
        if(err < 0) {

            emit status(err);

        }
        else{
            signal->gain_changed = false;
            start_wait_gain_changed = clock();
        }
//        qDebug() << "rx_plutosdr_daemon::set_gain" << gain_db;
    }
}
//-----------------------------------------------------------------------------------------------
int rx_pluto::rx_callback(usb_plutosdr_transfer* transfer)
{
    rx_pluto* ctx = static_cast<rx_pluto*>(transfer->ctx);

    transfer->mutex.lock();

    ctx->num_samples = transfer->num_bytes / 4;
    ctx->ptr_device_buffer = static_cast<int16_t*>(transfer->samples);
    transfer->ready = true;

    transfer->mutex.unlock();
    transfer->conditional.notify_all();

    return 0;
}
//-----------------------------------------------------------------------------------------------
void rx_pluto::work()
{
    done = true;
    std::unique_lock<std::mutex> lock(transfer.mutex);

    while(done){

        transfer.conditional.wait(lock);
        if(transfer.ready){

            transfer.ready = false;
            int gain_offset;
            correct.execute(num_samples, ptr_device_buffer, ptr_out, gain_offset);
            len_buffer += num_samples;

            if(demodulator->mutex->try_lock()) {

                if(signal->reset){
                    reset();

                    demodulator->mutex->unlock();

                    return;

                }
                // coarse frequency setting
                set_rf_frequency();
                // AGC
                if(gain_offset != 0 && signal->gain_changed){
                    signal->gain_offset = gain_offset;
                    signal->change_gain = true;
                }
                set_gain();

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
                if(blocks > max_blocks) {
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
    }
    
}
//-----------------------------------------------------------------------------------------------


