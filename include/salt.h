/*
 * SPDX-License-Identifier: Unlicense
 *
 * Public API for the salt encryption library and CLI entrypoints.
 */

#ifndef SALT_H
#define SALT_H

#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>

/*
 * CERT STR02-C: Avoid magic numbers; document all significant constants.
 * These limits reflect platform or API constraints.
 */

/* SALT_MAX_MESSAGE_LEN = 1MB: Library-level plaintext maximum capacity.
 * Callers can encrypt up to 1MB of plaintext; large enough for most secrets but
 * constrained to prevent excessive memory allocation on servers or embedded systems.
 */
#define SALT_MAX_MESSAGE_LEN ((size_t)(1024U * 1024U))

/* SALT_MAX_KEY_INPUT_LEN = 16KB: Maximum size of key input (base64, JSON, or raw).
 * Bound on parsing complexity and memory consumption for key extraction/validation.
 */
#define SALT_MAX_KEY_INPUT_LEN ((size_t)(16U * 1024U))

#define SALT_MAX_KEY_ID_LEN 256U

/*
 * GitHub Actions caps a single secret value at 48 KB of plaintext per the
 * "Limits for secrets" section of the secrets reference:
 *   https://docs.github.com/en/actions/reference/security/secrets#limits-for-secrets
 *
 * SALT_GITHUB_SECRET_MAX_VALUE_LEN = 48KB: CLI enforces this limit to ensure
 * encrypted payloads remain valid when uploaded through the GitHub Secrets API.
 * The library-level cap stays at SALT_MAX_MESSAGE_LEN (1MB) for non-GitHub callers.
 *
 * Base64 expansion (4/3 ratio): 48KB plaintext + crypto_box_SEALBYTES (32-byte overhead)
 * expands to ~64KB base64 ciphertext, staying within GitHub's practical limits.
 */
#define SALT_GITHUB_SECRET_MAX_VALUE_LEN ((size_t)(48U * 1024U))

#ifndef SALT_VERSION
#define SALT_VERSION "0.0.0"
#endif

/*
 * Return codes separate user input, crypto, runtime I/O, and signal-driven
 * interruption so automation can branch without scraping stderr wording.
 */
enum salt_status {
    SALT_OK = 0,
    SALT_ERR_INPUT = 1,
    SALT_ERR_CRYPTO = 2,
    SALT_ERR_INTERNAL = 3,
    SALT_ERR_IO = 4,
    SALT_ERR_SIGNAL = 5
};

/*
 * Compute the minimum base64 output buffer size required for encrypting a plaintext.
 *
 * Parameters:
 *   message_len: Length of plaintext in bytes.
 *
 * Returns:
 *   Minimum buffer size in bytes (includes space for NUL terminator).
 *   Returns SIZE_MAX if message_len is too large and the result would overflow;
 *   caller must check for SIZE_MAX and reject such requests.
 *
 * Overflow Guarantee:
 *   If result would overflow size_t (i.e., message_len is so large that
 *   base64(message_len + crypto_box_SEALBYTES) > SIZE_MAX), this function
 *   returns SIZE_MAX. Callers must detect this and treat it as a failure
 *   (e.g., reject the message as too large). A buffer of SIZE_MAX bytes
 *   cannot be allocated on any real system, so SIZE_MAX is a safe sentinel.
 *
 * Notes:
 *   - Base64 encoding expands output by 4/3 ratio (CERT STR02-C: documented constant).
 *   - crypto_box_SEALBYTES (32 bytes): libsodium sealed-box authentication tag overhead.
 *   - Safe for use with SALT_MAX_MESSAGE_LEN (1MB); overflow is only possible
 *     with message_len > SIZE_MAX / 4 (approximately).
 *
 * Concurrency:
 *   Not thread-safe. Do not call from multiple threads.
 *   See CERT-C CON00-C (locking discipline).
 *
 * Example:
 *   size_t output_size = salt_required_base64_output_len(message_len);
 *   if (output_size == SIZE_MAX) {
 *       // Message is too large; reject it
 *       fprintf(stderr, "Message too large to encrypt\n");
 *       return error_code;
 *   }
 *   char *output = malloc(output_size);
 */
