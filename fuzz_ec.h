// Copyright (c) 2018 Catena cyber
// Author Philippe Antoine <p.antoine@catenacyber.fr>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECDF_BYTECEIL(numbits) (((numbits) + 7) >> 3)
#define FUZZEC_NBPOINTS 1
#define FUZZEC_MAXPOINTLEN 0x1000

typedef struct _fuzzec_input_t {
    uint16_t tls_id;
    const uint8_t coord[FUZZEC_MAXPOINTLEN];
    const uint8_t * coordx;
    const uint8_t * coordy;
    const uint8_t coord2[FUZZEC_MAXPOINTLEN];
    const uint8_t * coord2x;
    const uint8_t * coord2y;
    const uint8_t * bignum;
    size_t coordSize;
    size_t bignumSize;
    //bit length of curve
    size_t groupBitLen;
} fuzzec_input_t;

/* TODO? more operations
    k2*G and (k2*G)+(k1*G) ?
 */

typedef enum
{
    FUZZEC_ERROR_NONE = 0,
    FUZZEC_ERROR_UNSUPPORTED=1,
    FUZZEC_ERROR_UNKNOWN=2,
} fuzzec_error_t;

typedef struct _fuzzec_output_t {
    fuzzec_error_t errorCode;
    uint8_t points[FUZZEC_NBPOINTS][FUZZEC_MAXPOINTLEN];
    size_t pointSizes[FUZZEC_NBPOINTS];
} fuzzec_output_t;

typedef struct _fuzzec_module_t {
    const char * name;
    void (*process) (fuzzec_input_t *, fuzzec_output_t *);
    void (*add2p) (fuzzec_input_t *, fuzzec_output_t *);
    int (*init) (void);
    void (*fail) (void);
} fuzzec_module_t;

#ifdef __cplusplus
}
#endif
