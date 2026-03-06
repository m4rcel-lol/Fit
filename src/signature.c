#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/err.h>
#include "fit.h"

#define SIG_KEY_FILE ".fit/private_key.pem"
#define PUB_KEY_FILE ".fit/public_key.pem"

/* Generate RSA key pair for signing (simplified, not using GPG) */
int signature_generate_keypair(void) {
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);

    if (!ctx) {
        fprintf(stderr, "Failed to create key context\n");
        return -1;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        fprintf(stderr, "Failed to init keygen\n");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        fprintf(stderr, "Failed to set key size\n");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        fprintf(stderr, "Failed to generate key\n");
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    /* Save private key */
    FILE *priv_file = fopen(SIG_KEY_FILE, "wb");
    if (!priv_file) {
        fprintf(stderr, "Failed to create private key file\n");
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (!PEM_write_PrivateKey(priv_file, pkey, NULL, NULL, 0, NULL, NULL)) {
        fprintf(stderr, "Failed to write private key\n");
        fclose(priv_file);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }
    fclose(priv_file);

    /* Save public key */
    FILE *pub_file = fopen(PUB_KEY_FILE, "wb");
    if (!pub_file) {
        fprintf(stderr, "Failed to create public key file\n");
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }

    if (!PEM_write_PUBKEY(pub_file, pkey)) {
        fprintf(stderr, "Failed to write public key\n");
        fclose(pub_file);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return -1;
    }
    fclose(pub_file);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);

    printf("Generated signing key pair:\n");
    printf("  Private: %s\n", SIG_KEY_FILE);
    printf("  Public:  %s\n", PUB_KEY_FILE);
    return 0;
}

/* Sign data with private key */
int signature_sign(const char *data, size_t data_len, char **signature_out, size_t *sig_len_out) {
    FILE *priv_file = fopen(SIG_KEY_FILE, "rb");
    if (!priv_file) {
        fprintf(stderr, "Private key not found. Run 'fit init-signing' first.\n");
        return -1;
    }

    EVP_PKEY *pkey = PEM_read_PrivateKey(priv_file, NULL, NULL, NULL);
    fclose(priv_file);

    if (!pkey) {
        fprintf(stderr, "Failed to read private key\n");
        return -1;
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, pkey) <= 0) {
        fprintf(stderr, "Failed to init signing\n");
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_DigestSignUpdate(mdctx, data, data_len) <= 0) {
        fprintf(stderr, "Failed to update signature\n");
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    size_t sig_len = 0;
    if (EVP_DigestSignFinal(mdctx, NULL, &sig_len) <= 0) {
        fprintf(stderr, "Failed to get signature length\n");
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    unsigned char *sig = malloc(sig_len);
    if (!sig) {
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_DigestSignFinal(mdctx, sig, &sig_len) <= 0) {
        fprintf(stderr, "Failed to finalize signature\n");
        free(sig);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    /* Convert to hex string for storage */
    char *hex_sig = malloc(sig_len * 2 + 1);
    if (!hex_sig) {
        free(sig);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    for (size_t i = 0; i < sig_len; i++) {
        snprintf(hex_sig + i * 2, 3, "%02x", sig[i]);
    }
    hex_sig[sig_len * 2] = '\0';

    *signature_out = hex_sig;
    *sig_len_out = sig_len * 2;

    free(sig);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    return 0;
}

/* Verify signature with public key */
int signature_verify(const char *data, size_t data_len, const char *hex_signature, size_t sig_hex_len) {
    FILE *pub_file = fopen(PUB_KEY_FILE, "rb");
    if (!pub_file) {
        fprintf(stderr, "Public key not found\n");
        return -1;
    }

    EVP_PKEY *pkey = PEM_read_PUBKEY(pub_file, NULL, NULL, NULL);
    fclose(pub_file);

    if (!pkey) {
        fprintf(stderr, "Failed to read public key\n");
        return -1;
    }

    /* Validate hex string length must be even */
    if (sig_hex_len % 2 != 0) {
        fprintf(stderr, "Invalid signature: hex length must be even\n");
        EVP_PKEY_free(pkey);
        return -1;
    }

    /* Validate hex string contains only valid hex characters */
    for (size_t i = 0; i < sig_hex_len; i++) {
        char c = hex_signature[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            fprintf(stderr, "Invalid signature: contains non-hex character at position %zu\n", i);
            EVP_PKEY_free(pkey);
            return -1;
        }
    }

    /* Convert hex signature to binary */
    size_t sig_len = sig_hex_len / 2;
    unsigned char *sig = malloc(sig_len);
    if (!sig) {
        EVP_PKEY_free(pkey);
        return -1;
    }

    for (size_t i = 0; i < sig_len; i++) {
        sscanf(hex_signature + i * 2, "%2hhx", &sig[i]);
    }

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        free(sig);
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, pkey) <= 0) {
        fprintf(stderr, "Failed to init verification\n");
        free(sig);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    if (EVP_DigestVerifyUpdate(mdctx, data, data_len) <= 0) {
        fprintf(stderr, "Failed to update verification\n");
        free(sig);
        EVP_MD_CTX_free(mdctx);
        EVP_PKEY_free(pkey);
        return -1;
    }

    int result = EVP_DigestVerifyFinal(mdctx, sig, sig_len);

    free(sig);
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(pkey);

    if (result == 1) {
        return 0; /* Verification successful */
    } else {
        return -1; /* Verification failed */
    }
}

/* Check if signing key exists */
int signature_has_key(void) {
    FILE *f = fopen(SIG_KEY_FILE, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}
