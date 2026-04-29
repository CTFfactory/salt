/*
 * SPDX-License-Identifier: Unlicense
 *
 * Helper for release-time roundtrip verification.
 *
 * Modes:
 *   keygen
 *     Prints public key then secret key as base64 (one per line).
 *
 *   decrypt --public-key PUB_B64 --secret-key SEC_B64 --ciphertext CIPHERTEXT_B64
 *     Decrypts a sealed-box ciphertext and writes raw plaintext bytes to stdout.
 */

#include <sodium.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *stream) {
    fprintf(stream,
            "usage:\n"
            "  verify-roundtrip-helper keygen\n"
            "  verify-roundtrip-helper decrypt --public-key PUB_B64 --secret-key SEC_B64 --ciphertext CIPHERTEXT_B64\n");
}

static int decode_exact_base64(const char *label, const char *input, unsigned char *out,
                               size_t out_len) {
    size_t written = 0U;
    const char *end = NULL;

    if (input == NULL || out == NULL) {
        fprintf(stderr, "verify-roundtrip-helper: invalid %s input\n", label);
        return 1;
    }

    if (sodium_base642bin(out, out_len, input, strlen(input), NULL, &written, &end,
                          sodium_base64_VARIANT_ORIGINAL) != 0) {
        fprintf(stderr, "verify-roundtrip-helper: invalid %s base64\n", label);
        return 1;
    }
    if (end == NULL || *end != '\0') {
        fprintf(stderr, "verify-roundtrip-helper: trailing characters in %s\n", label);
        return 1;
    }
    if (written != out_len) {
        fprintf(stderr, "verify-roundtrip-helper: invalid %s length\n", label);
        return 1;
    }

    return 0;
}

