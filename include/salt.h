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

#define SALT_MAX_MESSAGE_LEN ((size_t)(1024U * 1024U))
#define SALT_MAX_KEY_INPUT_LEN ((size_t)(16U * 1024U))
#define SALT_MAX_KEY_ID_LEN 256U

/*
 * GitHub Actions caps a single secret value at 48 KB of plaintext per the
 * "Limits for secrets" section of the secrets reference:
 *   https://docs.github.com/en/actions/reference/security/secrets#limits-for-secrets
 *
 * The CLI rejects plaintext longer than this so that a successfully encrypted
 * payload remains usable when uploaded through the GitHub Secrets API. The
 * library-level cap stays at SALT_MAX_MESSAGE_LEN for callers that wrap
 * crypto_box_seal for non-GitHub destinations.
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
 *
 * Notes:
 *   - Base64 encoding expands output by 4/3; sealed boxes add crypto_box_SEALBYTES overhead.
 *   - Safe for use with SALT_MAX_MESSAGE_LEN (1MB).
 *
 * Example:
 *   size_t output_size = salt_required_base64_output_len(message_len);
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
 * Parameters:
 *   message: Plaintext bytes to encrypt (may be NULL if message_len == 0, but fails).
 *   message_len: Length of plaintext (0 to SALT_MAX_MESSAGE_LEN bytes).
 *   public_key_b64: Recipient's public key in base64 format (libsodium-generated).
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
 *   - message_len must be <= SALT_MAX_MESSAGE_LEN (1MB).
 *   - public_key_b64 must be valid base64 decoding to exactly crypto_box_PUBLICKEYBYTES.
 *   - output must be non-NULL with size >= salt_required_base64_output_len(message_len).
 *
 * Memory Safety:
 *   - Sensitive plaintext is NOT zeroed by this function (caller's responsibility if needed).
 *   - Ciphertext buffer is zeroed before return on error (defense-in-depth).
 *   - Decryption requires libsodium crypto_box_seal_open() with the private key.
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
 * Invoke the salt CLI with standard streams.
 *
 * Parses command-line arguments and executes the salt encryption workflow,
 * reading plaintext from stdin if specified and writing results to stdout.
 * Uses process stdin for input.
 *
 * Parameters:
 *   argc: Argument count (argv[0] should be program name, e.g., "salt").
 *   argv: Argument array (argv[0] through argv[argc-1]).
 *   out_stream: Output stream for ciphertext (usually stdout).
 *   err_stream: Output stream for error messages (usually stderr).
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
 *
 * Thread Safety:
 *   - This wrapper inherits salt_cli_run_with_streams() signal behavior and is
 *     not reentrant for concurrent callers in the same process.
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
 *   in_stream: Input stream for plaintext or key (if stdin is requested).
 *   out_stream: Output stream for ciphertext.
 *   err_stream: Output stream for error messages.
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
 *   - Signal handlers (SIGINT, SIGTERM) are installed for the process and are
 *     NOT restored on return. Library embedders that care about preserving
 *     pre-existing dispositions must save/restore them around the call.
 *   - Can be called repeatedly; each call resets state and signal handlers.
 *   - Useful for REPL-style CLI or testing with piped input.
 *   - Not reentrant: concurrent callers in the same process share process-wide
 *     signal disposition and interruption state, so embedded multi-threaded
 *     use must serialize access externally.
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
