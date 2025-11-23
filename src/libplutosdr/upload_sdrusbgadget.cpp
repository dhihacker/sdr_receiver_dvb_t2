#include "upload_sdrusbgadget.h"

#include <QFile>
#include <chrono>
#include <thread>

#ifdef WIN32
#include <Winsock2.h>
#include "libssh.h"
#else
#include <libssh/libssh.h>
#endif

#include <QDebug>

#define S_IRWXU 0x777

//-------------------------------------------------------------------------------------------
upload_sdrusbgadget::upload_sdrusbgadget()
{

}
//-------------------------------------------------------------------------------------------
void upload_sdrusbgadget::upload(std::string ip)
{
    // return;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    ssh_session session = ssh_new();
    if (session == nullptr) {
        fprintf(stderr, "Unable to create SSH session \n");
        exit(-1);
    }
    std::string login = "root@" + ip;
    ssh_options_set(session, SSH_OPTIONS_HOST, login.c_str() );
    //    ssh_options_set(session, SSH_OPTIONS_HOST, "root@192.168.2.1" );
    int verbosity = SSH_LOG_PROTOCOL;
    ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    int port = 22;
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    int rc;
    rc = ssh_connect(session);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error connecting to localhost: %s\n", ssh_get_error(session));
        exit(-1);
    }
    rc = ssh_userauth_password(session, NULL, "analog");
    if (rc != SSH_AUTH_SUCCESS) {
        fprintf(stderr, "Error authenticating with password: %s\n", ssh_get_error(session));
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }

    ssh_scp scp;
    char* buffer;
    int size_file;

    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, "/tmp");
    if (scp == NULL) {
        fprintf(stderr, "Error allocating scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    rc = ssh_scp_init(scp);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error initializing scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    QFile file_script(":/runme.sh");
    if(file_script.isOpen()) file_script.close();
    if (!file_script.open(QIODevice::ReadOnly)) exit(-1);
    size_file = file_script.size();
    rc = ssh_scp_push_file(scp, "runme.sh", size_file, S_IRWXU);
    if (rc != SSH_OK) {
        file_script.close();
        fprintf(stderr, "Can't open remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    buffer = (char*)malloc(size_file);
    file_script.read(buffer, size_file);
    file_script.close();
    rc = ssh_scp_write(scp, buffer, size_file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, "/etc/init.d");
    if (scp == NULL) {
        fprintf(stderr, "Error allocating scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    rc = ssh_scp_init(scp);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error initializing scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    QFile file_start(":/S24udc");
    if(file_start.isOpen()) file_start.close();
    if (!file_start.open(QIODevice::ReadOnly)) exit(-1);
    size_file = file_start.size();
    rc = ssh_scp_push_file(scp, "S24udc", size_file, S_IRWXU);
    if (rc != SSH_OK) {
        file_start.close();
        fprintf(stderr, "Can't open remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    buffer = (char*)malloc(size_file);
    file_start.read(buffer, size_file);
    file_start.close();
    rc = ssh_scp_write(scp, buffer, size_file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    scp = ssh_scp_new(session, SSH_SCP_WRITE | SSH_SCP_RECURSIVE, "/usr/sbin");
    rc = ssh_scp_init(scp);
    if (rc != SSH_OK) {
        fprintf(stderr, "Error initializing scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }
    if (scp == NULL) {
        fprintf(stderr, "Error allocating scp session: %s\n", ssh_get_error(session));
        ssh_scp_close(scp);
        ssh_scp_free(scp);
        ssh_disconnect(session);
        ssh_free(session);
        exit(-1);
    }

    QFile file_blob(":/sdr_usb_gadget");
    if(file_blob.isOpen()) file_blob.close();
    if (!file_blob.open(QIODevice::ReadOnly)) {
        qDebug() << "!file_blob.open(QIODevice::ReadOnly)";
        exit(-1);
    }
    size_file = file_blob.size();
    rc = ssh_scp_push_file(scp, "sdr_usb_gadget", size_file, S_IRWXU);
    if (rc != SSH_OK) {
        file_blob.close();
        std::string err = ssh_get_error(session);
        size_t pos = err.find("Text file busy");
        if(pos != std::string::npos) return;
        fprintf(stderr, "Can't push file sdr_usb_gadget: %s\n", err.c_str());
        exit(-1);
    }
    buffer = (char*)malloc(size_file);
    file_blob.read(buffer, size_file);
    file_blob.close();
    rc = ssh_scp_write(scp, buffer, size_file);
    if (rc != SSH_OK) {
        fprintf(stderr, "Can't write to remote file: %s\n", ssh_get_error(session));
        exit(-1);
    }
    ssh_scp_close(scp);
    ssh_scp_free(scp);

    ssh_channel channel;

    channel = ssh_channel_new(session);
    if (channel == NULL) exit(-1);
    rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK) {
        ssh_channel_free(channel);
        exit(-1);
    }
    rc = ssh_channel_request_exec(channel, "/tmp/runme.sh");
    if (rc != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        exit(-1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);
    ssh_disconnect(session);
    ssh_free(session);

    // std::this_thread::sleep_for(std::chrono::milliseconds(3 * 1000));

    fprintf(stderr, "PlutoSDR kernel patch: Ok \n");

    //    exit(-1);
}
//-------------------------------------------------------------------------------------------
