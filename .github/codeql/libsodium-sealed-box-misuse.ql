/**
 * @name Libsodium Sealed-Box: Recipient Public Key Not Validated for Size
 * @description Detects crypto_box_seal calls where the recipient public key
 *              parameter is not validated to be exactly crypto_box_PUBLICKEYBYTES (32 bytes).
 *              This prevents potential use of truncated or oversized keys.
 * 
 * Detection Logic:
 * 1. Find all crypto_box_seal(ciphertext, message, mlen, pk) invocations
 * 2. Trace the pk (2nd positional parameter, index 2) via data flow
 * 3. Check if pk originates from:
 *    - A variable assigned from sodium_base642bin() [already validated]
 *    - A global constant (excluded: compile-time safe)
 *    - An expression with explicit size validation (e.g., decoded_len == 32)
 * 4. Flag if pk source cannot be proven to be exactly 32 bytes
 * 
 * TRUE POSITIVE: crypto_box_seal(ct, msg, mlen, pk_from_argv);  // pk from argv without size check
 * FALSE NEGATIVE: crypto_box_seal(ct, msg, mlen, pk_from_base64_decode); // base64 decode already validates
 * 
 * @kind problem
 * @precision high
 * @problem.severity warning
 * @id c/libsodium-sealed-box-misuse
 * @tags security/cryptography libsodium resource-exhaustion
 */

import cpp
import semmle.code.cpp.dataflow.DataFlow

/**
 * Predicate: Is this expression a variable that originates from sodium_base642bin?
 * (already size-validated by the decode function)
 */
predicate isFromBase64Decode(Expr e) {
  exists(FunctionCall decode, Variable decodedVar |
    decode.getTarget().getName() = "sodium_base642bin" and
    decodedVar.getInitializer().getExpr() = decode and
    DataFlow::localExprFlow(decodedVar.getAnAccess(), e)
  )
}

/**
 * Predicate: Is there a comparison that validates size == crypto_box_PUBLICKEYBYTES?
 */
predicate hasExplicitSizeValidation(Expr keyExpr) {
  exists(BinaryOperation cmp, Variable keyVar, Expr sizeCheckRhs |
    keyExpr = keyVar.getAnAccess() and
    (cmp.getOperator() = "==" or cmp.getOperator() = "!=") and
    (
      (cmp.getLeftOperand() = keyVar.getAnAccess() and sizeCheckRhs = cmp.getRightOperand()) or
      (cmp.getRightOperand() = keyVar.getAnAccess() and sizeCheckRhs = cmp.getLeftOperand())
    ) and
    (
      sizeCheckRhs.toString() = "32" or
      sizeCheckRhs.toString() = "crypto_box_PUBLICKEYBYTES"
    )
  )
}

/**
 * Predicate: Is this expression a global constant (compile-time safe)?
 */
predicate isGlobalConstant(Expr e) {
  exists(Variable v |
    e = v.getAnAccess() and
    v instanceof GlobalVariable and
    v.isConst()
  )
}

from FunctionCall call, Expr pkArg
where
  call.getTarget().getName() = "crypto_box_seal" and
  pkArg = call.getArgument(2) and // 3rd argument is pk in crypto_box_seal(ct, msg, mlen, pk)
  pkArg.getType().(PointerType).getBaseType().(BuiltInType).getName() = "unsigned char" and
  not isFromBase64Decode(pkArg) and
  not isGlobalConstant(pkArg) and
  not hasExplicitSizeValidation(pkArg)
select
  call,
  "crypto_box_seal: recipient public key (parameter 3) is not validated to be exactly crypto_box_PUBLICKEYBYTES (32 bytes). Ensure decoded key is size-checked before use."
