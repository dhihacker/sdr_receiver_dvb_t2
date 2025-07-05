#include "rx_plutosdr_daemon.h"

#include "libplutosdr/upload_sdrusbgadget.h"

//-----------------------------------------------------------------------------------------------
rx_plutosdr_daemon::rx_plutosdr_daemon(QObject *parent) : QObject(parent)
{
    usb_direct = new usb_plutosdr;
}
//-----------------------------------------------------------------------------------------------
string	rx_plutosdr_daemon::error (int _err)
{
    char err_str[1024];
    iio_strerror(_err, err_str, sizeof(err_str));
    return static_cast<string>(err_str);
}
//-----------------------------------------------------------------------------------------------
int rx_plutosdr_daemon::get(string _ip,string &_ser_no, string &_hw_ver)
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
    err = iio_context_get_attr(context, 0, &key, &value);

    if(err !=0) return err;

    _hw_ver = static_cast<string>(value);
    err = iio_context_get_attr(context, 2, &key, &value);

    if(err !=0) return err;

    _ser_no = static_cast<string>(value);

    return 0;
}
//-----------------------------------------------------------------------------------------------
int rx_plutosdr_daemon::init(uint32_t _rf_frequence_hz, int _gain)
{
    ch_frequency = _rf_frequence_hz;
    rf_frequency = ch_frequency;

    altvoltage0 = iio_device_find_channel(ad9361_phy, "altvoltage0", true);
    err = iio_channel_attr_write_longlong(altvoltage0, "frequency", rf_frequency);

    if(err !=0) return err;

    voltage0 = iio_device_find_channel(ad9361_phy, "voltage0", false);
    sample_rate_hz  = 9200000;
    err = iio_channel_attr_write_longlong(voltage0, "sampling_frequency", sample_rate_hz);

    if(err !=0) return err;

    iio_channel_attr_write(voltage0, "rf_port_select", "A_BALANCED");

    err = iio_channel_attr_write_longlong(voltage0, "rf_bandwidth", 8000000);

    if(err !=0) return err;

    err = iio_channel_attr_write(voltage0, "gain_control_mode", "manual");

//    if(err !=0) return err;

    gain_db = _gain;
    if(gain_db < 0) {
        gain_db = 0;
        agc = true;
    }

    err = iio_channel_attr_write_double(voltage0, "hardwaregain", gain_db);

    if(err !=0) return err;

    err = iio_channel_attr_write_bool(voltage0, "bb_dc_offset_tracking_en", false);

    if(err !=0) return err;

    err = iio_channel_attr_write_bool(voltage0, "quadrature_tracking_en", false);

    if(err !=0) return err;

    err = iio_channel_attr_write_bool(voltage0, "rf_dc_offset_tracking_en", false);

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

    max_len_out = len_out_device * max_blocks / 2;

    i_buffer_a = new int16_t[max_len_out];
    q_buffer_a = new int16_t[max_len_out];
    i_buffer_b = new int16_t[max_len_out];
    q_buffer_b = new int16_t[max_len_out];

    signal = new signal_estimate;

    demodulator = new dvbt2_demodulator(id_plutosdr, static_cast<float>(sample_rate_hz));
    thread = new QThread;
    demodulator->moveToThread(thread);
    connect(this, &rx_plutosdr_daemon::execute, demodulator, &dvbt2_demodulator::execute);
    connect(this, &rx_plutosdr_daemon::stop_demodulator, demodulator, &dvbt2_demodulator::stop);
    connect(demodulator, &dvbt2_demodulator::finished, demodulator, &dvbt2_demodulator::deleteLater);
    connect(demodulator, &dvbt2_demodulator::finished, thread, &QThread::quit, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();

    return 0;
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::start()
{
    transfer = new usb_plutosdr_transfer;
    transfer->ctx = this;
    transfer->num_samples = len_out_device;

    usb_direct->open_sdr_usb_gadget(context);

    reset();

    usb_direct->start(1, len_out_device, transfer, rx_callback);
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::reset()
{
    signal->reset = false;
    rf_frequency = ch_frequency;
    signal->coarse_freq_offset = 0.0;
    signal->change_frequency = true;
    signal->correct_resample = 0.0;
    if(agc) {
        gain_db = 0;
    }
    signal->gain_offset = 0;
    signal->change_gain = true;
    ptr_i_buffer = i_buffer_a;
    ptr_q_buffer = q_buffer_a;
    swap_buffer = true;
    len_buffer = 0;
    blocks = 1;
    set_rf_frequency();
    set_gain();
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::set_rf_frequency()
{
    if(!signal->frequency_changed) {
        end_wait_frequency_changed = clock();
        float mseconds = (end_wait_frequency_changed - start_wait_frequency_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 200) {
            signal->frequency_changed = true;
            emit radio_frequency(rf_frequency);
        }
    }
    if(signal->change_frequency) {
        signal->change_frequency = false;
        signal->correct_resample = signal->coarse_freq_offset / static_cast<double>(rf_frequency);
        rf_frequency += static_cast<uint64_t>(signal->coarse_freq_offset);
        int err = iio_channel_attr_write_longlong(altvoltage0, "frequency", rf_frequency);
        if(err < 0) {
            emit status(err);
        }
        else{
            signal->frequency_changed = false;
            start_wait_frequency_changed = clock();
        }
    }
}
//-------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::set_gain()
{
    if(!signal->gain_changed) {
        end_wait_gain_changed = clock();
        float mseconds = (end_wait_gain_changed - start_wait_gain_changed) /
                         (CLOCKS_PER_SEC / 1000);
        if(mseconds > 70) {
            signal->gain_changed = true;
            emit level_gain(gain_db);
        }
    }
    if(agc && signal->change_gain) {
        signal->change_gain = false;
        gain_db += signal->gain_offset;
        if(gain_db == 71) {
            gain_db = 0;
        }
        int err = iio_channel_attr_write_double(voltage0, "hardwaregain", gain_db);
        if(err < 0) {
            emit status(err);
        }
        else{
            signal->gain_changed = false;
            start_wait_gain_changed = clock();
        }
    }
}
//-----------------------------------------------------------------------------------------------
int rx_plutosdr_daemon::rx_callback(usb_plutosdr_transfer* transfer)
{
    rx_plutosdr_daemon* ctx = static_cast<rx_plutosdr_daemon*>(transfer->ctx);
    ctx->work(transfer->num_samples / 4, static_cast<int16_t*>(transfer->samples));

    return 0;
}
//-----------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::work(uint32_t len_out_device, int16_t* p_dat)
{

    for(uint i = 0; i < len_out_device; ++i) {
        ptr_i_buffer[i] = p_dat[0];
        ptr_q_buffer[i] = p_dat[1];
        p_dat += 2;
    }

    len_buffer += len_out_device;
    ptr_i_buffer += len_out_device;
    ptr_q_buffer += len_out_device;

    if(demodulator->mutex->try_lock()) {

        if(signal->reset){
            reset();

            demodulator->mutex->unlock();

            return;

        }
        // coarse frequency setting
        set_rf_frequency();
        // AGC
        set_gain();

        if(swap_buffer) {
            emit execute(len_buffer, i_buffer_a, q_buffer_a, signal);
            ptr_i_buffer = i_buffer_b;
            ptr_q_buffer = q_buffer_b;
        }
        else {
            emit execute(len_buffer, i_buffer_b, q_buffer_b, signal);
            ptr_i_buffer = i_buffer_a;
            ptr_q_buffer = q_buffer_a;
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
                ptr_i_buffer = i_buffer_a;
                ptr_q_buffer = q_buffer_a;
            }
            else {
                ptr_i_buffer = i_buffer_b;
                ptr_q_buffer = q_buffer_b;
            }
        }
    }
    
}
//-----------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::stop()
{
    emit stop_demodulator();
    if(thread->isRunning()) thread->wait(1000);
    shutdown();
    delete [] i_buffer_a;
    delete [] q_buffer_a;
    delete [] i_buffer_b;
    delete [] q_buffer_b;
    emit finished();
}
//-----------------------------------------------------------------------------------------------
void rx_plutosdr_daemon::shutdown()
{
	if (context != nullptr) iio_context_destroy(context);
}
//-----------------------------------------------------------------------------------------------
