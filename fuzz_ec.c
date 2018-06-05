// Copyright (c) 2018 Catena cyber
// Author Philippe Antoine <p.antoine@catenacyber.fr>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fuzz_ec.h"

size_t bitlenFromTlsId(uint16_t tlsid) {
    switch (tlsid) {
        //TODO complete curves from TLS
        case 18:
            //secp192k1
            return 192;
        case 19:
            //secp192r1
            return 192;
        case 20:
            //secp224k1
            return 224;
        case 21:
            //secp224r1
            return 224;
        case 22:
            //secp256k1
            return 256;
        case 23:
            //secp256r1
            return 256;
        case 24:
            //secp384r1
            return 384;
        case 25:
            //secp521r1
            return 521;
        case 26:
            //brainpoolP256r1
            return 256;
        case 27:
            //brainpoolP384r1
            return 384;
        case 28:
            //brainpoolP512r1
            return 512;
    }
    return 0;
}

#define NBMODULES 2
//TODO integrate more modules
void fuzzec_mbedtls_process(fuzzec_input_t * input, fuzzec_output_t * output);
void fuzzec_libecc_process(fuzzec_input_t * input, fuzzec_output_t * output);
fuzzec_module_t modules[NBMODULES] = {
    {
        "mbedtls",
        fuzzec_mbedtls_process,
    },
    {
        "libecc",
        fuzzec_libecc_process,
    },
};

int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    fuzzec_input_t input;
    fuzzec_output_t output[NBMODULES];
    size_t groupBitLen;
    size_t i, j, k;

    if (Size < 4) {
        //2 bytes for TLS group, 1 for each of two big integers
        return 0;
    }
    //splits Data in tlsid, big nuber 1, big number 2
    input.tls_id = (Data[0] << 8) | Data[1];
    groupBitLen = bitlenFromTlsId(input.tls_id);
    if (groupBitLen == 0) {
        //unsupported curve
        return 0;
    }

    Size -= 2;
    input.bignum1 = Data + 2;
    if (Size > 2 * ((groupBitLen >> 3) + ((groupBitLen & 0x7) ? 1 : 0))) {
        Size = 2 * ((groupBitLen >> 3) + ((groupBitLen & 0x7) ? 1 : 0));
    }
    input.bignum1Size = Size/2;
    input.bignum2 = input.bignum1 + input.bignum1Size;
    input.bignum2Size = Size - input.bignum1Size;

    //iterate modules
    for (i=0; i<NBMODULES; i++) {
        modules[i].process(&input, &output[i]);
        if (output[i].errorCode == FUZZEC_ERROR_NONE) {
            for (j=0; j<i; j++) {
                if (output[j].errorCode != FUZZEC_ERROR_NONE) {
                    continue;
                }
                for (k=0; k<FUZZEC_NBPOINTS; k++) {
                    if (output[i].pointSizes[k] != output[j].pointSizes[k]) {
                        printf("Module %s and %s returned different lengths for test %zu : %zu vs %zu\n", modules[i].name, modules[j].name, k, output[i].pointSizes[k], output[j].pointSizes[k]);
                        abort();
                    }
                    if (memcmp(output[i].points[k], output[j].points[k], output[i].pointSizes[k]) != 0) {
                        printf("Module %s and %s returned different points for test %zu\n", modules[i].name, modules[j].name, k);
                        abort();
                    }
                }
            }
        } else if (output[i].errorCode != FUZZEC_ERROR_UNSUPPORTED) {
            printf("Module %s returned %d\n", modules[i].name, output[i].errorCode);
            abort();
        }
    }

    return 0;
}
