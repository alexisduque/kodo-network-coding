// Copyright Steinwurf ApS 2014.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

#include "com_steinwurf_dummy_android_MainActivity.h"
#include <cstdlib>
#include <time.h>
#include <inttypes.h>
#include <string.h>
#include <android/log.h>
#include "kodoc/kodoc.h"


#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, "kodo::", __VA_ARGS__))

void trace_callback(const char* zone, const char* data, void* context)
{
    (void) context;

    if (strcmp(zone, "decoder_state") == 0 ||
        strcmp(zone, "symbol_coefficients_before_read_symbol") == 0)
    {
        LOGI("%s:\n", zone);
        LOGI("%s\n", data);
    }
}


JNIEXPORT
jstring JNICALL Java_com_steinwurf_dummy_1android_MainActivity_runKodo(JNIEnv* env, jobject thiz)
{
       // Seed the random number generator to produce different data every time
        srand(time(NULL));

        // Set the number of symbols and the symbol size
        uint32_t max_symbols = 8;
        uint32_t max_symbol_size = 1;

        // Here we select the codec we wish to use
        int32_t codec = kodoc_perpetual;

        // Here we select the finite field to use.
        // kodoc_binary8 is common choice for the perpetual codec
        int32_t finite_field = kodoc_binary8;

        // First, we create an encoder & decoder factory.
        // The factories are used to build actual encoders/decoders
        kodoc_factory_t encoder_factory =
            kodoc_new_encoder_factory(codec, finite_field,
                                     max_symbols, max_symbol_size);

        kodoc_factory_t decoder_factory =
            kodoc_new_decoder_factory(codec, finite_field,
                                     max_symbols, max_symbol_size);

        kodoc_coder_t encoder = kodoc_factory_build_coder(encoder_factory);
        kodoc_coder_t decoder = kodoc_factory_build_coder(decoder_factory);

        // The perpetual encoder supports three operation modes:
        //
        // 1) Random pivot mode (default):
        //    The pivot element is drawn at random
        // 2) Pseudo-systematic
        //    Pivot elements are generated with indices 0,1,2,...,n
        //    After that, the pivots are drawn at random.
        // 3) Pre-charging
        //    For the first "width" symbols, the pivot index is 0. After that,
        ///   the pseudo-systematic mode is used. Finally, pivots are drawn at
        ///   random. The resulting indices: 0(width times),1,2,...,n
        //
        // The operation mode is set with the following API.
        // Note that if both pre-charging and pseudo-systematic is enabled,
        // pre-charging takes precedence.

        // Enable the pseudo-systematic operation mode - faster
        kodoc_set_pseudo_systematic(encoder, 1);

        // Enable the pre-charing operation mode - even faster
        //kodoc_set_pre_charging(encoder, 1);

        LOGI("Pseudo-systematic flag: %d\n", kodoc_pseudo_systematic(encoder));
        LOGI("Pre-charging flag: %d\n", kodoc_pre_charging(encoder));

        // The width of the perpetual code can be set either as a number of symbols
        // using kodoc_set_width(), or as a ratio of the number of symbols using
        // kodoc_set_width_ratio().
        //
        // The default width is set to 10% of the number of symbols.
        LOGI("The width ratio defaults to: %0.2f"
               " (therefore the calculated width is %d)\n",
               kodoc_width_ratio(encoder), kodoc_width(encoder));

        /// When modifying the width, the width ratio will change as well
        kodoc_set_width(encoder, 4);
        LOGI("The width was set to: %d "
               " (therefore the calculated width ratio is %0.2f)\n",
               kodoc_width(encoder), kodoc_width_ratio(encoder));

        /// When modifying the width ratio, the width will change as well
        kodoc_set_width_ratio(encoder, 0.2);
        LOGI("The width ratio was set to: %0.2f"
               " (therefore the calculated width is %d)\n",
               kodoc_width_ratio(encoder), kodoc_width(encoder));

        // Allocate some storage for a "payload". The payload is what we would
        // eventually send over a network.
        uint32_t bytes_used;
        uint32_t payload_size = kodoc_payload_size(encoder);
        uint8_t* payload = (uint8_t*) malloc(payload_size);

        // Allocate input and output data buffers
        uint32_t block_size = kodoc_block_size(encoder);
        uint8_t* data_in = (uint8_t*) malloc(block_size);
        uint8_t* data_out = (uint8_t*) malloc(block_size);

        // Fill the input buffer with random data
        uint32_t i = 0;
        for(; i < block_size; ++i)
            data_in[i] = rand() % 256;

        // Assign the data buffers to the encoder and decoder
        kodoc_set_const_symbols(encoder, data_in, block_size);
        kodoc_set_mutable_symbols(decoder, data_out, block_size);

        // Install a custom trace function for the decoder
        kodoc_set_trace_callback(decoder, trace_callback, NULL);

        uint32_t lost_payloads = 0;
        uint32_t received_payloads = 0;
        while (!kodoc_is_complete(decoder))
        {
            // The encoder will use a certain amount of bytes of the payload buffer
            bytes_used = kodoc_write_payload(encoder, payload);
            LOGI("Payload generated by encoder, bytes used = %d\n", bytes_used);

            // Simulate a channel with a 50% loss rate
            if (rand() % 2)
            {
                lost_payloads++;
                LOGI("Symbol lost on channel\n\n");
                continue;
            }

            // Pass the generated packet to the decoder
            received_payloads++;
            kodoc_read_payload(decoder, payload);
            LOGI("Payload processed by decoder, current rank = %d\n\n",
                   kodoc_rank(decoder));
        }

        LOGI("Number of lost payloads: %d\n", lost_payloads);
        LOGI("Number of received payloads: %d\n", received_payloads);

        // Check that we properly decoded the data
        if (memcmp(data_in, data_out, block_size) == 0)
        {
            LOGI("Data decoded correctly\n");
        }
        else
        {
            LOGI("Unexpected failure to decode, please file a bug report :)\n");
        }


        jstring jpayload;
        char* dst = (char*) malloc(payload_size);
        memcpy(payload, dst, payload_size);
        jpayload = env->NewStringUTF("Success");

        // Free the allocated buffers and the kodo objects
        free(data_in);
        free(data_out);
        free(payload);
        free(dst);

        kodoc_delete_coder(encoder);
        kodoc_delete_coder(decoder);

        kodoc_delete_factory(encoder_factory);
        kodoc_delete_factory(decoder_factory);


        return jpayload;
}