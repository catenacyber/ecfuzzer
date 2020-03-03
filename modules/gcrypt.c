// Copyright (c) 2018 Catena cyber
// Author Philippe Antoine <p.antoine@catenacyber.fr>


#include "../fuzz_ec.h"
#include <gcrypt.h>
#include <stdlib.h>


static const char * eccurvetypeFromTlsId(uint16_t tlsid) {
    switch (tlsid) {
        case 19:
            return "NIST P-192";
        case 21:
            return "NIST P-224";
        case 22:
            return "secp256k1";
        case 23:
            return "NIST P-256";
        case 24:
            return "NIST P-384";
        case 25:
            return "NIST P-521";
        case 26:
            return "brainpoolP256r1";
        case 27:
            return "brainpoolP384r1";
        case 28:
            return "brainpoolP512r1";
    }
    return "";
}

static void my_gcry_logger (void *dummy, int level, const char *format, va_list arg_ptr) {
    return;
}

int fuzzec_gcrypt_init(){
    gpg_error_t err;
    err=gcry_control(GCRYCTL_DISABLE_SECMEM, 0);
    if (err)
        return 1;
    err=gcry_control(GCRYCTL_ENABLE_QUICK_RANDOM, 0);
    if (err)
        return 1;
    err=gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    if (err)
        return 1;
    gcry_set_log_handler (my_gcry_logger, NULL);
    return 0;
}

static void gcrypt_to_ecfuzzer(gcry_mpi_point_t pointZ, fuzzec_output_t * output, size_t index, size_t byteLen, gcry_ctx_t ctx) {
    gcry_mpi_t coordx;
    gcry_mpi_t coordy;
    gcry_mpi_t coordz;
    gpg_error_t err;
    size_t i, realLen, lapse;

    coordx = gcry_mpi_new(0);
    coordy = gcry_mpi_new(0);

    if (gcry_mpi_ec_get_affine (coordx, coordy, pointZ, ctx)) {
        coordz = gcry_mpi_new(0);
        gcry_mpi_point_get(coordx, coordy, coordz, pointZ);
        if (gcry_mpi_get_nbits(coordz) == 0) {
            output->pointSizes[index] = 1;
            output->points[index][0] = 0;
        } else {
            output->errorCode = FUZZEC_ERROR_UNKNOWN;
            gcry_mpi_release(coordx);
            gcry_mpi_release(coordy);
            gcry_mpi_release(coordz);
            return;
        }
        gcry_mpi_release(coordz);
    } else if (gcry_mpi_get_nbits(coordx) == 0 && gcry_mpi_get_nbits(coordy) == 0) {
        output->pointSizes[index] = 1;
        output->points[index][0] = 0;
    } else {
        output->pointSizes[index] = 1 + 2 * byteLen;
        //uncompressed form
        memset(output->points[index], 0,FUZZEC_MAXPOINTLEN);
        output->points[index][0] = 4;

        err = gcry_mpi_print(GCRYMPI_FMT_USG, output->points[index]+1, byteLen, &realLen, coordx);
        if (err) {
            output->errorCode = FUZZEC_ERROR_UNKNOWN;
            gcry_mpi_release(coordx);
            gcry_mpi_release(coordy);
            return;
        }
        if (realLen < byteLen) {
            lapse = byteLen - realLen;
            for (i=byteLen; i>lapse; i--) {
                output->points[index][i] = output->points[index][i-lapse];
            }
            for(i=0; i<lapse; i++) {
                output->points[index][i+1]=0;
            }
        }
        err = gcry_mpi_print(GCRYMPI_FMT_USG, output->points[index]+1+byteLen, byteLen, &realLen, coordy);
        if (err) {
            output->errorCode = FUZZEC_ERROR_UNKNOWN;
            gcry_mpi_release(coordx);
            gcry_mpi_release(coordy);
            return;
        }
        if (realLen < byteLen) {
            lapse = byteLen - realLen;
            for (i=byteLen; i>lapse; i--) {
                output->points[index][byteLen+i] = output->points[index][byteLen+i-lapse];
            }
            for(i=0; i<lapse; i++) {
                output->points[index][1+byteLen+i]=0;
            }
        }

    }
    gcry_mpi_release(coordx);
    gcry_mpi_release(coordy);
}

