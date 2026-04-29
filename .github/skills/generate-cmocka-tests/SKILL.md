---
name: generate-cmocka-tests
description: 'Generate deterministic cmocka tests for success and failure paths in the salt CLI'
argument-hint: '[target file or behavior]'
---

# Generate cmocka Tests

Use this skill to produce or extend cmocka test coverage.

## Steps

1. Identify behavior under test and expected contract.
2. Build fixture setup/teardown for reusable state.
3. Add success-path tests first, then failure-path tests.
4. Assert return codes, stderr/stdout behavior, and output format.
5. Ensure compatibility with sanitizer-enabled execution.
