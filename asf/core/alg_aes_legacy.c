#if defined(__MACH__)
#include <stdlib.h>
#else
#include <malloc.h>
#endif
#include <stdio.h>
#include <string.h>

/**************************************************************************
 *  DEFINITIONS
 **************************************************************************/
#define MOD                         "AES_LEGACY"
#define CIPHER_BLOCK_SIZE           (16)

/**************************************************************************
 *  COMPILE ASSERT
 **************************************************************************/
#define COMPILE_ASSERT(condition) ((void)sizeof(char[1 - 2*!!!(condition)]))

/**************************************************************************
 *  AES FUNCTION
 **************************************************************************/

#define AES_RPOL      0x011b
#define AES_GEN       0x03
#define AES_SBOX_CC   0x63
#define KEY_128 (128/8)
#define KEY_192 (192/8)
#define KEY_256 (256/8)

#define a_mul(a, b) ((a)&&(b)?g_a_ilogt[(g_a_logt[(a)]+g_a_logt[(b)])%0xff]:0)
#define a_inv(a)    ((a)?g_a_ilogt[0xff-g_a_logt[(a)]]:0)

unsigned char g_a_logt[256], g_a_ilogt[256];
unsigned char g_a_sbox[256], g_a_isbox[256];

typedef struct {

    unsigned char state[4][4];
    int kcol;
    unsigned int r;
    unsigned long keysched[0];

} a_ctx_t;

/**************************************************************************
 *  GLOBAL VARIABLES
 **************************************************************************/
static a_ctx_t *ctx;

/**************************************************************************
 *  INTERNAL FUNCTION
 **************************************************************************/
void a_init(void);

a_ctx_t *a_alloc_ctx(unsigned char *key, unsigned int keyLen);

unsigned long a_subword(unsigned long w);

unsigned long a_rotword(unsigned long w);

void a_key_exp(a_ctx_t *ctx);

unsigned char a_mul_manual(unsigned char a, unsigned char b); // use a_mul instead
void a_subbytes(a_ctx_t *ctx);

void a_shi_row(a_ctx_t *ctx);

void a_mix_col(a_ctx_t *ctx);

void a_add_key(a_ctx_t *ctx, int round);

void a_sub_b(a_ctx_t *ctx);

void a_inv_shi_row(a_ctx_t *ctx);

void a_inv_mix_col(a_ctx_t *ctx);

static void a_enc(a_ctx_t *ctx, unsigned char input[16], unsigned char output[16]);

static void a_dec(a_ctx_t *ctx, unsigned char input[16], unsigned char output[16]);

void a_free_ctx(a_ctx_t *ctx);

/**************************************************************************
 *  FUNCTIONS
 **************************************************************************/
a_ctx_t *a_alloc_ctx(unsigned char *key, unsigned int keyLen) {
    a_ctx_t *ctx;
    unsigned int r;
    unsigned int ks_size;

    switch (keyLen) {
        case 16:
            r = 10;
            break;

        case 24:
            r = 12;
            break;

        case 32:
            r = 14;
            break;

        default:
            return NULL;
    }

    ks_size = 4 * (r + 1) * sizeof(unsigned long);

    ctx = malloc(sizeof(a_ctx_t) + ks_size);

    if (ctx) {

        ctx->r = r;
        ctx->kcol = keyLen / 4;
        memcpy(ctx->keysched, key, keyLen);
        ctx->keysched[43] = 0;
        a_key_exp(ctx);

    }

    return ctx;

}

unsigned long a_subword(unsigned long w) {

    return g_a_sbox[w & 0x000000ff] |
           (g_a_sbox[(w & 0x0000ff00) >> 8] << 8) |
           (g_a_sbox[(w & 0x00ff0000) >> 16] << 16) |
           (g_a_sbox[(w & 0xff000000) >> 24] << 24);

}

unsigned long a_rotword(unsigned long w) {
    return ((w & 0x000000ff) << 24) |

           ((w & 0x0000ff00) >> 8) |
           ((w & 0x00ff0000) >> 8) |
           ((w & 0xff000000) >> 8);

}

void a_key_exp(a_ctx_t *ctx) {
    unsigned long temp;
    unsigned long rcon;
    register int i;

    rcon = 0x00000001;
    for (i = ctx->kcol; i < (4 * (ctx->r + 1)); i++) {

        temp = ctx->keysched[i - 1];

        if (!(i % ctx->kcol)) {

            temp = a_subword(a_rotword(temp)) ^ rcon;
            rcon = a_mul(rcon, 2);

        } else if (ctx->kcol > 6 && i % ctx->kcol == 4) {
            temp = a_subword(temp);
        }

        ctx->keysched[i] = ctx->keysched[i - ctx->kcol] ^ temp;

    }

}