void fuzzec_gcrypt_process(fuzzec_input_t * input, fuzzec_output_t * output) {
    gpg_error_t err;
    gcry_ctx_t ctx;
    gcry_mpi_t scalar1;
    gcry_mpi_t scalar2;
    gcry_mpi_t scalarz;
    gcry_mpi_point_t point1 = NULL;
    gcry_mpi_point_t point2 = NULL;

    //initialize
    //TODO fuzz custom curves
    err = gcry_mpi_ec_new (&ctx, NULL, eccurvetypeFromTlsId(input->tls_id));
    if (err) {
        output->errorCode = FUZZEC_ERROR_UNSUPPORTED;
        return;
    }

    err = gcry_mpi_scan(&scalar1, GCRYMPI_FMT_USG, input->coordx, input->coordSize, NULL);
    if (err) {
        gcry_ctx_release(ctx);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    err = gcry_mpi_scan(&scalar2, GCRYMPI_FMT_USG, input->coordy, input->coordSize, NULL);
    if (err) {
        gcry_mpi_release(scalar1);
        gcry_ctx_release(ctx);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    point1 = gcry_mpi_point_new(0);
    scalarz = gcry_mpi_set_ui (NULL, 1);
    gcry_mpi_point_set(point1, scalar1, scalar2, scalarz);
    gcry_mpi_release(scalarz);
    point2 = gcry_mpi_point_new(0);
    gcry_mpi_release(scalar1);
    err = gcry_mpi_scan(&scalar1, GCRYMPI_FMT_USG, input->bignum, input->bignumSize, NULL);
    if (err) {
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        goto end;
    }

    //elliptic curve computations
    //P2=scalar2*P1
    gcry_mpi_ec_mul(point2, scalar1, point1, ctx);

    //format output
    gcrypt_to_ecfuzzer(point2, output, 0, ECDF_BYTECEIL(input->groupBitLen), ctx);

    output->errorCode = FUZZEC_ERROR_NONE;

end:
    if (point1) {
        gcry_mpi_point_release(point1);
    }
    if (point2) {
        gcry_mpi_point_release(point2);
    }
    gcry_mpi_release(scalar2);
    gcry_mpi_release(scalar1);
    gcry_ctx_release(ctx);
    return;
}

void fuzzec_gcrypt_add(fuzzec_input_t * input, fuzzec_output_t * output) {
    gpg_error_t err;
    gcry_ctx_t ctx;
    gcry_mpi_t scalar1;
    gcry_mpi_t scalar2;
    gcry_mpi_t scalarz;
    gcry_mpi_point_t point1 = NULL;
    gcry_mpi_point_t point2 = NULL;
    gcry_mpi_point_t point3 = NULL;

    //initialize
    //TODO fuzz custom curves
    err = gcry_mpi_ec_new (&ctx, NULL, eccurvetypeFromTlsId(input->tls_id));
    if (err) {
        output->errorCode = FUZZEC_ERROR_UNSUPPORTED;
        return;
    }

    err = gcry_mpi_scan(&scalar1, GCRYMPI_FMT_USG, input->coordx, input->coordSize, NULL);
    if (err) {
        gcry_ctx_release(ctx);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    err = gcry_mpi_scan(&scalar2, GCRYMPI_FMT_USG, input->coordy, input->coordSize, NULL);
    if (err) {
        gcry_mpi_release(scalar1);
        gcry_ctx_release(ctx);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    point1 = gcry_mpi_point_new(0);
    scalarz = gcry_mpi_set_ui (NULL, 1);
    gcry_mpi_point_set(point1, scalar1, scalar2, scalarz);
    gcry_mpi_release(scalarz);
    gcry_mpi_release(scalar1);
    gcry_mpi_release(scalar2);

    err = gcry_mpi_scan(&scalar1, GCRYMPI_FMT_USG, input->coord2x, input->coordSize, NULL);
    if (err) {
        gcry_ctx_release(ctx);
        gcry_mpi_point_release(point1);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    err = gcry_mpi_scan(&scalar2, GCRYMPI_FMT_USG, input->coord2y, input->coordSize, NULL);
    if (err) {
        gcry_mpi_release(scalar1);
        gcry_ctx_release(ctx);
        gcry_mpi_point_release(point1);
        output->errorCode = FUZZEC_ERROR_UNKNOWN;
        return;
    }
    point2 = gcry_mpi_point_new(0);
    scalarz = gcry_mpi_set_ui (NULL, 1);
    gcry_mpi_point_set(point2, scalar1, scalar2, scalarz);
    gcry_mpi_release(scalarz);

    point3 = gcry_mpi_point_new(0);

    //elliptic curve computations
    //P3=P2+P1
    gcry_mpi_ec_add(point3, point2, point1, ctx);

    //format output
    gcrypt_to_ecfuzzer(point3, output, 0, ECDF_BYTECEIL(input->groupBitLen), ctx);

    output->errorCode = FUZZEC_ERROR_NONE;

    if (point1) {
        gcry_mpi_point_release(point1);
    }
    if (point2) {
        gcry_mpi_point_release(point2);
    }
    if (point3) {
        gcry_mpi_point_release(point3);
    }
    gcry_mpi_release(scalar2);
    gcry_mpi_release(scalar1);
    gcry_ctx_release(ctx);
    return;
}
