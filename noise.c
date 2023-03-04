#include <signal.h>
#include <stdio.h>
#include <portaudio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include "cflags.h"

#define UNUSED(s) (void)(s)

struct noise_data;
typedef int16_t (*noise_generator)(struct noise_data*);
typedef struct noise_data {
    int16_t last_sample;
    noise_generator generator;
} noise_data;


int should_exit = 0;

void error(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    fflush(stderr);
    exit(1);
}

void cleanup() {
    PaError err = Pa_Terminate();
    if (err != paNoError) { error("Failed to terminate PortAudio"); }
}

void signal_handler(int s) {
    switch (s) {
        case SIGINT:
        case SIGTERM:
            cleanup();
            should_exit = 1;
            break;
        default:
            printf("Unhandled signal: %d\n", s);
            exit(1);
    }
}

void register_signal_handler() {
    struct sigaction sigIntHandler;

    sigIntHandler.sa_handler = signal_handler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;

    sigaction(SIGINT, &sigIntHandler, NULL);
}

int16_t generator_white(noise_data* data) {
    UNUSED(data);
    return (int16_t)rand();
}

int16_t generator_brown(noise_data* data) {
    const int max_diff = 1000;
    int new_sample;
    do {
        int16_t diff = (int16_t)rand() % max_diff;
        new_sample = data->last_sample + diff;
    } while (new_sample < INT16_MIN || new_sample > INT16_MAX);

    return (int16_t)new_sample;
}

static int pa_callback(
        const void *input_buf,
        void* output_buf,
        unsigned long frames,
        const PaStreamCallbackTimeInfo* time_info,
        PaStreamCallbackFlags status_flags,
        void* user_data) {

    UNUSED(input_buf);
    UNUSED(status_flags);
    UNUSED(time_info);

    noise_data *data = (noise_data*)user_data;
    int16_t *out = (int16_t*)output_buf;
    unsigned int i;

    for( i=0; i < frames; i++ ) {
        int16_t sample = data->generator(data);
        *(out++) = sample;
        *(out++) = sample;
        data->last_sample = sample;
    }
    return 0;
}

void usage(cflags_t* flags, int exit_code) {
    cflags_print_usage(flags,
                       "[OPTION]... [ARG]...",
                       "Generates noise.",
                       "https://github.com/Dillonb/noise-machine");
    exit(exit_code);
}

int main(int argc, char** argv) {
    register_signal_handler();
    cflags_t* flags = cflags_init();
    const char* generator = "brown";
    cflags_add_string(flags, 'g', "generator", &generator, "Choose a generator. Options: brown, white. Default: brown");
    bool help = false;
    cflags_add_bool(flags, '\0', "help", &help, "print this text and exit");

    if (!cflags_parse(flags, argc, argv) || flags->argc == 0) {
        usage(flags, 1);
    } else if (help) {
        usage(flags, 0);
    }

    noise_data data;
    data.last_sample = 0;
    if (strcasecmp("brown", generator) == 0) {
        data.generator = generator_brown;
    } else if (strcasecmp("white", generator) == 0) {
        data.generator = generator_white;
    } else {
        usage(flags, 1);
    }

    PaError err = Pa_Initialize();
    if (err != paNoError) { error("Failed to initialize PortAudio"); }
    PaStream* stream;

    err = Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 44100, paFramesPerBufferUnspecified, pa_callback, &data);
    if( err != paNoError ) { error("Unable to open default stream"); }
    err = Pa_StartStream( stream );
    if( err != paNoError ) { error("Unable to start stream"); }

    while (!should_exit) {
        sleep(100);
    }
}