unsigned char a_mul_manual(unsigned char a, unsigned char b) {
    register unsigned short ac;
    register unsigned char ret;
    ac = a;
    ret = 0;

    while (b) {
        if (b & 0x01) {
            ret ^= ac;
        }

        ac <<= 1;
        b >>= 1;

        if (ac & 0x0100) {
            ac ^= AES_RPOL;
        }
    }

    return ret;
}

void a_subbytes(a_ctx_t *ctx) {
    int i;

    for (i = 0; i < 16; i++) {
        int x, y;
        x = i & 0x03;
        y = i >> 2;
        ctx->state[x][y] = g_a_sbox[ctx->state[x][y]];
    }
}

void a_shi_row(a_ctx_t *ctx) {
    unsigned char nstate[4][4];
    int i;

    for (i = 0; i < 16; i++) {
        int x, y;
        x = i & 0x03;
        y = i >> 2;
        nstate[x][y] = ctx->state[x][(y + x) & 0x03];
    }
    memcpy(ctx->state, nstate, sizeof(ctx->state));
}

void a_mix_col(a_ctx_t *ctx) {
    unsigned char nstate[4][4];
    int i;

    for (i = 0; i < 4; i++) {
        nstate[0][i] = a_mul(0x02, ctx->state[0][i]) ^
                       a_mul(0x03, ctx->state[1][i]) ^
                       ctx->state[2][i] ^
                       ctx->state[3][i];
        nstate[1][i] = ctx->state[0][i] ^
                       a_mul(0x02, ctx->state[1][i]) ^
                       a_mul(0x03, ctx->state[2][i]) ^
                       ctx->state[3][i];
        nstate[2][i] = ctx->state[0][i] ^
                       ctx->state[1][i] ^
                       a_mul(0x02, ctx->state[2][i]) ^
                       a_mul(0x03, ctx->state[3][i]);
        nstate[3][i] = a_mul(0x03, ctx->state[0][i]) ^
                       ctx->state[1][i] ^
                       ctx->state[2][i] ^
                       a_mul(0x02, ctx->state[3][i]);
    }

    memcpy(ctx->state, nstate, sizeof(ctx->state));
}

void a_add_key(a_ctx_t *ctx, int round) {
    int i;

    for (i = 0; i < 16; i++) {
        int x, y;
        x = i & 0x03;
        y = i >> 2;
        ctx->state[x][y] = ctx->state[x][y] ^
                           ((ctx->keysched[round * 4 + y] & (0xff << (x * 8))) >> (x * 8));
    }
}

void a_inv_shi_row(a_ctx_t *ctx) {
    unsigned char nstate[4][4];

    int i;

    for (i = 0; i < 16; i++) {
        int x, y;
        x = i & 0x03;
        y = i >> 2;
        nstate[x][(y + x) & 0x03] = ctx->state[x][y];
    }

    memcpy(ctx->state, nstate, sizeof(ctx->state));
}

void a_sub_b(a_ctx_t *ctx) {
    int i;

    for (i = 0; i < 16; i++) {
        int x, y;
        x = i & 0x03;
        y = i >> 2;
        ctx->state[x][y] = g_a_isbox[ctx->state[x][y]];
    }
}

void a_inv_mix_col(a_ctx_t *ctx) {
    unsigned char nstate[4][4];
    int i;

    for (i = 0; i < 4; i++) {
        nstate[0][i] = a_mul(0x0e, ctx->state[0][i]) ^
                       a_mul(0x0b, ctx->state[1][i]) ^
                       a_mul(0x0d, ctx->state[2][i]) ^
                       a_mul(0x09, ctx->state[3][i]);
        nstate[1][i] = a_mul(0x09, ctx->state[0][i]) ^
                       a_mul(0x0e, ctx->state[1][i]) ^
                       a_mul(0x0b, ctx->state[2][i]) ^
                       a_mul(0x0d, ctx->state[3][i]);
        nstate[2][i] = a_mul(0x0d, ctx->state[0][i]) ^
                       a_mul(0x09, ctx->state[1][i]) ^
                       a_mul(0x0e, ctx->state[2][i]) ^
                       a_mul(0x0b, ctx->state[3][i]);
        nstate[3][i] = a_mul(0x0b, ctx->state[0][i]) ^
                       a_mul(0x0d, ctx->state[1][i]) ^
                       a_mul(0x09, ctx->state[2][i]) ^
                       a_mul(0x0e, ctx->state[3][i]);
    }

    memcpy(ctx->state, nstate, sizeof(ctx->state));
}

static void a_dec(a_ctx_t *ctx, unsigned char input[16], unsigned char output[16]) {
    int i;

    for (i = 0; i < 16; i++) {
        ctx->state[i & 0x03][i >> 2] = input[i];
    }

    a_add_key(ctx, ctx->r);

    for (i = ctx->r - 1; i >= 1; i--) {
        a_inv_shi_row(ctx);
        a_sub_b(ctx);
        a_add_key(ctx, i);
        a_inv_mix_col(ctx);
    }

    a_inv_shi_row(ctx);
    a_sub_b(ctx);
    a_add_key(ctx, 0);

    for (i = 0; i < 16; i++) {
        output[i] = ctx->state[i & 0x03][i >> 2];
    }
}

