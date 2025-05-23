#include "defines.h"
#include <stdio.h>

#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdexcept>
#include <cassert>
#include "circular_buffer.h"
#include "cast.h"
#include "codecs/codec_flac.h"
#include <signal.h>

#define NET_BUFFER_BYTES 65536

static int is_icecast_initialized = 0;

fmice_icecast::fmice_icecast(int channels, int sampRate, fmice_codec* codec) {
    //Set parameters
    this->channels = channels;
    this->sample_rate = sampRate;
    this->codec = codec;

    //Init mutex
    if (pthread_mutex_init(&mutex, NULL) != 0)
        throw new std::runtime_error("Failed to initialize mutex.");

    //Clear setup/stat vars
    memset(icecast_host, 0, sizeof(icecast_host));
    icecast_port = 0;
    memset(icecast_mount, 0, sizeof(icecast_mount));
    memset(icecast_username, 0, sizeof(icecast_username));
    memset(icecast_password, 0, sizeof(icecast_password));
    stat_status = FMICE_ICECAST_STATUS_INIT;
    stat_retries = 0;

    //Init global icecast if it's not
    if (!is_icecast_initialized)
        shout_init();
    is_icecast_initialized = 1;
}

fmice_icecast::~fmice_icecast() {
    //Destroy mutex
    pthread_mutex_destroy(&mutex);
}

void fmice_icecast::set_host(const char* hostname) {
    strncpy(icecast_host, hostname, sizeof(icecast_host) - 1);
    icecast_host[sizeof(icecast_host) - 1] = 0;
}

void fmice_icecast::set_port(unsigned int port) {
    icecast_port = port;
}

void fmice_icecast::set_mount(const char* mount) {
    strncpy(icecast_mount, mount, sizeof(icecast_mount) - 1);
    icecast_mount[sizeof(icecast_mount) - 1] = 0;
}

void fmice_icecast::set_username(const char* username) {
    strncpy(icecast_username, username, sizeof(icecast_username) - 1);
    icecast_username[sizeof(icecast_username) - 1] = 0;
}

void fmice_icecast::set_password(const char* password) {
    strncpy(icecast_password, password, sizeof(icecast_password) - 1);
    icecast_password[sizeof(icecast_password) - 1] = 0;
}

int fmice_icecast::get_status() {
    pthread_mutex_lock(&mutex);
    int result = stat_status;
    pthread_mutex_unlock(&mutex);
    return result;
}

int fmice_icecast::get_retries() {
    pthread_mutex_lock(&mutex);
    int result = stat_retries;
    pthread_mutex_unlock(&mutex);
    return result;
}

void fmice_icecast::set_status(int req) {
    pthread_mutex_lock(&mutex);
    stat_status = req;
    pthread_mutex_unlock(&mutex);
}

void fmice_icecast::inc_retries() {
    pthread_mutex_lock(&mutex);
    stat_retries++;
    pthread_mutex_unlock(&mutex);
}

bool fmice_icecast::is_configured() {
    return strlen(icecast_host) > 0 &&
        icecast_port > 0 &&
        strlen(icecast_mount) > 0 &&
        strlen(icecast_username) > 0 &&
        strlen(icecast_password);
}

void fmice_icecast::init() {
    //Make sure we're ready
    if (!is_configured())
        throw new std::runtime_error("Icecast is not configured.");

    //Start worker thread
    pthread_create(&worker_thread, NULL, work_static, this);
}

void fmice_icecast::push(dsp::stereo_t* samples, int count) {
    //If the status is not connected, discard the buffer
    if (get_status() != FMICE_ICECAST_STATUS_OK)
        return;

    //Submit to codec for encoding
    codec->write(samples, count);
}

void fmice_icecast::push(float* samples, int count) {
    //If the status is not connected, discard the buffer
    if (get_status() != FMICE_ICECAST_STATUS_OK)
        return;

    //Submit to codec for encoding
    codec->write(samples, count);
}

void* fmice_icecast::work_static(void* ctx) {
    ((fmice_icecast*)ctx)->work();
    return 0;
}

void fmice_icecast::work() {
    //Disable signal from icecast so we handle it ourselves (this caused me must anguish)
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    //Enter loop
    shout_t* shout;
    uint8_t workingBuffer[NET_BUFFER_BYTES];
    while (1) {
        //Set status
        set_status(FMICE_ICECAST_STATUS_CONNECTING);

        //Allocate shoutcast
        //printf("[CAST] Connecting to Icecast...\n");
        shout = shout_new();
        assert(shout != NULL);

        //Set up paramters
        pthread_mutex_lock(&mutex);
        shout_set_host(shout, icecast_host);
        shout_set_protocol(shout, SHOUT_PROTOCOL_HTTP);
        shout_set_port(shout, icecast_port);
        shout_set_password(shout, icecast_password);
        shout_set_mount(shout, icecast_mount);
        shout_set_user(shout, icecast_username);
        codec->configure_shout(shout); // Sets content type
        pthread_mutex_unlock(&mutex);

        //Connect
        if (shout_open(shout) == SHOUTERR_SUCCESS) {
            //Set status
            set_status(FMICE_ICECAST_STATUS_OK);

            //Enter loop
            while (1) {
                //Wait for buffers to become available
                int read = codec->read(workingBuffer, NET_BUFFER_BYTES);

                //Send
                if (shout_send(shout, workingBuffer, read) != SHOUTERR_SUCCESS) {
                    printf("[CAST] Failed to send packet to Icecast.\n");
                    break;
                }

                //Check error flag
                if (codec->has_error()) {
                    printf("[CAST] Codec encountered an error. Disconnecting...\n");
                    break;
                }
            }
        }
        else {
            //Error establishing connection. Wait and try again
            //printf("[CAST] Failed to establish connection. Retrying shortly...\n");
            sleep(3);
        }

        //Destroy shoutcast
        //printf("[CAST] Disconnecting from Icecast...\n");
        set_status(FMICE_ICECAST_STATUS_CONNECTION_LOST);
        inc_retries();
        shout_close(shout);
        shout_free(shout);

        //Reset codec
        codec->reset();
    }
}