#include <stdio.h>

/**************************************************************************
 *  AES FUNCTION
 **************************************************************************/
#include "sec_aes.h"
/* legacy function used for W1128/32 MP */
#include "aes_legacy.h"
/* standard operation aes function used for W1150 MP */
#include "aes_so.h"

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                             "AES_EXPORT"

/**************************************************************************
 *  GLOBAL VARIABLE
 **************************************************************************/
/* using W1128/32 mP solution by default */
AES_VER g_ver = AES_VER_LEGACY;

/**************************************************************************
 *  LIBRARY EXPORT FUNCTION - ENCRYPTION
 **************************************************************************/
int lib_aes_enc(unsigned char *input_buf, unsigned int input_len, unsigned char *output_buf, unsigned int output_len) {

    switch (g_ver) {
        case AES_VER_LEGACY:
            if (0 != aes_legacy_enc(input_buf, input_len, output_buf, output_len)) {
                goto _err;
            }
            break;

        case AES_VER_SO:
            if (0 != aes_so_enc(input_buf, input_len, output_buf, output_len)) {
                goto _err;
            }
            break;

        default:
            printf("[%s] Invalid Ver\n", MOD);
            goto _err;
    }

    return 0;

    _err:

    return -1;

}

/**************************************************************************
 *  LIBRARY EXPORT FUNCTION - DECRYPTION
 **************************************************************************/
int lib_aes_dec(unsigned char *input_buf, unsigned int input_len, unsigned char *output_buf, unsigned int output_len) {
    switch (g_ver) {
        case AES_VER_LEGACY:
            if (0 != aes_legacy_dec(input_buf, input_len, output_buf, output_len)) {
                goto _err;
            }
            break;

        case AES_VER_SO:
            if (0 != aes_so_dec(input_buf, input_len, output_buf, output_len)) {
                goto _err;
            }
            break;

        default:
            printf("[%s] Invalid Ver\n", MOD);
            goto _err;
    }

    return 0;

    _err:

    return -1;

}

/**************************************************************************
 *  LIBRARY EXPORT FUNCTION - KEY INITIALIZATION
 **************************************************************************/
int lib_aes_init_key(unsigned char *key_buf, unsigned int key_len, AES_VER ver) {
    switch (ver) {
        case AES_VER_LEGACY:
            g_ver = AES_VER_LEGACY;
            printf("\n[%s] Legacy\n", MOD);
            if (0 != aes_legacy_init_key(key_buf, key_len)) {
                goto _err;
            }
            break;

        case AES_VER_SO:
            g_ver = AES_VER_SO;
            printf("\n[%s] SO\n", MOD);
            if (0 != aes_so_init_key(key_buf, key_len)) {
                goto _err;
            }
            break;

        default:
            printf("\n[%s] Invalid Ver\n", MOD);
            goto _err;
    }

    return 0;

    _err:

    return -1;
}

int lib_aes_init_vector(AES_VER ver) {
    switch (ver) {
        case AES_VER_LEGACY:
            g_ver = AES_VER_LEGACY;
            printf("[%s] Legacy(V)\n", MOD);
            if (0 != aes_legacy_init_vector()) {
                goto _err;
            }
            break;

        case AES_VER_SO:
            g_ver = AES_VER_SO;
            printf("[%s] SO(V)\n", MOD);
            if (0 != aes_so_init_vector()) {
                goto _err;
            }
            break;

        default:
            printf("[%s] Invalid Ver(V)\n", MOD);
            goto _err;
    }

    return 0;

    _err:
    return -1;
}

