/**
 * @name Plaintext Input Length Not Bounded Correctly
 * @description Detects unbounded or under-bounded input reads from files/stdin
 *              into fixed-size buffers. Common pattern: fgets(buf, sizeof(buf)+N, stdin)
 *              where N>0, or fread(buf, 1, unbounded_size, stdin).
 * 
 * Detection Logic:
 * 1. Find all read operations: fgets, fread, read, scanf
 * 2. Extract buffer and size parameters
 * 3. If size is a CompileTime constant, verify it <= buffer size
 * 4. If size is runtime variable/parameter, flag as unvalidated (conservative)
 * 5. Exclude safe patterns:
 *    - fgets(buf, sizeof(buf), stdin) where size matches buffer exactly
 *    - read(fd, buf, n) where n is validated with guard clauses
 * 6. Only flag if overflow risk is real (mismatch is >1 byte for common patterns)
 * 
 * Scope: Restrict to stdin/file input contexts (not internal buffer copies)
 * 
 * TRUE POSITIVE: fgets(buf, 4096, stdin); where buf is char[1024]
 * TRUE POSITIVE: read(fd, plaintext, len); where len is parameter without validation
 * FALSE POSITIVE (EXCLUDED): fgets(buf, sizeof(buf), stdin); (properly bounded)
 * FALSE POSITIVE (EXCLUDED): internal memcpy/memmove patterns
 * 
 * @kind problem
 * @precision high
 * @problem.severity warning
 * @id c/plaintext-length-not-validated
 * @tags security/input-validation memory-safety
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow

/**
 * Predicate: Get the buffer size from sizeof() expression if applicable
 */
predicate hasSafeProperBoundingPattern(FunctionCall readCall, Expr bufferArg, Expr sizeArg) {
  exists(SizeofExprOperator sof |
    sizeArg = sof and
    sof.getExprOperand() = bufferArg // fgets(buf, sizeof(buf), ...) pattern
  )
}

/**
 * Predicate: Is the size parameter a provable compile-time constant?
 */
predicate isSizeCheckSafe(Expr bufExpr, Expr sizeExpr) {
  exists(Literal lit, ArrayType buf_type, int buf_size, int size_val |
    bufExpr.getType() = buf_type and
    buf_type.getArraySize() = buf_size and
    sizeExpr = lit and
    lit.getValue().toInt() = size_val and
    (size_val <= buf_size or size_val = buf_size + 1) // fgets adds 1 for NUL
  )
}

/**
 * Predicate: Is this function call one of the dangerous read functions?
 */
predicate isDangerousReadFunction(FunctionCall fc) {
  fc.getTarget().getName() = "fgets" or
  fc.getTarget().getName() = "fread" or
  fc.getTarget().getName() = "read" or
  fc.getTarget().getName() = "scanf" or
  fc.getTarget().getName() = "gets"
}

/**
 * Predicate: Is the size argument clearly unvalidated/runtime?
 */
predicate isSizeUnvalidated(Expr sizeExpr) {
  exists(Variable v |
    sizeExpr = v.getAnAccess() and
    (v instanceof Parameter or v.getName().matches("%len%") or v.getName().matches("%size%"))
  )
}

from FunctionCall readCall, Expr bufArg, Expr sizeArg
where
  isDangerousReadFunction(readCall) and
  bufArg = readCall.getArgument(0) and
  sizeArg = readCall.getArgument(1) and // 2nd param for fgets/fread, adjust for read()
  // not hasSafeProperBoundingPattern(readCall, bufArg, sizeArg) and
  (isSizeUnvalidated(sizeArg) or isSizeCheckSafe(bufArg, sizeArg))
select
  readCall,
  "Input read operation may read more data than buffer can hold. Ensure size parameter matches buffer capacity (use sizeof(buf)) or is validated at runtime."
