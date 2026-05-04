# Salt Architecture and Design Guide

This document provides a comprehensive description of the **salt** architecture, including all inputs, outputs, and the data flow between them. Salt is a C CLI tool that encrypts plaintext using libsodium's sealed-box cryptography, designed specifically for GitHub Actions Secrets API workflows.

---

## Table of Contents

1. [Overview](#overview)
2. [System Inputs](#system-inputs)
3. [System Outputs](#system-outputs)
4. [Core Architecture](#core-architecture)
5. [Data Flow](#data-flow)
6. [API Surface](#api-surface)
7. [Configuration and Constraints](#configuration-and-constraints)
8. [Error Handling](#error-handling)
9. [Memory and Security Model](#memory-and-security-model)
10. [Trust Boundaries](#trust-boundaries)

---

## Overview

**salt** is a command-line encryption tool that takes three primary inputs:

- A **plaintext** message (from CLI argument or stdin)
- A **recipient public key** in base64 or JSON format (from CLI option or stdin)
- **Output configuration** (text vs JSON, key format hints)

It produces a single primary output:

- **Encrypted ciphertext** as base64-encoded sealed box (text mode) or JSON envelope (JSON mode)

All operations are deterministic, non-interactive, and designed to fail closed on invalid inputs. The tool uses libsodium's `crypto_box_seal` for authenticated encryption and handles sensitive data with memory locking and explicit zeroing.

---

## System Inputs

### 1. Command-Line Arguments

The salt CLI accepts arguments in the following patterns:

```
salt [OPTIONS] [PLAINTEXT]
```

#### Required Options

| Option | Short | Type | Purpose |
|--------|-------|------|---------|
| `--key VALUE` | `-k VALUE` | string | Recipient's public key (base64, JSON object, or `-` for stdin) |

#### Optional Options

| Option | Short | Type | Default | Purpose |
|--------|-------|------|---------|---------|
| `--key-id ID` | `-i ID` | string | none | Key ID override/injector for JSON output |
| `--output FORMAT` | `-o FORMAT` | enum | `text` | Output format: `text` or `json` |
| `--key-format MODE` | `-f MODE` | enum | `auto` | Key parser mode: `auto`, `base64`, or `json` |
| `--help` | `-h` | flag | - | Print usage and exit |
| `--version` | `-V` | flag | - | Print version and exit |

#### Positional Arguments

| Argument | Type | Purpose |
|----------|------|---------|
| `PLAINTEXT` | string | Plaintext to encrypt (omit or use `-` to read from stdin) |

**Example Usage:**

```bash
# Base64 key with positional plaintext
salt -k "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234" "Hello World!"

# JSON key object with JSON output
salt --output json -k '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' "Hello World!"

# Key from stdin, plaintext from CLI argument
echo '{"key_id":"012345678912345678","key":"2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"}' | salt --key - "Hello World!"

# Both key and plaintext from stdin
echo '{"key_id":"012345678912345678","key":"..."}' | salt -k - < plaintext.txt
```

### 2. Stdin Inputs

#### Key from Stdin (`--key -`)

When `--key -` is specified, the tool reads the entire key input from standard input:

- **Format:** Base64 string or JSON object
- **Maximum Size:** 16 KiB (enforced by `SALT_MAX_KEY_INPUT_LEN`)
- **Whitespace Handling:** Leading/trailing ASCII whitespace is trimmed
- **Termination:** Reading stops at EOF

#### Plaintext from Stdin (positional `-` or omitted)

When `PLAINTEXT` is omitted or is `-`, the tool reads plaintext from stdin:

- **Format:** Raw binary or text
- **Maximum Size:** 48 KiB for CLI (enforced by `SALT_GITHUB_SECRET_MAX_VALUE_LEN`), 1 MiB for library
- **Constraints:** Must be non-empty after reading
- **Termination:** Reading stops at EOF

**Constraint:** Key and plaintext cannot both be read from stdin in a single invocation.

### 3. Key Input Formats

#### Base64 Format

- **Encoding:** Standard base64 (RFC 4648) with optional padding
- **Decoding:** Accepts both padded and unpadded forms (using libsodium's `sodium_base642bin`)
- **Expected Length:** 32 bytes (256 bits) when decoded, encoded as ~43-44 characters
- **Variants:** Both `sodium_base64_VARIANT_ORIGINAL` and `sodium_base64_VARIANT_ORIGINAL_NO_PADDING` are accepted

**Example:**
```
2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234
```

#### JSON Key Object Format

- **Standard:** RFC 8259 (strict JSON)
- **Structure:** Object with exactly two fields:
  - `key` (required): Public key in base64 format
  - `key_id` (optional): String identifier for the key
- **Allowed Fields:** Only `key` and `key_id` are permitted; any other fields cause rejection
- **Whitespace:** Trimmed before parsing; leading/trailing spaces ignored

**Example:**
```json
{
  "key_id": "012345678912345678",
  "key": "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234"
}
```

#### Auto-Detection (`--key-format auto`)

When `--key-format auto` is used (default):

1. **Trim** leading/trailing ASCII whitespace
2. **Inspect** first non-whitespace character:
   - `{` → Parse as JSON
   - Otherwise → Parse as base64
3. **Fallback:** If JSON parsing fails, reject (no second attempt as base64)

#### Explicit Format Hints

- `--key-format base64`: Force base64 parsing; reject JSON
- `--key-format json`: Force JSON parsing; reject plain base64

---

## System Outputs

### 1. Standard Output (Stdout)

#### Text Mode (`--output text`, default)

**Format:** Single line containing base64-encoded sealed-box ciphertext followed by newline

**Example:**
```
rKdR6cXq8V1x9P2mL5qZ0...rest of base64...
```

**Characteristics:**
- Pure base64 output (RFC 4648)
- No padding in common use (configurable)
- No additional whitespace or metadata
- Suitable for piping to other tools (e.g., `jq`, `curl`)

#### JSON Mode (`--output json`)

**Format:** RFC 8259 JSON object on a single line, with UTF-8 encoding

**Example:**
```json
{"encrypted_value":"rKdR6cXq8V1x9P2mL5qZ0...base64...","key_id":"012345678912345678"}
```

**Structure:**
- `encrypted_value` (string): Base64-encoded sealed-box ciphertext
- `key_id` (string): Key identifier (from JSON input or `--key-id` option)

**Requirements:**
- `key_id` must be provided (either via JSON key input or `--key-id` option)
- Output is UTF-8 compliant
- Field order is consistent (for reproducibility)

**Constraints:**
- JSON output mode requires a valid `key_id`
- If neither JSON key input nor `--key-id` is provided, salt exits with `SALT_ERR_INPUT`

### 2. Standard Error (Stderr)

#### Error Messages

All error diagnostics go to stderr with a consistent format:

```
error: <message>
```

**Categories:**

| Category | Example | Exit Code |
|----------|---------|-----------|
| **Input Errors** | `--key/-k is required` | 1 |
| **Crypto Errors** | `libsodium initialization failed` | 2 |
| **Internal Errors** | `memory allocation failed` | 3 |
| **I/O Errors** | `failed to write output` | 4 |
| **Signal Errors** | `interrupted by signal` | 5 |

#### Error Message Properties

- **Deterministic:** Same input produces identical error message
- **Parseable:** Stable format suitable for shell automation
- **Non-Sensitive:** Never includes plaintext, decoded keys, or partial ciphertext
- **Machine-Readable:** Exit codes are stable numerics (0-5) for branching in scripts

**Examples:**
```
error: --key/-k is required
error: invalid public key encoding or length
error: plaintext must not be empty
error: JSON output requires key_id
error: key and plaintext cannot both be read from stdin
```

### 3. Exit Codes

| Code | Status | Meaning | Typical Cause |
|------|--------|---------|---------------|
| `0` | `SALT_OK` | Success | Encryption completed; output written |
| `1` | `SALT_ERR_INPUT` | Input/usage error | Missing required option, malformed JSON, invalid key encoding, empty plaintext |
| `2` | `SALT_ERR_CRYPTO` | Cryptographic error | libsodium initialization failure, sealed-box encryption failure |
| `3` | `SALT_ERR_INTERNAL` | Internal error | Memory allocation failure, internal state inconsistency |
| `4` | `SALT_ERR_IO` | I/O error | stdin read failure, stdout write failure, broken pipe, `/dev/full` |
| `5` | `SALT_ERR_SIGNAL` | Signal interruption | SIGINT, SIGTERM caught during execution |

---

## Core Architecture

### 1. Module Organization

```
include/
  salt.h                           # Public API: salt_seal_base64(), salt_cli_run*()
  salt_test_internal.h             # Test hook declarations

src/
  main.c                           # Process entrypoint, hardening, usage/version
  salt.c                           # Core sealed-box encryption, base64 output

  cli/
    args.c / args.h                # CLI argument parsing and validation
    input.c / input.h              # Key/plaintext stdin input handling
    parse.c                        # Key format parsing (base64/JSON)
    prepare.c / prepare.h          # Pre-encryption preparation (buffer setup)
    output.c / output.h            # Output serialization (text/JSON)
    state.c / state.h              # CLI state lifecycle
    signal.c / signals.h           # Signal handler setup and interruption tracking
    stream.c / stream.h            # Stream write helpers
    internal.h                     # Shared CLI internal helpers
    test_hooks.c / test_hooks.h    # Mockable libsodium/libc hooks (testing only)

tests/                             # cmocka unit tests
  cli/test_contract.c              # CLI contract, option handling
  cli/test_io.c                    # stdin/stdout, allocation paths
  cli/test_json.c                  # JSON key parsing, JSON output, UTF-8
  cli/test_runtime.c               # Signal, resource, performance checks
  cli/test_support.c / test_support.h  # Shared test fixtures

fuzz/                              # libFuzzer harnesses
  parse/                           # Key format parser fuzzing
  cli/                             # Full CLI invocation fuzzing
  output/                          # Output encoding fuzzing
  boundary/                        # Edge case and size boundary fuzzing
  roundtrip/                        # Encryption roundtrip fuzzing
```

### 2. Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  Process Entrypoint & Hardening                             │
│  (main.c: core dump suppression, argv scrubbing, RLIMIT)    │
└────────────────────┬────────────────────────────────────────┘
                     │
┌────────────────────▼────────────────────────────────────────┐
│  CLI Orchestration                                          │
│  (src/main.c: salt_cli_run_with_streams)                   │
└────────────────────┬────────────────────────────────────────┘
                     │
     ┌───────────────┼──────────────────┐
     │               │                  │
┌────▼────────┐ ┌───▼──────┐  ┌───────▼──────┐
│ Argument    │ │ Key/Text │  │ State        │
│ Parsing     │ │ Stdin    │  │ Lifecycle    │
│ (args.c)    │ │ Input    │  │ (state.c)    │
└────┬────────┘ │ (input.c)│  └───────┬──────┘
     │          └───┬──────┘         │
     │              │                │
     └──────────────┼────────────────┘
                    │
     ┌──────────────▼──────────────┐
     │ Key Parsing & Validation    │
     │ (parse.c)                   │
     │ - Base64 decoding           │
     │ - JSON object parsing       │
     │ - Field validation          │
     └──────────────┬──────────────┘
                    │
     ┌──────────────▼──────────────┐
     │ Encryption Preparation      │
     │ (prepare.c)                 │
     │ - Buffer allocation         │
     │ - Length validation         │
     │ - Memory locking            │
     └──────────────┬──────────────┘
                    │
     ┌──────────────▼──────────────────────────────┐
     │ Core Encryption Library                     │
     │ (src/salt.c: salt_seal_base64)             │
     │ - libsodium init                            │
     │ - crypto_box_seal()                         │
     │ - base64 encoding                           │
     └──────────────┬──────────────────────────────┘
                    │
     ┌──────────────▼──────────────┐
     │ Output Serialization        │
     │ (output.c)                  │
     │ - Text: base64 + newline    │
     │ - JSON: RFC 8259 envelope   │
     │ - UTF-8 validation          │
     └──────────────┬──────────────┘
                    │
     ┌──────────────▼──────────────┐
     │ Stream Write & Cleanup      │
     │ (stream.c, state.c)         │
     │ - stdout write              │
     │ - Buffer scrubbing          │
     │ - Signal handler restore    │
     └──────────────────────────────┘
```

### 3. Key Data Structures

#### `struct salt_cli_state`

Holds all transient data for a single CLI invocation:

```c
struct salt_cli_state {
    // CLI options (from arguments)
    const char *key_option;              // --key value
    const char *key_id_option;           // --key-id value
    enum salt_cli_output_mode output_mode;  // text|json
    enum salt_cli_key_format key_format;    // auto|base64|json
    bool show_help, show_version;

    // Input sources
    const char *plaintext_arg;           // Positional argument (may be NULL)
    bool plaintext_from_stdin;           // Positional is "-" or omitted
    bool key_from_stdin;                 // --key is "-"

    // Stdin buffers (dynamically allocated, sodium_mlock'd)
    char *stdin_key;                     // Key read from stdin
    char *stdin_plaintext;               // Plaintext read from stdin
    char *argv_plaintext_copy;           // Copy of positional (for scrubbing)

    // Parsing buffers (fixed-size, mlock'd)
    char parsed_key_b64[...];            // Decoded key in base64
    char parsed_key_id[...];             // Parsed key_id from JSON
    bool parsed_has_key_id;              // Whether key_id was in JSON
    const char *effective_key_id;        // Resolved key_id (for output)

    // Message and output (dynamically allocated, mlock'd)
    const unsigned char *message;        // Plaintext bytes
    size_t message_len;                  // Plaintext length
    char *output;                        // Ciphertext buffer
    size_t output_cap;                   // Ciphertext buffer size
    size_t output_written;               // Bytes written to output

    // Diagnostics
    char error[256];                     // Error message (mlock'd)
    bool fixed_buffers_locked;           // Whether mlock succeeded
};
```

---

## Data Flow

### Complete Encryption Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│ User Invocation                                                         │
│ salt -k "key123..." "plaintext"                                         │
│ or: salt --key - --output json < config.json                           │
│ or: echo "secret" | salt -k "key123..." -                              │
└────────────────────────────┬────────────────────────────────────────────┘
                             │
                    ┌────────▼────────┐
                    │ Parse Arguments │
                    │ (args.c)        │
                    └────────┬────────┘
                             │
         ┌───────────────────┼───────────────────┐
         │                   │                   │
         ▼                   ▼                   ▼
    ┌─────────┐         ┌──────────┐      ┌──────────┐
    │ --help? │         │ --key?   │      │ --output?│
    │ --version?│        │ --key-id?│      │ Positional?
    └──┬──────┘         └──┬───────┘      └──┬───────┘
       │ yes               │ required        │
       │ (print & exit)    ▼ (validate)     ▼
       │            ┌──────────────────────┐
       │            │ Determine Input      │
       │            │ Sources:             │
       │            │ - Key from stdin?    │
       │            │ - Plaintext from     │
       │            │   stdin?             │
       │            └──────┬───────────────┘
       │                   │
       │ Constraint Check: Key & plaintext cannot both be from stdin
       │                   │ ✓ valid
       │                   ▼
       │            ┌──────────────────────┐
       │            │ Read Stdin Inputs    │
       │            │ (input.c)            │
       │            │ - Read key (≤ 16 KiB)│
       │            │ - Read plaintext     │
       │            │   (≤ 48 KiB CLI /    │
       │            │    1 MiB library)    │
       │            └──────┬───────────────┘
       │                   │
       │            ┌──────▼───────────────┐
       │            │ Trim Whitespace      │
       │            │ (parse.c)            │
       │            │ - Key trimming       │
       │            │ - Auto-detect format │
       │            └──────┬───────────────┘
       │                   │
       │            ┌──────▼───────────────┐
       │            │ Parse Key            │
       │            │ (parse.c)            │
       │            │ if base64:           │
       │            │  - Decode with       │
       │            │    libsodium         │
       │            │  - Validate length   │
       │            │    == 32 bytes       │
       │            │ if JSON:             │
       │            │  - Parse object      │
       │            │  - Extract "key"     │
       │            │  - Extract "key_id" │
       │            │    (optional)        │
       │            │  - Reject extra      │
       │            │    fields            │
       │            │  - UTF-8 validate    │
       │            └──────┬───────────────┘
       │                   │
       │            ┌──────▼───────────────┐
       │            │ Validate Plaintext   │
       │            │ - Must be non-empty  │
       │            │ - Length ≤ limit    │
       │            │ - Encode validation  │
       │            └──────┬───────────────┘
       │                   │
       └───────────────────┼───────────────┘
                           │
                    ┌──────▼────────────────┐
                    │ Resolve Effective     │
                    │ Key ID                │
                    │ Precedence:           │
                    │ 1. --key-id option   │
                    │ 2. JSON key_id field │
                    │ 3. NULL              │
                    └──────┬───────────────┘
                           │
    ┌──────────────────────▼────────────────┐
    │ Check Output Mode Requirements         │
    │ if JSON mode:                          │
    │  - Must have key_id                    │
    │  - Reject if missing                   │
    └──────┬───────────────────────────────┘
           │
    ┌──────▼──────────────────────────────┐
    │ Allocate & Lock Buffers              │
    │ (prepare.c, state.c)                 │
    │ - Output buffer (size based on       │
    │   plaintext length)                  │
    │ - All sensitive buffers locked with  │
    │   sodium_mlock() (best effort)       │
    └──────┬───────────────────────────────┘
           │
    ┌──────▼──────────────────────────────┐
    │ Core Encryption                      │
    │ (salt.c: salt_seal_base64)           │
    │ - libsodium init (one-time)          │
    │ - crypto_box_seal() encryption       │
    │ - base64 encode output               │
    └──────┬───────────────────────────────┘
           │
    ┌──────▼──────────────────────────────┐
    │ Serialize Output                     │
    │ (output.c)                           │
    │ if text mode:                        │
    │  - Write base64 ciphertext           │
    │  - Add newline                       │
    │ if JSON mode:                        │
    │  - Create JSON object                │
    │  - UTF-8 validate                    │
    │  - Write on single line              │
    └──────┬───────────────────────────────┘
           │
    ┌──────▼──────────────────────────────┐
    │ Write to Stdout                      │
    │ (stream.c)                           │
    │ - Check write success                │
    │ - Handle broken pipes, /dev/full    │
    └──────┬───────────────────────────────┘
           │
    ┌──────▼──────────────────────────────┐
    │ Cleanup & Exit                       │
    │ (state.c)                            │
    │ - Scrub buffers with sodium_memzero()
    │ - Unlock mlock'd regions             │
    │ - Free dynamic allocations           │
    │ - Restore signal handlers            │
    │ - Exit with code 0                   │
    └──────────────────────────────────────┘
```

### Error Path Example

```
salt -k "invalid_key_length" "hello"
      │
      ▼
┌─────────────────────────────────┐
│ Arguments parsed OK             │
│ Key option: "invalid_key..."    │
│ Plaintext option: "hello"       │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Read & Trim Key Input           │
│ Result: "invalid_key_length"    │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Auto-Detect Format              │
│ First char: 'i' (not '{')       │
│ → Treat as base64              │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Decode Base64                   │
│ libsodium base642bin() called    │
│ Result: decoded length != 32    │
│ → REJECT                        │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Set Error Message               │
│ error: "invalid public key      │
│ encoding or length"             │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Write to Stderr                 │
│ "error: invalid public key      │
│ encoding or length"             │
└─────────────┬───────────────────┘
              │
              ▼
┌─────────────────────────────────┐
│ Exit with Code 1                │
│ (SALT_ERR_INPUT)                │
└─────────────────────────────────┘
```

---

## API Surface

### Library Functions (Public API in `include/salt.h`)

#### `salt_required_base64_output_len(message_len: size_t) → size_t`

Computes the minimum buffer size required for base64-encoded output.

**Parameters:**
- `message_len`: Plaintext length in bytes

**Returns:**
- Buffer size in bytes (including space for NUL terminator)

**Formula:**
```
sealed_box_size = message_len + crypto_box_SEALBYTES (32 bytes)
base64_size = ceil(sealed_box_size * 4 / 3)
output_size = base64_size + 1 (for NUL)
```

**Example:**
```c
size_t plain_len = 100;
size_t buf_size = salt_required_base64_output_len(plain_len);
// Returns: ~200 bytes (100 + 32 sealed-box overhead, base64-encoded, + NUL)
```

#### `salt_seal_base64(message, message_len, public_key_b64, output, output_size, output_written, error, error_size) → int`

Core encryption function: encrypts plaintext using a base64 public key and returns base64 ciphertext.

**Parameters:**
- `message`: Plaintext bytes (NULL only if `message_len == 0`, which fails)
- `message_len`: Plaintext length (0 to `SALT_MAX_MESSAGE_LEN`)
- `public_key_b64`: Base64-encoded public key string
- `output`: Destination buffer for base64 ciphertext (non-NULL)
- `output_size`: Destination buffer size
- `output_written`: Optional output; receives byte count written (excluding NUL)
- `error`: Optional error buffer for diagnostics
- `error_size`: Size of error buffer

**Returns:**
- `SALT_OK` (0): Success
- `SALT_ERR_INPUT` (1): Validation error (empty plaintext, bad key, buffers)
- `SALT_ERR_CRYPTO` (2): libsodium failure
- `SALT_ERR_INTERNAL` (3): Allocation failure

**Example:**
```c
unsigned char plaintext[] = "secret";
char key_b64[] = "2Sg8iYjAxxmI2LvUXpJjkYrMxURPc8r+dB7TJyvv1234";
char output[512];
char error[256];
size_t written;

int rc = salt_seal_base64(plaintext, sizeof(plaintext) - 1, key_b64,
                          output, sizeof(output), &written, error, sizeof(error));
if (rc == SALT_OK) {
    printf("Ciphertext: %s (%zu bytes)\n", output, written);
} else {
    fprintf(stderr, "Error: %s\n", error);
}
```

#### `salt_cli_run(argc, argv, out_stream, err_stream) → int`

Invokes the salt CLI with standard process streams (stdin, stdout, stderr).

**Parameters:**
- `argc`: Argument count
- `argv`: Argument array
- `out_stream`: Output stream for ciphertext (usually `stdout`)
- `err_stream`: Error stream for diagnostics (usually `stderr`)

**Returns:**
- `SALT_OK` (0) through `SALT_ERR_SIGNAL` (5)

**Notes:**
- Installs signal handlers (SIGINT, SIGTERM) for the process
- NOT reentrant; concurrent calls share process-wide signal state

#### `salt_cli_run_with_streams(argc, argv, in_stream, out_stream, err_stream) → int`

Invokes the salt CLI with explicit input/output streams (for testing and embedding).

**Parameters:**
- `argc`: Argument count
- `argv`: Argument array
- `in_stream`: Input stream for plaintext/key (stdin)
- `out_stream`: Output stream for ciphertext (stdout)
- `err_stream`: Error stream for diagnostics (stderr)

**Returns:**
- Same as `salt_cli_run`

**Notes:**
- Allows redirection of stdin for testing
- Signal handlers are process-global (not restored on return)
- Useful for REPL-style CLI or piped input scenarios

---

## Configuration and Constraints

### Size Constraints

| Constraint | Value | Purpose |
|-----------|-------|---------|
| `SALT_MAX_MESSAGE_LEN` | 1 MiB (1048576 bytes) | Library-level cap on plaintext |
| `SALT_GITHUB_SECRET_MAX_VALUE_LEN` | 48 KiB (49152 bytes) | CLI enforced cap (GitHub Actions limit) |
| `SALT_MAX_KEY_INPUT_LEN` | 16 KiB (16384 bytes) | Stdin key input cap |
| `SALT_MAX_KEY_ID_LEN` | 256 bytes | Key ID field cap |
| Crypto box seal overhead | 32 bytes | Appended by `crypto_box_seal` |
| Base64 expansion | 4/3× | Encoding multiplier |

### Key Derivation

- **Public Key Size:** 32 bytes (256 bits, libsodium `crypto_box_PUBLICKEYBYTES`)
- **Encryption Method:** `crypto_box_seal` (anonymous authenticated encryption)
- **Nonce Handling:** Implicit (ephemeral sender keypair generated per message)
- **No Key Derivation:** Keys are used directly as `crypto_box` public keys

### Plaintext Handling

- **Supported Range:** 1 byte to 48 KiB (CLI) or 1 MiB (library)
- **Format:** Raw binary or text (treated as raw bytes)
- **Empty Rejection:** Plaintext of 0 bytes is rejected with `SALT_ERR_INPUT`
- **Encoding:** No assumptions; treated as opaque bytes

### Output Stability

- **Deterministic:** Same input (plaintext + key + options) produces same ciphertext
- **Seal Box Nonce:** Ephemeral per invocation (sender keypair is random)
- **Ciphertext Randomness:** Each invocation produces different ciphertext (even for same plaintext)
- **Output Format:** Stable (same base64 encoding rules, same JSON field order)

### Character Encoding

- **JSON Output:** UTF-8 with RFC 8259 compliance
- **Error Messages:** ASCII (subset of UTF-8)
- **Base64 Output:** ASCII (RFC 4648)
- **Key ID Values:** Arbitrary UTF-8 from JSON input

---

## Error Handling

### Validation Checks (by stage)

#### 1. Argument Parsing

- **Missing `--key`:** → `SALT_ERR_INPUT` ("--key/-k is required")
- **Empty `--key` value:** → `SALT_ERR_INPUT` ("--key/-k value must not be empty")
- **Invalid option:** → `SALT_ERR_INPUT` ("invalid option: ...")
- **Conflicting inputs:** → `SALT_ERR_INPUT` ("key and plaintext cannot both be read from stdin")

#### 2. Stdin Reading

- **Read failure:** → `SALT_ERR_IO` ("failed to read from stdin")
- **Key input too large:** → `SALT_ERR_INPUT` ("key input exceeds maximum size")
- **Plaintext too large:** → `SALT_ERR_INPUT` ("plaintext exceeds maximum size")

#### 3. Key Parsing

- **Base64 decode failure:** → `SALT_ERR_INPUT` ("invalid public key encoding or length")
- **Decoded key wrong length:** → `SALT_ERR_INPUT` ("invalid public key encoding or length")
- **JSON parse failure:** → `SALT_ERR_INPUT` ("malformed JSON key object")
- **Missing "key" field:** → `SALT_ERR_INPUT` ("JSON key object missing 'key' field")
- **Extra JSON fields:** → `SALT_ERR_INPUT` ("JSON key object has unexpected fields")
- **key_id not a string:** → `SALT_ERR_INPUT` ("JSON 'key_id' must be a string")

#### 4. Plaintext Validation

- **Empty plaintext:** → `SALT_ERR_INPUT` ("plaintext must not be empty")
- **Plaintext exceeds limit:** → `SALT_ERR_INPUT` ("plaintext exceeds maximum size")

#### 5. Output Mode Validation

- **JSON mode without key_id:** → `SALT_ERR_INPUT` ("JSON output requires key_id")
- **Invalid output format:** → `SALT_ERR_INPUT` ("invalid output format 'X' (expected text|json)")

#### 6. Encryption

- **libsodium init failure:** → `SALT_ERR_CRYPTO` ("libsodium initialization failed")
- **crypto_box_seal failure:** → `SALT_ERR_CRYPTO` ("encryption failed")

#### 7. Memory & I/O

- **malloc() failure:** → `SALT_ERR_INTERNAL` ("memory allocation failed")
- **stdout write failure:** → `SALT_ERR_IO` ("failed to write output")
- **stdout broken pipe:** → `SALT_ERR_IO` ("write error: Broken pipe")

#### 8. Signals

- **SIGINT/SIGTERM:** → `SALT_ERR_SIGNAL` ("interrupted by signal")

---

## Memory and Security Model

### Memory Allocation Strategy

**Fixed-Size Buffers (on stack):**
- Key buffer (parsed key b64): 16 KiB + 1 (mlock'd)
- Key ID buffer: 256 bytes + 1 (mlock'd)
- Error message: 256 bytes (mlock'd)
- CLI state structure: ~200 bytes

**Dynamic Buffers (heap, mlock'd when possible):**
- Stdin key: allocated based on actual read size (≤ 16 KiB)
- Stdin plaintext: allocated based on actual read size (≤ 48 KiB)
- Output: allocated based on `salt_required_base64_output_len(plaintext_len)`
- Temporary intermediate buffers: sealed-box binary form before base64 encoding

### Memory Locking (Defense-in-Depth)

**Locked Buffers:**
- Parsed key (base64)
- Parsed key ID
- Error message buffer
- Stdin key buffer
- Stdin plaintext buffer
- Output ciphertext buffer

**Locking Mechanism:**
- `sodium_mlock()` for sensitive buffers
- Best-effort (soft limit violations are non-fatal)
- Process-wide RLIMIT_MEMLOCK cap applies

**Scrubbing:**
- `sodium_memzero()` on all sensitive buffers before deallocation
- `sodium_munlock()` implicit zeroing as part of unlock
- Stack wipe: fixed 8 KiB scratch area zeroed on exit

### Input Validation

**Constraints Enforced:**
- Non-NULL checks on all pointers
- Length bounds on all user inputs
- Format validation (base64, JSON)
- Range checks before arithmetic (overflow prevention)
- UTF-8 validation on JSON output (RFC 8259)

### Stdin Input Safety

**Key from stdin:**
- Read up to 16 KiB
- Reject (not truncate) if stdin provides more
- Trim whitespace only once at start

**Plaintext from stdin:**
- Read up to 48 KiB (CLI) / 1 MiB (library)
- Reject if exceeds limit
- No truncation

### Process Hardening (main.c)

**Core dump suppression:**
- `setrlimit(RLIMIT_CORE, 0)` prevents core dumps with sensitive data

**Argv scrubbing (on exit):**
- Sensitive plaintext in argv[n] is overwritten with spaces
- Prevents `/proc/<pid>/cmdline` leakage if process crashes

**Signal handling:**
- SIGINT, SIGTERM caught and gracefully exit
- Signal handlers restored (if possible) on cleanup

---

## Trust Boundaries

### Untrusted Inputs

1. **Command-line arguments** (from user shell)
   - All processed as potentially malicious
   - Length limits enforced
   - Format validation on all inputs

2. **Stdin key input** (from pipe, file, heredoc)
   - Treated as untrusted
   - Decoded and validated before use
   - Format parsing is strict (no partial acceptance)

3. **Stdin plaintext** (from pipe, file, or heredoc)
   - Treated as attacker-controlled bytes
   - Length limits enforced
   - No assumptions about encoding

### Trusted Inputs

1. **Environment (libsodium):** Assumed to provide correct crypto primitives
2. **OS kernel:** Assumed to provide correct process isolation and signals
3. **Process state:** At start-of-day (after initialization), assumed consistent

### Security Properties

**Confidentiality:**
- Plaintext visible to user (passed on command line or stdin)
- Plaintext NOT visible in error messages or logs
- Sealed boxes provide confidentiality to holder of private key only

**Integrity:**
- Sealed boxes provide authentication (libsodium guarantee)
- Ciphertext tampering detected by decryption

**Authenticity:**
- Sender is anonymous (sealed boxes do not reveal sender identity)
- Receiver is authenticated (only holder of private key can decrypt)

**Non-Repudiation:**
- None (sender remains anonymous by design)

---

## Summary Data Flow Diagram (Text-Based Flowchart)

```
                          ┌─────────────────────────────────────┐
                          │  User Shell                         │
                          │  salt -k <key> [-o json] [TEXT]    │
                          │  or piped input                     │
                          └────────────┬────────────────────────┘
                                       │
                 ┌─────────────────────▼────────────────────┐
                 │  stdin (if key or plaintext is "-")      │
                 │  ≤ 16 KiB key / ≤ 48 KiB plaintext     │
                 └─────────────────────┬────────────────────┘
                                       │
               ┌───────────────────────▼─────────────────┐
               │  CLI Argument & Stdin Processing        │
               │  1. Parse args (--key, --output, etc)   │
               │  2. Read stdin if needed                │
               │  3. Trim whitespace                     │
               │  4. Auto-detect key format (base64/JSON)│
               └───────────────────────┬─────────────────┘
                                       │
               ┌───────────────────────▼─────────────────┐
               │  Key Parsing                            │
               │  Base64: decode with libsodium          │
               │  JSON: parse object, extract fields     │
               │  Result: 32-byte key + optional key_id  │
               └───────────────────────┬─────────────────┘
                                       │
               ┌───────────────────────▼─────────────────┐
               │  Plaintext Validation                   │
               │  - Non-empty                            │
               │  - ≤ 48 KiB                             │
               │  - Encoding valid (if applicable)       │
               └───────────────────────┬─────────────────┘
                                       │
               ┌───────────────────────▼─────────────────┐
               │  Buffer Setup & Locking                 │
               │  - Allocate output buffer               │
               │  - sodium_mlock() sensitive buffers     │
               │  - Prepare for crypto operation         │
               └───────────────────────┬─────────────────┘
                                       │
               ┌───────────────────────▼─────────────────┐
               │  Core Encryption                        │
               │  salt_seal_base64()                     │
               │  1. libsodium init (one-time)           │
               │  2. crypto_box_seal(plaintext, key)     │
               │  3. base64 encode output                │
               │  Result: base64 ciphertext              │
               └───────────────────────┬─────────────────┘
                                       │
            ┌──────────────────────────▼──────────────────┐
            │  Output Mode Selection                      │
            │  Text: ciphertext + newline                 │
            │  JSON: {"encrypted_value":"...", ...}      │
            └──────────────────────────┬──────────────────┘
                                       │
            ┌──────────────────────────▼──────────────────┐
            │  stdout (normal output)                     │
            │  - Text: base64 line                        │
            │  - JSON: UTF-8 JSON object                  │
            │                                             │
            │  stderr (on error)                          │
            │  - "error: <message>"                       │
            │                                             │
            │  Exit Code:                                 │
            │  - 0 (success)                              │
            │  - 1-5 (error codes)                        │
            └─────────────────────────────────────────────┘
```

---

## Comprehensive Feature Matrix

| Feature | Input | Processing | Output |
|---------|-------|-----------|--------|
| **Plaintext Input** | Positional arg or stdin | Validated length, encoding | Encrypted in sealed box |
| **Key Input** | Stdin or `--key` option | Base64/JSON parsed, validated | Used for `crypto_box_seal` |
| **Output Mode** | `--output text\|json` | Format selection | Text (base64) or JSON envelope |
| **Key Format** | `--key-format auto\|base64\|json` | Format detection | Parsed key + optional ID |
| **Key ID** | JSON or `--key-id` option | Optional, validated | Included in JSON output |
| **Error Handling** | All inputs validated | Deterministic error messages | stderr + exit code (1-5) |
| **Memory Safety** | Dynamic allocation checked | Locking on sensitive buffers | Scrubbing on cleanup |
| **Signal Handling** | Process signals | SIGINT/SIGTERM caught | Graceful exit with code 5 |

---

## References and Related Documentation

- **README.md** — User-facing overview and quick start
- **TESTING.md** — Test plan, coverage policy, and quality gates
- **INSTALL.md** — Build and installation instructions
- **SECURITY.md** — Attack surfaces and security policies
- **CONTRIBUTING.md** — Contributor workflow and documentation requirements
- **include/salt.h** — Public API documentation (inline comments)
- **src/main.c, src/salt.c, src/cli/*.c** — Implementation details (inline comments)
- **libsodium documentation** — crypto_box_seal, sodium_base642bin, sodium_bin2base64

---

**Document Generated:** 2026-05-03
**Scope:** salt CLI v0.0.0+ architecture and design
**Applies To:** All code under `src/`, `include/`, CLI entry points
**Target Audience:** Contributors, reviewers, auditors, and maintainers