size_t salt_required_base64_output_len(size_t message_len);

/*
 * Encrypt plaintext using libsodium sealed boxes (public-key cryptography).
 *
 * Encrypts a message using only the recipient's public key. The sealed box
 * cannot be decrypted without the corresponding private key. Output is
 * base64-encoded for transmission over text channels (e.g., GitHub Secrets API).
 * Callers that need raw binary sealed boxes should use libsodium's
 * crypto_box_seal() directly rather than this CLI-oriented wrapper.
 *
 * API selection:
 *   For new code, prefer salt_seal_base64_keylen() so key material crosses the
 *   trust boundary as an explicit {pointer,length} pair. This C-string variant
 *   is retained for compatibility and forwards to the keylen implementation.
 *
 * Parameters:
 *   message: Plaintext bytes to encrypt (may be NULL if message_len == 0, but fails).
 *   message_len: Length of plaintext (0 to SALT_MAX_MESSAGE_LEN bytes).
 *   public_key_b64: Recipient's public key in base64 format (libsodium-generated).
 *     Must be a NUL-terminated C string. For bounded/untrusted buffers and all
 *     new integrations, use salt_seal_base64_keylen().
 *   output: Destination buffer for base64 ciphertext (must be non-NULL).
 *   output_size: Size of output buffer (use salt_required_base64_output_len()).
 *   output_written: Optional; receives count of bytes written (excluding NUL).
 *   error: Optional; receives human-readable error message on failure.
 *   error_size: Size of error buffer (0 is allowed; error is not written).
 *
 * Return Values:
 *   SALT_OK (0): Success; base64 ciphertext written to output.
 *   SALT_ERR_INPUT (1): Validation failure such as empty plaintext, malformed
 *     public key, or caller-provided buffers that do not satisfy preconditions.
 *   SALT_ERR_CRYPTO (2): Libsodium operation failed, such as sodium_init() or
 *     crypto_box_seal() returning an error.
 *   SALT_ERR_INTERNAL (3): Allocation or other internal failure prevented a
 *     usable result from being produced.
 *
 * Preconditions:
 *   - message_len must be between 1 and SALT_MAX_MESSAGE_LEN (1MB).
 *   - public_key_b64 must be valid base64-encoded Curve25519 public key
 *     (decoding to exactly crypto_box_PUBLICKEYBYTES); returns SALT_ERR_INPUT otherwise.
 *   - public_key_b64 must be NUL-terminated.
 *   - output must be non-NULL with size >= salt_required_base64_output_len(message_len).
 *
 * Error Buffer Semantics (CERT C ERR33-C):
 *   - Precondition: If error is non-NULL and error_size > 0, this function WILL write
 *     a UTF-8 error message to error on failure (when return value is not SALT_OK).
 *   - On success (SALT_OK): error buffer is NOT modified; callers should not assume
 *     any content is present.
 *   - Guarantee: If error_size >= 1, error message is always NUL-terminated before return,
 *     even if truncated due to buffer size.
 *   - If error is NULL or error_size is 0, the function is safe to call; no error
 *     message is written, but the function proceeds normally.
 *
 * Output Semantics:
 *   - output_written (if non-NULL): Always written before return; set to 0 on error,
 *     set to byte count written (excluding NUL terminator) on success.
 *   - Guarantee: If output_written is non-NULL, this function ALWAYS writes to it
 *     before returning, on every path (success and error).
 *
 * Memory Safety:
 *   - Sensitive plaintext is NOT zeroed by this function (caller's responsibility if needed).
 *     Use salt_seal_base64_with_plaintext_zeroing() if automatic zeroing is required.
 *   - Ciphertext buffer is zeroed before return on error (defense-in-depth).
 *   - Decryption requires libsodium crypto_box_seal_open() with the private key.
 *
 * Concurrency:
 *   Not thread-safe. Do not call from multiple threads.
 *   See CERT-C CON00-C (locking discipline).
 *
 * API Scope:
 *   - This function intentionally exposes text-oriented, base64 output only.
 *   - Detailed diagnostics are reported through error strings inside the shared
 *     status-code taxonomy documented by enum salt_status.
 *
 * Example:
 *   unsigned char message[] = "secret";
 *   char key_b64[...] = "jQ7xQ+Yd..."; // base64-encoded public key
 *   char output[128];
 *   char error[256];
 *   size_t written;
 *
 *   int rc = salt_seal_base64(message, sizeof(message) - 1, key_b64,
 *                             output, sizeof(output), &written, error, sizeof(error));
 *   if (rc != SALT_OK) {
 *       fprintf(stderr, "Encryption failed: %s\n", error);
 *       return rc;
 *   }
 *   printf("Ciphertext: %s\n", output);
 */