static void a_enc(a_ctx_t *ctx, unsigned char input[16], unsigned char output[16]) {
    int i;

    for (i = 0; i < 16; i++) {
        ctx->state[i & 0x03][i >> 2] = input[i];
    }

    a_add_key(ctx, 0);

    for (i = 1; i < ctx->r; i++) {
        a_subbytes(ctx);
        a_shi_row(ctx);
        a_mix_col(ctx);
        a_add_key(ctx, i);
    }
    a_subbytes(ctx);
    a_shi_row(ctx);
    a_add_key(ctx, ctx->r);

    for (i = 0; i < 16; i++) {
        output[i] = ctx->state[i & 0x03][i >> 2];
    }
}

void a_free_ctx(a_ctx_t *ctx) {
    free(ctx);
}

void init_aes(void) {
    int i;
    unsigned char gen;

    gen = 1;

    for (i = 0; i < 0xff; i++) {
        g_a_logt[gen] = i;
        g_a_ilogt[i] = gen;
        gen = a_mul_manual(gen, AES_GEN);

    }

    for (i = 0; i <= 0xff; i++) {

        char bi;
        unsigned char inv = a_inv(i);

        g_a_sbox[i] = 0;

        for (bi = 0; bi < 8; bi++) {

            g_a_sbox[i] |= ((inv & (1 << bi) ? 1 : 0)

                            ^ (inv & (1 << ((bi + 4) & 7)) ? 1 : 0)
                            ^ (inv & (1 << ((bi + 5) & 7)) ? 1 : 0)
                            ^ (inv & (1 << ((bi + 6) & 7)) ? 1 : 0)
                            ^ (inv & (1 << ((bi + 7) & 7)) ? 1 : 0)
                            ^ (AES_SBOX_CC & (1 << bi) ? 1 : 0)

            ) << bi;

        }

        g_a_isbox[g_a_sbox[i]] = i;

    }

    g_a_sbox[1] = 0x7c;
    g_a_isbox[0x7c] = 1;
    g_a_isbox[0x63] = 0;

}

/**************************************************************************
 *  LEGACY FUNCTION - ENCRYPTION
 **************************************************************************/
int
aes_legacy_enc(unsigned char *input_buf, unsigned int input_len, unsigned char *output_buf, unsigned int output_len) {
    unsigned int i = 0;

    if (input_len != output_len) {
        printf("[%s] error, input len should be equal to output len\n", MOD);
        return -1;
    }

    if (0 != input_len % CIPHER_BLOCK_SIZE) {
        printf("[%s] error, input len should be mutiple of %d bytes\n", MOD, CIPHER_BLOCK_SIZE);
        return -1;
    }

    for (i = 0; i != input_len; i += CIPHER_BLOCK_SIZE) {
        a_enc(ctx, input_buf + i, output_buf + i);
    }
    return 0;
}

/**************************************************************************
 *  LEGACY FUNCTION - DECRYPTION
 **************************************************************************/
int
aes_legacy_dec(unsigned char *input_buf, unsigned int input_len, unsigned char *output_buf, unsigned int output_len) {
    unsigned int i = 0;

    if (input_len != output_len) {
        printf("[%s] error, input len should be equal to output len\n", MOD);
        return -1;
    }

    if (0 != input_len % CIPHER_BLOCK_SIZE) {
        printf("[%s] error, input len should be mutiple of %d bytes\n", MOD, CIPHER_BLOCK_SIZE);
        return -1;
    }

    for (i = 0; i != input_len; i += CIPHER_BLOCK_SIZE) {
        a_dec(ctx, input_buf + i, output_buf + i);
    }
    return 0;
}

/**************************************************************************
 *  LEGACY FUNCTION - KEY INITIALIZATION
 **************************************************************************/
int aes_legacy_init_key(unsigned char *key_buf, unsigned int key_len) {
    unsigned char key[KEY_256] = {0};

    if (KEY_256 != key_len) {
        printf("[%s] key size error (%d) (should be %d bytes)\n", MOD, key_len, KEY_256);
        return -1;
    }

    memcpy(key, key_buf, KEY_256);
    init_aes();
    ctx = a_alloc_ctx(key, sizeof(key));

    if (!ctx) {
        printf("[%s] aes alloc ctx fail\n", MOD);
        return -1;
    }

    return 0;

}

/**************************************************************************
 *  LEGACY FUNCTION - VECTOR INITIALIZATION
 **************************************************************************/
int aes_legacy_init_vector(void) {
    init_aes();

    return 0;
}

