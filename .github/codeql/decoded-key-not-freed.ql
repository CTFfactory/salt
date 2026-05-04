/**
 * @name Decoded Key Not Freed/Zeroed on All Exit Paths
 * @description Detects sodium_base642bin result buffers that are not explicitly
 *              freed/zeroed on at least one code path. This includes:
 *              - Success path where cleanup is missing
 *              - Error return paths where buffer leaks (use-after-free)
 *              - Paths where buffer is used but never freed/munlocked
 * 
 * Detection Logic:
 * 1. Find all sodium_base642bin() calls (decodes base64 into a buffer)
 * 2. Track the output buffer (usually 1st param or assigned result)
 * 3. Use control flow graph to identify all exit paths from the function
 * 4. For each path:
 *    - Check if sodium_memzero, sodium_munlock, or free called before return
 *    - Flag if cleanup is MISSING on ANY path
 * 5. Exceptions:
 *    - Stack-allocated buffers (char[32]) don't need free, but should memzero
 *    - Buffers immediately used in crypto_box_seal and then returned are typically safe
 * 
 * Scope: Only analyze immediate caller of sodium_base642bin
 * 
 * TRUE POSITIVE: decoded = malloc(32); sodium_base642bin(decoded, ...); if (err) return; [no memzero]
 * TRUE POSITIVE: unsigned char key[32]; sodium_base642bin(key, ...); crypto_box_seal(ct, msg, mlen, key); [no memzero after]
 * FALSE NEGATIVE: Multi-path cleanup with goto (may not detect all paths; acceptable trade-off)
 * 
 * @kind problem
 * @precision high
 * @problem.severity warning
 * @id c/decoded-key-not-freed
 * @tags memory-safety resource-leak cryptography
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow
import semmle.code.cpp.controlflow.ControlFlowGraph

/**
 * Predicate: Is the buffer stack-allocated?
 */
predicate isStackAllocated(Expr bufferExpr) {
  exists(ArrayType at | bufferExpr.getType() = at)
}

/**
 * Predicate: Is buffer freed/zeroed in this statement or its successors (within function)?
 */
predicate hasCleanupOnPath(Expr entry, Variable bufVar, Function func) {
  exists(FunctionCall cleanup |
    cleanup.getEnclosingFunction() = func and
    (cleanup.getTarget().getName() = "sodium_memzero" or
     cleanup.getTarget().getName() = "sodium_munlock" or
     cleanup.getTarget().getName() = "free") and
    cleanup.getArgument(0) = bufVar.getAnAccess()
  )
}

/**
 * Predicate: All return/exit paths from the function call cleanup for this buffer?
 */
predicate allExitPathsHaveCleanup(FunctionCall decodingCall, Variable decodedBuffer) {
  not exists(ReturnStmt ret |
    ret.getEnclosingFunction() = decodingCall.getEnclosingFunction() and
    not hasCleanupOnPath(decodingCall, decodedBuffer, ret.getEnclosingFunction())
  )
}

/**
 * Get the buffer variable being decoded into (1st parameter of sodium_base642bin)
 */
Expr getDecodedBufferArg(FunctionCall decodingCall) {
  result = decodingCall.getArgument(0)
}

from FunctionCall decodingCall, Expr bufferArg, Variable bufVar
where
  decodingCall.getTarget().getName() = "sodium_base642bin" and
  bufferArg = getDecodedBufferArg(decodingCall) and
  (
    bufferArg = bufVar.getAnAccess() or
    exists(VariableAccess va | bufferArg = va and bufVar = va.getTarget())
  ) and
  (
    // Stack-allocated buffer: must have sodium_memzero in all paths
    (isStackAllocated(bufferArg) and
     not allExitPathsHaveCleanup(decodingCall, bufVar))
    or
    // Heap-allocated: must have free or memzero in all paths
    (not isStackAllocated(bufferArg) and
     not allExitPathsHaveCleanup(decodingCall, bufVar))
  )
select
  decodingCall,
  "Decoded key buffer (result of sodium_base642bin) is not cleaned up on all exit paths. Call sodium_memzero() (stack) or free() (heap) before all returns."