int salt_seal_base64(const unsigned char *message, size_t message_len, const char *public_key_b64,
                     char *output, size_t output_size, size_t *output_written, char *error,
                     size_t error_size);

/*
 * Bounded-buffer variant of salt_seal_base64().
 * Recommended canonical API for new integrations.
 *
 * Trust boundary:
 *   public_key_b64/public_key_b64_len are untrusted caller input. This API validates
 *   that the byte sequence is a complete base64-encoded Curve25519 public key and
 *   rejects invalid length/encoding with SALT_ERR_INPUT.
 *
 * Parameters:
 *   message: Plaintext bytes to encrypt (must be non-NULL).
 *   message_len: Length of plaintext (1..SALT_MAX_MESSAGE_LEN).
 *   public_key_b64: Pointer to base64 key bytes (need not be NUL-terminated).
 *   public_key_b64_len: Number of bytes available at public_key_b64 (must be > 0).
 *   output: Destination buffer for base64 ciphertext.
 *   output_size: Size of output buffer.
 *   output_written: Optional output byte count (excluding trailing NUL).
 *   error: Optional error buffer.
 *   error_size: Size of error buffer.
 *
 * Return values and other semantics match salt_seal_base64().
 */
int salt_seal_base64_keylen(const unsigned char *message, size_t message_len,
                            const char *public_key_b64, size_t public_key_b64_len, char *output,
                            size_t output_size, size_t *output_written, char *error,
                            size_t error_size);

