/**
 * @name Sensitive Buffer Not Locked in Memory
 * @description Detects sensitive buffers (plaintext, decoded keys, crypto intermediates)
 *              in memory-sensitive functions that are NOT protected with sodium_mlock()
 *              to prevent swapping to disk. This is a multi-stage analysis:
 *              1. Identify crypto-critical functions (read_plaintext, parse_key, seal_base64, etc.)
 *              2. Within those functions, find buffer variables >=16 bytes
 *              3. Exclude const/static/thread_local globals and function parameters
 *              4. Check if buffer is NEVER passed to sodium_mlock() in function scope
 *              5. Flag only buffers that are actually used to hold secrets (indirect check via size)
 * 
 * Detection Logic:
 * - Focus ONLY on: src/cli/*.c, src/salt.c (crypto core)
 * - Exclude: tests/, src/internal.c, header-only files
 * - StackVariable with size >=16 bytes in these scopes
 * - NOT const, NOT static, NOT function param
 * - NOT passed to sodium_mlock() in any call within the function
 * 
 * TRUE POSITIVE: unsigned char plaintext[8192]; // no mlock call
 * TRUE POSITIVE: unsigned char key[32]; // decoded key, no mlock
 * FALSE POSITIVE (acceptable): small work buffers <16 bytes (filtered by size check)
 * 
 * @kind problem
 * @precision high
 * @problem.severity warning
 * @id c/sensitive-buffer-not-locked
 * @tags security/cryptography libsodium memory-safety
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow

/**
 * Predicate: Is this file in a crypto-critical directory/name pattern?
 * Include only src/cli/*.c and src/salt*.c
 */
predicate isCryptoCriticalFile(File f) {
  f.getRelativePath().matches("src/salt%.c") or
  f.getRelativePath().matches("src/cli/%.c")
}

/**
 * Predicate: Is this variable a compile-time-safe constant (no need to lock)?
 */
predicate isCompileTimeConstant(StackVariable sv) {
  sv.isConst() or
  sv.isStatic() or
  sv.getType().isConst()
}

/**
 * Predicate: Is this variable passed to sodium_mlock in its function?
 */
predicate isLockedInFunction(StackVariable sv) {
  exists(FunctionCall mlockCall, Function func |
    sv.getFunction() = func and
    mlockCall.getEnclosingFunction() = func and
    mlockCall.getTarget().getName() = "sodium_mlock" and
    mlockCall.getArgument(0) = sv.getAnAccess()
  )
}

/**
 * Predicate: Is this function crypto-critical (handles secrets)?
 */
predicate isCryptoCriticalFunction(Function f) {
  f.getName().matches("read_%") or
  f.getName().matches("parse_%") or
  f.getName().matches("seal_%") or
  f.getName().matches("cli_input_%") or
  f.getName() = "salt_crypto_box_seal" or
  f.getName() = "crypto_box_seal" or
  f.getName() = "decode_public_key"
}

from StackVariable sv
where
  isCryptoCriticalFile(sv.getFile()) and
  isCryptoCriticalFunction(sv.getFunction()) and
  sv.getType().(ArrayType).getBaseType().(BuiltInType).getName() = "unsigned char" and
  sv.getType().(ArrayType).getArraySize() >= 16 and
  not isCompileTimeConstant(sv) and
  not isLockedInFunction(sv)
select
  sv,
  "Sensitive buffer in crypto-critical function should be locked in memory with sodium_mlock() to prevent disk swapping. Buffer size: " + sv.getType().(ArrayType).getArraySize().toString() + " bytes."