static int command_keygen(void) {
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    char public_key_b64[sodium_base64_ENCODED_LEN(crypto_box_PUBLICKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    char secret_key_b64[sodium_base64_ENCODED_LEN(crypto_box_SECRETKEYBYTES,
                                                  sodium_base64_VARIANT_ORIGINAL)];
    size_t secret_key_b64_len;
    int rc = 1;

    if (crypto_box_keypair(public_key, secret_key) != 0) {
        fprintf(stderr, "verify-roundtrip-helper: crypto_box_keypair failed\n");
        return 1;
    }

    if (sodium_bin2base64(public_key_b64, sizeof(public_key_b64), public_key,
                          sizeof(public_key), sodium_base64_VARIANT_ORIGINAL) == NULL) {
        fprintf(stderr, "verify-roundtrip-helper: public key encoding failed\n");
        goto out;
    }
    if (sodium_bin2base64(secret_key_b64, sizeof(secret_key_b64), secret_key,
                          sizeof(secret_key), sodium_base64_VARIANT_ORIGINAL) == NULL) {
        fprintf(stderr, "verify-roundtrip-helper: secret key encoding failed\n");
        goto out;
    }

    secret_key_b64_len = strlen(secret_key_b64);
    if (fputs(public_key_b64, stdout) == EOF || fputc('\n', stdout) == EOF ||
        fwrite(secret_key_b64, 1U, secret_key_b64_len, stdout) != secret_key_b64_len ||
        fputc('\n', stdout) == EOF) {
        fprintf(stderr, "verify-roundtrip-helper: failed to write key output\n");
        goto out;
    }

    rc = 0;

out:
    sodium_memzero(secret_key_b64, sizeof(secret_key_b64));
    sodium_memzero(secret_key, sizeof(secret_key));
    return rc;
}

static int command_decrypt(int argc, char **argv) {
    const char *public_key_b64 = NULL;
    const char *secret_key_b64 = NULL;
    const char *ciphertext_b64 = NULL;
    unsigned char public_key[crypto_box_PUBLICKEYBYTES];
    unsigned char secret_key[crypto_box_SECRETKEYBYTES];
    unsigned char *ciphertext = NULL;
    unsigned char *plaintext = NULL;
    size_t ciphertext_max_len;
    size_t ciphertext_len = 0U;
    size_t plaintext_len;
    const char *end = NULL;
    size_t i;
    int rc = 1;

    for (i = 2U; i < (size_t)argc; i += 2U) {
        if ((i + 1U) >= (size_t)argc) {
            fprintf(stderr, "verify-roundtrip-helper: missing value for %s\n", argv[i]);
            goto out;
        }

        if (strcmp(argv[i], "--public-key") == 0) {
            public_key_b64 = argv[i + 1U];
        } else if (strcmp(argv[i], "--secret-key") == 0) {
            secret_key_b64 = argv[i + 1U];
        } else if (strcmp(argv[i], "--ciphertext") == 0) {
            ciphertext_b64 = argv[i + 1U];
        } else {
            fprintf(stderr, "verify-roundtrip-helper: unknown option: %s\n", argv[i]);
            goto out;
        }
    }

    if (public_key_b64 == NULL || secret_key_b64 == NULL || ciphertext_b64 == NULL) {
        fprintf(stderr,
                "verify-roundtrip-helper: decrypt requires --public-key, --secret-key, and --ciphertext\n");
        goto out;
    }

    if (decode_exact_base64("public key", public_key_b64, public_key,
                            sizeof(public_key)) != 0) {
        goto out;
    }
    if (decode_exact_base64("secret key", secret_key_b64, secret_key,
                            sizeof(secret_key)) != 0) {
        goto out;
    }

    /* Base64 decode output is always <= input length for valid payloads. */
    ciphertext_max_len = strlen(ciphertext_b64);
    if (ciphertext_max_len < crypto_box_SEALBYTES) {
        fprintf(stderr, "verify-roundtrip-helper: ciphertext too short\n");
        goto out;
    }

    ciphertext = (unsigned char *)malloc(ciphertext_max_len + 1U);
    if (ciphertext == NULL) {
        fprintf(stderr, "verify-roundtrip-helper: ciphertext allocation failed\n");
        goto out;
    }

    if (sodium_base642bin(ciphertext, ciphertext_max_len, ciphertext_b64, strlen(ciphertext_b64),
                          NULL, &ciphertext_len, &end, sodium_base64_VARIANT_ORIGINAL) != 0) {
        fprintf(stderr, "verify-roundtrip-helper: invalid ciphertext base64\n");
        goto out;
    }
    if (end == NULL || *end != '\0') {
        fprintf(stderr, "verify-roundtrip-helper: trailing characters in ciphertext\n");
        goto out;
    }
    if (ciphertext_len < crypto_box_SEALBYTES) {
        fprintf(stderr, "verify-roundtrip-helper: ciphertext too short\n");
        goto out;
    }

    plaintext_len = ciphertext_len - crypto_box_SEALBYTES;
    plaintext = (unsigned char *)malloc(plaintext_len == 0U ? 1U : plaintext_len);
    if (plaintext == NULL) {
        fprintf(stderr, "verify-roundtrip-helper: plaintext allocation failed\n");
        goto out;
    }

    if (crypto_box_seal_open(plaintext, ciphertext, (unsigned long long)ciphertext_len, public_key,
                             secret_key) != 0) {
        fprintf(stderr, "verify-roundtrip-helper: decryption failed\n");
        goto out;
    }

    if (plaintext_len > 0U && fwrite(plaintext, 1U, plaintext_len, stdout) != plaintext_len) {
        fprintf(stderr, "verify-roundtrip-helper: failed to write plaintext\n");
        goto out;
    }

    rc = 0;

out:
    sodium_memzero(secret_key, sizeof(secret_key));
    if (plaintext != NULL) {
        sodium_memzero(plaintext, plaintext_len == 0U ? 1U : plaintext_len);
        free(plaintext);
    }
    if (ciphertext != NULL) {
        sodium_memzero(ciphertext, ciphertext_max_len);
        free(ciphertext);
    }
    return rc;
}

int main(int argc, char **argv) {
    if (sodium_init() < 0) {
        fprintf(stderr, "verify-roundtrip-helper: sodium_init failed\n");
        return 1;
    }

    if (argc == 2 && strcmp(argv[1], "keygen") == 0) {
        return command_keygen();
    }

    if (argc >= 3 && strcmp(argv[1], "decrypt") == 0) {
        return command_decrypt(argc, argv);
    }

    usage(stderr);
    return 2;
}