/*
 * Encrypt plaintext using sealed boxes and automatically zero plaintext on return.
 *
 * This is a companion to salt_seal_base64() that enforces the security invariant
 * of zeroing the caller's plaintext buffer on return. This ensures
 * that even if the caller forgets to zero the plaintext, the library will do it
 * automatically (defense-in-depth).
 *
 * API selection:
 *   For new code, prefer salt_seal_base64_with_plaintext_zeroing_keylen() so
 *   public-key input is length-bounded. This C-string variant is retained for
 *   compatibility and forwards to the keylen implementation.
 *
 * On both success and failure paths, the plaintext buffer is wiped with
 * sodium_memzero() before return when message is non-NULL and message_len is within
 * the accepted range (1..SALT_MAX_MESSAGE_LEN).
 *
 * Parameters:
 *   message: Plaintext bytes to encrypt (may be NULL if message_len == 0, but fails).
 *   message_len: Length of plaintext (0 to SALT_MAX_MESSAGE_LEN bytes).
 *   public_key_b64: Recipient's public key in base64 format (libsodium-generated).
 *     Must be a NUL-terminated C string. For bounded/untrusted buffers and all
 *     new integrations, use salt_seal_base64_with_plaintext_zeroing_keylen().
 *   output: Destination buffer for base64 ciphertext (must be non-NULL).
 *   output_size: Size of output buffer (use salt_required_base64_output_len()).
 *   output_written: Optional; receives count of bytes written (excluding NUL).
 *   error: Optional; receives human-readable error message on failure.
 *   error_size: Size of error buffer (0 is allowed; error is not written).
 *
 * Return Values:
 *   SALT_OK (0): Success; base64 ciphertext written to output, plaintext zeroed.
 *   SALT_ERR_INPUT (1): Validation failure such as empty plaintext, malformed
 *     public key, or caller-provided buffers that do not satisfy preconditions.
 *   SALT_ERR_CRYPTO (2): Libsodium operation failed, such as sodium_init() or
 *     crypto_box_seal() returning an error.
 *   SALT_ERR_INTERNAL (3): Allocation or other internal failure prevented a
 *     usable result from being produced.
 *
 * Preconditions:
 *   - message_len must be between 1 and SALT_MAX_MESSAGE_LEN (1MB).
 *   - public_key_b64 must be valid base64-encoded Curve25519 public key
 *     (decoding to exactly crypto_box_PUBLICKEYBYTES); returns SALT_ERR_INPUT otherwise.
 *   - public_key_b64 must be NUL-terminated.
 *   - output must be non-NULL with size >= salt_required_base64_output_len(message_len).
 *
 * Error Buffer Semantics (CERT C ERR33-C):
 *   - Precondition: If error is non-NULL and error_size > 0, this function WILL write
 *     a UTF-8 error message to error on failure (when return value is not SALT_OK).
 *   - On success (SALT_OK): error buffer is NOT modified; callers should not assume
 *     any content is present.
 *   - Guarantee: If error_size >= 1, error message is always NUL-terminated before return,
 *     even if truncated due to buffer size.
 *   - If error is NULL or error_size is 0, the function is safe to call; no error
 *     message is written, but the function proceeds normally.
 *
 * Output Semantics:
 *   - output_written (if non-NULL): Always written before return; set to 0 on error,
 *     set to byte count written (excluding NUL terminator) on success.
 *   - Guarantee: If output_written is non-NULL, this function ALWAYS writes to it
 *     before returning, on every path (success and error).
 *
 * Memory Safety:
 *   - Plaintext is wiped with sodium_memzero() before return on both success
 *     and failure paths when message_len is within the accepted range.
 *   - Ciphertext buffer is zeroed before return on error (defense-in-depth).
 *   - Decryption requires libsodium crypto_box_seal_open() with the private key.
 *
 * Concurrency:
 *   Not thread-safe. Do not call from multiple threads.
 *   See CERT-C CON00-C (locking discipline).
 *
 * Example:
 *   unsigned char message[] = "secret";
 *   char key_b64[...] = "jQ7xQ+Yd...";
 *   char output[128];
 *   char error[256];
 *   size_t written;
 *
 *   // After this call, message[] is zeroed even if caller forgets
 *   int rc = salt_seal_base64_with_plaintext_zeroing(message, sizeof(message) - 1, key_b64,
 *                                                     output, sizeof(output), &written,
 *                                                     error, sizeof(error));
 *   if (rc != SALT_OK) {
 *       fprintf(stderr, "Encryption failed: %s\n", error);
 *       return rc;
 *   }
 *   printf("Ciphertext: %s\n", output);
 */
int salt_seal_base64_with_plaintext_zeroing(unsigned char *message, size_t message_len,
                                            const char *public_key_b64, char *output,
                                            size_t output_size, size_t *output_written, char *error,
                                            size_t error_size);

/*
 * Bounded-buffer variant of salt_seal_base64_with_plaintext_zeroing().
 * Recommended canonical API for new integrations that require plaintext wiping.
 *
 * Parameters and trust-boundary rules mirror salt_seal_base64_keylen().
 * Plaintext zeroing semantics mirror salt_seal_base64_with_plaintext_zeroing().
 */
int salt_seal_base64_with_plaintext_zeroing_keylen(unsigned char *message, size_t message_len,
                                                   const char *public_key_b64,
                                                   size_t public_key_b64_len, char *output,
                                                   size_t output_size, size_t *output_written,
                                                   char *error, size_t error_size);

/*
 * Invoke the salt CLI with standard streams.
 *
 * Parses command-line arguments and executes the salt encryption workflow,
 * reading plaintext from stdin if specified and writing results to stdout.
 * Uses process stdin for input.
 *
 * Parameters:
 *   argc: Argument count (argv[0] should be program name, e.g., "salt").
 *   argv: Argument array (argv[0] through argv[argc-1]).
 *   out_stream: Borrowed output stream for ciphertext (usually stdout).
 *   err_stream: Borrowed output stream for error messages (usually stderr).
 *
 * Return Values:
 *   SALT_OK (0): Success; ciphertext written to out_stream.
 *   SALT_ERR_INPUT (1): Invalid arguments or user input such as missing --key,
 *     malformed JSON, or invalid public-key material.
 *   SALT_ERR_CRYPTO (2): Encryption or other libsodium-backed operation failed.
 *   SALT_ERR_INTERNAL (3): Allocation or other internal setup failure.
 *   SALT_ERR_IO (4): Runtime stdin/stdout/stderr failure or truncated output.
 *   SALT_ERR_SIGNAL (5): Signal-driven interruption (SIGINT, SIGTERM).
 *
 * Behavior:
 *   - Parses options: --key, --key-id, --output (text|json), --key-format (auto|base64|json).
 *   - Positional argument: plaintext (omit or use "-" to read from stdin).
 *   - Reads key from stdin if --key is "-".
 *   - Key and plaintext cannot both be read from stdin in one invocation.
 *   - Output is base64-encoded by default; use --output json for JSON envelope.
 *   - Does not install or restore process signal handlers; embedders retain
 *     ownership of signal disposition policy.
 *
 * Concurrency:
 *   Not thread-safe. Do not call from multiple threads.
 *   See CERT-C CON00-C (locking discipline).
 *
 * Example:
 *   char *argv[] = { "salt", "--key", "jQ7xQ+...", "hello" };
 *   int rc = salt_cli_run(4, argv, stdout, stderr);
 */
int salt_cli_run(int argc, char **argv, FILE *out_stream, FILE *err_stream);

/*
 * Invoke the salt CLI with explicit input/output streams.
 *
 * Similar to salt_cli_run() but allows redirection of stdin for testing
 * and embedding scenarios (e.g., encrypting data from a pipe).
 *
 * Parameters:
 *   argc: Argument count.
 *   argv: Argument array.
 *   in_stream: Borrowed input stream for plaintext or key (if stdin is requested).
 *   out_stream: Borrowed output stream for ciphertext.
 *   err_stream: Borrowed output stream for error messages.
 *
 * Return Values:
 *   SALT_OK (0): Success.
 *   SALT_ERR_INPUT (1): Invalid arguments or malformed input.
 *   SALT_ERR_CRYPTO (2): Encryption failed.
 *   SALT_ERR_INTERNAL (3): Internal error.
 *   SALT_ERR_IO (4): Runtime stream failure or failed output write.
 *   SALT_ERR_SIGNAL (5): Interruption observed through the CLI signal path.
 *
 * Notes:
 *   - This API does not modify process-global signal handlers.
 *   - The standalone executable path (`main`) installs/restores SIGINT/SIGTERM
 *     handlers around each invocation to provide graceful interruption.
 *   - Can be called repeatedly; each call resets state and interruption flags.
 *   - Useful for REPL-style CLI or testing with piped input.
 *   - Not reentrant: concurrent callers in the same process still share
 *     process-scoped interruption state, so embedded multi-threaded use must
 *     serialize access externally.
 *   - Stream ownership: all FILE* parameters are borrowed; this function never
 *     closes them. Streams may be flushed as part of normal output/error handling.
 *
 * Concurrency:
 *   Not thread-safe. Do not call from multiple threads.
 *   See CERT-C CON00-C (locking discipline).
 *
 * Example (Testing):
 *   FILE *in = fopen("plaintext.txt", "r");
 *   FILE *out = tmpfile();
 *   FILE *err = tmpfile();
 *   char *argv[] = { "salt", "--key", "...", "-" };
 *   int rc = salt_cli_run_with_streams(4, argv, in, out, err);
 */
int salt_cli_run_with_streams(int argc, char **argv, FILE *in_stream, FILE *out_stream,
                              FILE *err_stream);

#endif
