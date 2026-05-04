AUTOMAKE_OPTIONS = foreign
# This Makefile requires GNU Make due to pattern rules and other GNU extensions
ACLOCAL_AMFLAGS = -I m4

# ============================================================================
# Source Lists
# ============================================================================

SALT_SRCS = src/main.c src/cli/args.c src/cli/input.c src/cli/prepare.c src/cli/test_hooks.c src/cli/parse.c src/cli/output.c src/cli/state.c src/cli/signal.c src/cli/stream.c src/salt.c
SALT_SRC_FILES = $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c \
$(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c \
$(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c \
$(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c \
$(top_srcdir)/src/cli/signal.c $(top_srcdir)/src/cli/stream.c \
$(top_srcdir)/src/salt.c
SYMBOL_AUDIT_ALLOW = main salt_seal_base64 salt_seal_base64_keylen salt_seal_base64_with_plaintext_zeroing salt_seal_base64_with_plaintext_zeroing_keylen salt_required_base64_output_len salt_cli_run salt_cli_run_with_streams salt_cli_emit_output salt_cli_parse_key_input salt_cli_parse_key_json_object_strict salt_cli_trim_whitespace_inplace salt_cli_check_interrupted salt_cli_ensure_signal_handlers salt_cli_restore_signal_handlers salt_cli_flush_stream salt_cli_report_stream_status salt_cli_reset_interrupted salt_cli_resolve_effective_key_id salt_cli_set_error salt_cli_state_cleanup salt_cli_state_init salt_cli_write_stream salt_cli_args_parse_arguments salt_cli_input_load_key_input salt_cli_input_load_plaintext salt_cli_prepare_output
TEST_SRCS = tests/test_main.c tests/test_salt.c \
tests/cli/test_support.c tests/cli/test_contract.c \
tests/cli/test_io.c tests/cli/test_json.c \
tests/cli/test_runtime.c tests/cli/test_helpers.c \
tests/cli/test_characterization.c tests/test_fuzz_regressions.c \
$(SALT_SRCS)
AUDIT_SRC_FILES = $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/parse.c \
$(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c $(top_srcdir)/src/cli/signal.c \
$(top_srcdir)/src/cli/stream.c $(top_srcdir)/src/salt.c
TEST_SRC_FILES = $(top_srcdir)/tests/test_main.c $(top_srcdir)/tests/test_salt.c \
$(top_srcdir)/tests/cli/test_support.c $(top_srcdir)/tests/cli/test_contract.c \
$(top_srcdir)/tests/cli/test_io.c $(top_srcdir)/tests/cli/test_json.c \
$(top_srcdir)/tests/cli/test_runtime.c $(top_srcdir)/tests/cli/test_helpers.c \
$(top_srcdir)/tests/cli/test_characterization.c $(top_srcdir)/tests/test_fuzz_regressions.c \
$(SALT_SRC_FILES)
TEST_HEADER_FILES = $(top_srcdir)/src/cli/internal.h \
$(top_srcdir)/src/cli/args.h $(top_srcdir)/src/cli/input.h $(top_srcdir)/src/cli/prepare.h $(top_srcdir)/src/cli/test_hooks.h $(top_srcdir)/src/cli/output.h \
$(top_srcdir)/src/cli/state.h $(top_srcdir)/src/cli/signals.h $(top_srcdir)/src/cli/stream.h \
$(top_srcdir)/src/salt_test_internal.h $(top_srcdir)/tests/test_suites.h \
$(top_srcdir)/tests/cli/test_shared.h $(top_srcdir)/include/salt.h

# ============================================================================
# Primary Autotools Targets
# ============================================================================

bin_PROGRAMS = salt
include_HEADERS = include/salt.h
man_MANS = man/salt.1

EXTRA_DIST = \
	bootstrap \
	man/salt.1 \
	assets/coverage/solarized-light.css \
	assets/coverage/solarized-dark.css \
	$(wildcard scripts/*.sh) \
	$(wildcard scripts/*.py) \
	$(wildcard scripts/*.c) \
	$(wildcard examples/*.sh) \
	$(wildcard bench/*.[ch]) \
	bench/baseline.json \
	$(wildcard fuzz/*.c) \
	$(wildcard fuzz/*.h) \
	$(wildcard fuzz/corpus/*/*) \
	$(wildcard fuzz/dictionaries/*) \
	$(wildcard docker/ubuntu/*/Dockerfile)

noinst_HEADERS = src/cli/internal.h src/cli/args.h src/cli/input.h src/cli/prepare.h src/cli/test_hooks.h \
src/cli/output.h src/cli/state.h src/cli/signals.h src/cli/stream.h src/salt_test_internal.h \
tests/test_suites.h tests/cli/test_shared.h

AM_CPPFLAGS = -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" \
$(SODIUM_CFLAGS)
AM_CFLAGS = $(BASE_CFLAGS) $(HARDEN_CFLAGS) $(TARGET_CFLAGS)
AM_LDFLAGS = $(HARDEN_LDFLAGS) $(TARGET_LDFLAGS)

salt_SOURCES = $(SALT_SRCS) include/salt.h
salt_LDADD = $(SODIUM_LIBS)

if ENABLE_DEBUG
STRIP_BUILD_BINARIES =
else
STRIP_BUILD_BINARIES = strip-build-binaries
endif

all-local: $(STRIP_BUILD_BINARIES)

.PHONY: strip-build-binaries
strip-build-binaries: salt$(EXEEXT)
	$(STRIP) salt$(EXEEXT)

if ENABLE_TESTS
check_PROGRAMS = test_salt
TESTS = test_salt

test_salt_SOURCES = $(TEST_SRCS) include/salt.h
test_salt_CPPFLAGS = $(AM_CPPFLAGS) $(CMOCKA_CFLAGS) -DSALT_NO_MAIN
test_salt_CFLAGS = $(AM_CFLAGS)
test_salt_LDFLAGS = $(AM_LDFLAGS)
test_salt_LDADD = $(CMOCKA_LIBS) $(SODIUM_LIBS)

tests/test_salt-%.o: tests/%.c
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(test_salt_CPPFLAGS) $(CPPFLAGS) \
	$(test_salt_CFLAGS) $(CFLAGS) -c -o $@ $<

tests/cli/test_salt-%.o: tests/cli/%.c
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(test_salt_CPPFLAGS) $(CPPFLAGS) \
	$(test_salt_CFLAGS) $(CFLAGS) -c -o $@ $<

src/test_salt-%.o: src/%.c
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(test_salt_CPPFLAGS) $(CPPFLAGS) \
	$(test_salt_CFLAGS) $(CFLAGS) -c -o $@ $<

src/cli/test_salt-%.o: src/cli/%.c
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(test_salt_CPPFLAGS) $(CPPFLAGS) \
	$(test_salt_CFLAGS) $(CFLAGS) -c -o $@ $<
endif

# ============================================================================
# Advanced Testing and Quality-Control Targets
# ============================================================================

# Sanitizer and valgrind binaries
SAN_BIN = test_salt_sanitize
VALGRIND_BIN = test_salt_valgrind
VALGRIND_BENCH_KEY_B64 ?= rQB7XCRJP1Z+jY4mN1pK5tWvE3Gd0aHfXkBSYr9hzVE=
SAN_FLAGS ?= -fsanitize=address,undefined
SAN_ASAN_OPTIONS ?= detect_leaks=1:abort_on_error=1:detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1:strict_init_order=1:detect_invalid_pointer_pairs=2
SAN_UBSAN_OPTIONS ?= print_stacktrace=1:halt_on_error=1
VALGRIND_FLAGS ?= --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --track-origins=yes --track-fds=yes --num-callers=40 --keep-stacktraces=alloc-and-free --expensive-definedness-checks=yes --read-var-info=yes --malloc-fill=0xAA --free-fill=0x55 --error-exitcode=42

# Fuzzing configuration
FUZZ_CC ?= clang
FUZZ_FLAGS ?= -g -O1 -fno-omit-frame-pointer \
              -Wno-unknown-warning-option \
              -fsanitize=fuzzer,address,undefined \
              -fsanitize=integer,nullability,implicit-conversion,float-cast-overflow \
              -fno-sanitize=unsigned-integer-overflow,unsigned-shift-base \
              -fsanitize-address-use-after-scope \
              -fno-sanitize-recover=undefined,integer
FUZZ_MAX_TOTAL_TIME ?= 30
FUZZ_SMOKE_TIME ?= 20
FUZZ_STATE_DIR ?= .fuzz
FUZZ_CORPUS_DIR ?= $(FUZZ_STATE_DIR)
FUZZ_SEED_CORPUS_ROOT ?= fuzz/corpus
FUZZ_LIBFUZZER_FLAGS ?= -timeout=10 -rss_limit_mb=2048 -malloc_limit_mb=256 \
                        -print_pcs=0 -print_final_stats=1
FUZZ_MAX_LEN_PARSE ?= 4096
FUZZ_MAX_LEN_CLI ?= 4096
FUZZ_MAX_LEN_OUTPUT ?= 4096
FUZZ_MAX_LEN_BOUNDARY ?= 256
FUZZ_MAX_LEN_ROUNDTRIP ?= 4096
FUZZ_ASAN_OPTIONS ?= abort_on_error=1:detect_stack_use_after_return=1:strict_string_checks=1:check_initialization_order=1:strict_init_order=1:detect_invalid_pointer_pairs=2:allocator_may_return_null=1
FUZZ_UBSAN_OPTIONS ?= print_stacktrace=1:halt_on_error=1

# Coverage configuration
COVERAGE ?= 99
FUNCTION_COVERAGE ?= 99
BLOCK_COVERAGE ?= 99
BRANCH_COVERAGE ?= 99
COV_FLAGS ?= --coverage
COV_BUILD_CFLAGS ?= $(BASE_CFLAGS)
COV_BIN = test_salt_coverage
COVERAGE_HTML_DIR ?= coverage
COVERAGE_CSS_LIGHT ?= $(top_srcdir)/assets/coverage/solarized-light.css
COVERAGE_CSS_DARK ?= $(top_srcdir)/assets/coverage/solarized-dark.css

# Benchmark configuration
BENCH_DIR = $(top_srcdir)/bench
BENCH_BIN = bench_salt
BENCH_SPAWN_BIN = bench_spawn
BENCH_RESULTS_DIR = bench-results
BENCH_RESULTS = $(BENCH_RESULTS_DIR)/results.json
BENCH_RESULTS_MICRO = $(BENCH_RESULTS_DIR)/results-micro.json
BENCH_RESULTS_SPAWN = $(BENCH_RESULTS_DIR)/results-spawn.json
BENCH_BASELINE = $(top_srcdir)/bench/baseline.json
BENCH_REGRESSION_PCT ?= 40
BENCH_P95_REGRESSION_PCT ?= 70
BENCH_COLD_START_REGRESSION_PCT ?= 80
BENCH_COLD_START_P95_REGRESSION_PCT ?= 120
BENCH_COMPARE_MICRO_REGEX ?= ^(seal_|base64_encode_|parse_|output_)
BENCH_COMPARE_SPAWN_REGEX ?= ^cli_cold_start_
BENCH_CFLAGS ?= -std=c17 -Wall -Wextra -Wpedantic -O3 -DNDEBUG \
                -D_FORTIFY_SOURCE=2 -fstack-protector-strong
BENCH_SRC = $(top_srcdir)/bench/bench_main.c $(top_srcdir)/bench/bench_common.c \
              $(top_srcdir)/bench/bench_seal.c $(top_srcdir)/bench/bench_base64.c \
              $(top_srcdir)/bench/bench_parse.c $(top_srcdir)/bench/bench_output.c \
              $(top_srcdir)/src/salt.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/cli/output.c
BENCH_SPAWN_SRC = $(top_srcdir)/bench/bench_spawn.c $(top_srcdir)/bench/bench_common.c

# Linting tools
CLANG_TIDY ?= clang-tidy-18
CLANG_FORMAT ?= clang-format-18
CLANG ?= clang-18
CPPCHECK ?= cppcheck
SHELLCHECK ?= shellcheck
CODESPELL ?= codespell
CODESPELL_ARGS ?= --check-filenames --quiet-level=2 --ignore-words-list=bu,crasher,crashers --skip=build,bin,.git,fuzz/corpus,terraform,autom4te.cache,build-aux
FORMAT_SRCS = $(SALT_SRC_FILES) \
               $(top_srcdir)/src/cli/args.h $(top_srcdir)/src/cli/input.h $(top_srcdir)/src/cli/prepare.h $(top_srcdir)/src/cli/test_hooks.h $(top_srcdir)/src/salt_test_internal.h \
               $(top_srcdir)/include/salt.h $(top_srcdir)/src/cli/internal.h \
               $(top_srcdir)/fuzz/fuzz_common.h \
               $(top_srcdir)/tests/test_salt.c $(top_srcdir)/tests/test_main.c \
               $(top_srcdir)/tests/test_suites.h \
               $(top_srcdir)/tests/cli/test_support.c $(top_srcdir)/tests/cli/test_contract.c \
               $(top_srcdir)/tests/cli/test_io.c $(top_srcdir)/tests/cli/test_json.c \
               $(top_srcdir)/tests/cli/test_runtime.c $(top_srcdir)/tests/cli/test_helpers.c \
               $(top_srcdir)/tests/cli/test_characterization.c $(top_srcdir)/tests/test_fuzz_regressions.c \
               $(top_srcdir)/tests/cli/test_shared.h \
               $(top_srcdir)/fuzz/fuzz_parse.c $(top_srcdir)/fuzz/fuzz_cli.c \
               $(top_srcdir)/fuzz/fuzz_output.c $(top_srcdir)/fuzz/fuzz_boundary.c \
               $(top_srcdir)/fuzz/fuzz_roundtrip.c

# SBOM configuration
SBOM_DIR ?= sbom
SBOM_STAGING_DIR ?= $(SBOM_DIR)/staging
SBOM_SOURCE_NAME ?= salt
SBOM_SOURCE_SUPPLIER ?= CTFfactory
SBOM_VERSION ?= 0.0.0
SBOM_SOURCE_REF ?=
PROJECT_URL ?= https://github.com/CTFfactory/salt
SBOM_LICENSE ?= Unlicense
SBOM_COPYRIGHT ?= Copyright 2026 CTFfactory
SBOM_FILE_METADATA_SELECTION ?= all
LIBSODIUM_UPSTREAM_VERSION ?= 1.0.18
SBOM_UBUNTU_DEPS ?= $(SBOM_DIR)/ubuntu-build-provenance.json
SBOM_BUILD_TOOL_CC ?= $(CC)
SBOM_BUILD_TOOL_CLANG ?= clang
SBOM_BUILD_TOOL_MAKE ?= make
SBOM_BUILD_TOOL_LIBC_DEV ?= libc6-dev
SBOM_BUILD_TOOL_LDD ?= libc-bin
SBOM_BUILD_TOOL_BINUTILS ?= binutils
SBOM_BUILD_TOOL_AUTOCONF ?= autoconf
SBOM_BUILD_TOOL_AUTOMAKE ?= automake
SBOM_BUILD_TOOL_LIBTOOL ?= libtool
SBOM_BUILD_TOOL_PKG_CONFIG ?= pkg-config
SBOM_DYNAMIC = $(SBOM_DIR)/salt-linux-amd64.spdx.json
SBOM_STATIC = $(SBOM_DIR)/salt-static-linux-amd64.spdx.json
SBOM_DYNAMIC_CYCLONEDX = $(SBOM_DIR)/salt-linux-amd64.cyclonedx.json
SBOM_STATIC_CYCLONEDX = $(SBOM_DIR)/salt-static-linux-amd64.cyclonedx.json
PARLAY_SBOMS ?= $(SBOM_DYNAMIC) $(SBOM_STATIC) $(SBOM_DYNAMIC_CYCLONEDX) $(SBOM_STATIC_CYCLONEDX)
PARLAY_SNYK_ENRICH ?= 0
GRYPE_FAIL_ON ?= negligible

# Tool installation
ACTIONLINT_VERSION ?= 1.7.8
ACTIONLINT_SHA256 ?= be92c2652ab7b6d08425428797ceabeb16e31a781c07bc388456b4e592f3e36a
ACTIONLINT_BIN ?= tools/actionlint
SYFT_VERSION ?= 1.44.0
SYFT_SHA256 ?= 0e91737aee2b5baf1d255b959630194a302335d848ff97bb07921eb6205b5f5a
SYFT_BIN ?= tools/syft
PARLAY_VERSION ?= 0.11.0
PARLAY_SHA256 ?= 2654d8e91c53f8e264f439684f657fc0139a8c13d842c30aa4f12c9cc9be1243
PARLAY_BIN ?= tools/parlay
GRYPE_VERSION ?= 0.111.1
GRYPE_SHA256 ?= 2bc0bc60f1f4e10b0429f5e84517ec4cf0d769d2ef66875c64fc6640e136fd8f
GRYPE_BIN ?= tools/grype

# Docker/Package configuration
DOCKER ?= docker
DOCKER_UBUNTU_VERSIONS ?= 22 24 26
DEB_UBUNTU_VERSIONS ?= 22 24 26
PACKAGE_DIR ?= packages
SALT_PACKAGE_VERSION ?= 0.0.0
UBUNTU_VERSION ?= 24
SALT_PACKAGE_RELEASE ?= 1ubuntu$(UBUNTU_VERSION)

# Static binary configuration
STATIC_BIN = salt-static
STATIC_LDFLAGS ?= -static $(HARDEN_LDFLAGS) -Wl,--gc-sections
STATIC_LDLIBS ?= -l:libsodium.a -lpthread
STATIC_CFLAGS ?= $(BASE_CFLAGS) $(HARDEN_CFLAGS) -ffunction-sections -fdata-sections

# Audit binaries (unstripped for symbol checks)
AUDIT_BIN = salt.audit
AUDIT_STATIC_BIN = salt-static.audit

# Fuzz binaries
FUZZ_PARSE_BIN = fuzz_parse
FUZZ_CLI_BIN = fuzz_cli
FUZZ_OUTPUT_BIN = fuzz_output
FUZZ_BOUNDARY_BIN = fuzz_boundary
FUZZ_ROUNDTRIP_BIN = fuzz_roundtrip
FUZZ_PARSE_CORPUS_DIR ?= $(FUZZ_CORPUS_DIR)/parse/corpus
FUZZ_CLI_CORPUS_DIR ?= $(FUZZ_CORPUS_DIR)/cli/corpus
FUZZ_OUTPUT_CORPUS_DIR ?= $(FUZZ_CORPUS_DIR)/output/corpus
FUZZ_BOUNDARY_CORPUS_DIR ?= $(FUZZ_CORPUS_DIR)/boundary/corpus
FUZZ_ROUNDTRIP_CORPUS_DIR ?= $(FUZZ_CORPUS_DIR)/roundtrip/corpus

# QA helper
VERIFY_ROUNDTRIP_HELPER = verify_roundtrip_helper

CLEANFILES = $(SAN_BIN) $(VALGRIND_BIN) $(COV_BIN) \
$(BENCH_BIN) $(BENCH_SPAWN_BIN) \
$(FUZZ_PARSE_BIN) $(FUZZ_CLI_BIN) $(FUZZ_OUTPUT_BIN) $(FUZZ_BOUNDARY_BIN) $(FUZZ_ROUNDTRIP_BIN) \
$(STATIC_BIN) $(AUDIT_BIN) $(AUDIT_STATIC_BIN) $(VERIFY_ROUNDTRIP_HELPER) \
*.gcda *.gcno *.gcov *.gcov.json.gz

# ============================================================================
# Compatibility Aliases
# ============================================================================

.PHONY: build
build: all

.PHONY: test
test: check

# ============================================================================
# Memory Safety Testing
# ============================================================================

if ENABLE_TESTS
test-sanitize:
	$(MAKE) --no-print-directory "$(SAN_BIN)"
	ASAN_OPTIONS="$(SAN_ASAN_OPTIONS)" UBSAN_OPTIONS="$(SAN_UBSAN_OPTIONS)" "./$(SAN_BIN)"

$(SAN_BIN): $(TEST_SRC_FILES) $(TEST_HEADER_FILES)
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CMOCKA_CFLAGS) $(CPPFLAGS) -DSALT_NO_MAIN \
	$(BASE_CFLAGS) $(HARDEN_CFLAGS) $(CFLAGS) $(SAN_FLAGS) \
	$(TEST_SRC_FILES) -o "$@" \
	$(HARDEN_LDFLAGS) $(LDFLAGS) $(SAN_FLAGS) $(CMOCKA_LIBS) $(SODIUM_LIBS)

test-valgrind:
	$(MAKE) --no-print-directory "$(VALGRIND_BIN)"
if HAVE_VALGRIND
	"$(VALGRIND)" $(VALGRIND_FLAGS) "./$(VALGRIND_BIN)"
else
	@echo "FAIL: valgrind is required for test-valgrind but was not found at configure time" >&2
	@exit 1
endif

test-valgrind-cli: build
if HAVE_VALGRIND
	@printf '%s' 'hello' | "$(VALGRIND)" $(VALGRIND_FLAGS) ./salt --key "$(VALGRIND_BENCH_KEY_B64)" - >/dev/null
	@printf '%s' 'hello' | "$(VALGRIND)" $(VALGRIND_FLAGS) ./salt --output json --key "$(VALGRIND_BENCH_KEY_B64)" --key-id "kid-vg" - >/dev/null
	@printf '%s' '{"key_id":"kid-vg","key":"$(VALGRIND_BENCH_KEY_B64)"}' | "$(VALGRIND)" $(VALGRIND_FLAGS) ./salt --output json --key - "hello" >/dev/null
else
	@echo "FAIL: valgrind is required for test-valgrind-cli but was not found at configure time" >&2
	@exit 1
endif

$(VALGRIND_BIN): $(TEST_SRC_FILES) $(TEST_HEADER_FILES)
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CMOCKA_CFLAGS) $(CPPFLAGS) -DSALT_NO_MAIN \
	-std=c17 -Wall -Wextra -Wpedantic -O1 -g \
	$(TEST_SRC_FILES) -o "$@" \
	$(HARDEN_LDFLAGS) $(LDFLAGS) $(CMOCKA_LIBS) $(SODIUM_LIBS)
endif

# ============================================================================
# Clang Testing
# ============================================================================

.PHONY: test-clang
test-clang:
	$(MAKE) --no-print-directory test CC=$(CLANG)

# ============================================================================
# Static Binary Build
# ============================================================================

.PHONY: static
static: $(STATIC_BIN)

$(STATIC_BIN): $(SALT_SRC_FILES) include/salt.h
	$(CC) $(STATIC_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" \
	$(SALT_SRC_FILES) -o $@ $(STATIC_LDFLAGS) $(STATIC_LDLIBS)
if ENABLE_DEBUG
	@:
else
	$(STRIP) $@
endif

.PHONY: static-check
static-check: $(STATIC_BIN)
	@if ldd $(STATIC_BIN) 2>&1 | grep -qE 'not a dynamic executable|statically linked'; then \
	echo "OK: $(STATIC_BIN) is statically linked"; \
else \
	echo "FAIL: $(STATIC_BIN) reports dynamic dependencies:"; \
	ldd $(STATIC_BIN); \
	exit 1; \
	fi
	@./$(STATIC_BIN) --version >/dev/null
	@echo "OK: $(STATIC_BIN) executes"

# ============================================================================
# Coverage Testing
# ============================================================================

.PHONY: coverage-check
coverage-check: $(COV_BIN)
	./$(COV_BIN)
	@gcovr --root $(top_srcdir) --object-directory . --filter '(.*/)?src/(salt|main)\.c$$' --exclude-function-lines --merge-mode-functions=merge-use-line-min --txt-summary > gcovr.txt
	@rm -f *.gcov.json.gz && gcov -j -a -o . salt_cov.o main_cov.o >/dev/null
	@python3 $(top_srcdir)/scripts/gcov_blocks.py > gcov_blocks.txt
	@cat gcovr.txt
	@cat gcov_blocks.txt
	@line_stats=$$(awk '/^lines:/ {gsub(/%/, "", $$2); gsub(/[()]/, "", $$3); gsub(/[()]/, "", $$6); print $$2, $$3, $$6}' gcovr.txt); \
	function_stats=$$(awk '/^functions:/ {gsub(/%/, "", $$2); gsub(/[()]/, "", $$3); gsub(/[()]/, "", $$6); print $$2, $$3, $$6}' gcovr.txt); \
	block_stats=$$(awk '/^blocks:/ {gsub(/%/, "", $$2); gsub(/[()]/, "", $$3); gsub(/[()]/, "", $$6); print $$2, $$3, $$6}' gcov_blocks.txt); \
	branch_stats=$$(awk '/^branches:/ {gsub(/%/, "", $$2); gsub(/[()]/, "", $$3); gsub(/[()]/, "", $$6); print $$2, $$3, $$6}' gcovr.txt); \
if [ -z "$$line_stats" ] || [ -z "$$function_stats" ] || [ -z "$$block_stats" ] || [ -z "$$branch_stats" ]; then \
	echo "FAIL: unable to determine line/function/block/branch coverage"; \
	exit 1; \
	fi; \
	set -- $$line_stats; line_pct=$$1; line_covered=$$2; line_total=$$3; \
	set -- $$function_stats; function_pct=$$1; function_covered=$$2; function_total=$$3; \
	set -- $$block_stats; block_pct=$$1; block_covered=$$2; block_total=$$3; \
	set -- $$branch_stats; branch_pct=$$1; branch_covered=$$2; branch_total=$$3; \
if [ "$$line_total" -le 0 ] || [ "$$function_total" -le 0 ] || [ "$$block_total" -le 0 ] || [ "$$branch_total" -le 0 ]; then \
	echo "FAIL: unable to determine line/function/block/branch coverage"; \
	exit 1; \
	fi; \
if [ $$((line_covered * 100)) -le $$(( $(COVERAGE) * line_total )) ]; then \
	echo "FAIL: line coverage $$line_pct% <= $(COVERAGE)%"; \
	exit 1; \
	fi; \
if [ $$((function_covered * 100)) -le $$(( $(FUNCTION_COVERAGE) * function_total )) ]; then \
	echo "FAIL: function coverage $$function_pct% <= $(FUNCTION_COVERAGE)%"; \
	exit 1; \
	fi; \
if [ $$((block_covered * 100)) -le $$(( $(BLOCK_COVERAGE) * block_total )) ]; then \
	echo "FAIL: block coverage $$block_pct% <= $(BLOCK_COVERAGE)%"; \
	exit 1; \
	fi; \
if [ $$((branch_covered * 100)) -le $$(( $(BRANCH_COVERAGE) * branch_total )) ]; then \
	echo "FAIL: branch coverage $$branch_pct% <= $(BRANCH_COVERAGE)%"; \
	exit 1; \
	fi; \
	echo "OK: line coverage $$line_pct% > $(COVERAGE)%"; \
	echo "OK: function coverage $$function_pct% > $(FUNCTION_COVERAGE)%"; \
	echo "OK: block coverage $$block_pct% > $(BLOCK_COVERAGE)%"; \
	echo "OK: branch coverage $$branch_pct% > $(BRANCH_COVERAGE)%"

.PHONY: coverage-html
coverage-html: $(COV_BIN)
	@mkdir -p $(COVERAGE_HTML_DIR)/light $(COVERAGE_HTML_DIR)/dark
	@gcovr --root $(top_srcdir) --object-directory . --filter '(.*/)?src/(salt|main)\.c$$' \
	--merge-mode-functions=merge-use-line-min \
	--exclude-function-lines \
	--html-details $(COVERAGE_HTML_DIR)/light/index.html \
	--html-title "salt coverage (Solarized Light)" \
	--html-css $(COVERAGE_CSS_LIGHT)
	@gcovr --root $(top_srcdir) --object-directory . --filter '(.*/)?src/(salt|main)\.c$$' \
	--merge-mode-functions=merge-use-line-min \
	--exclude-function-lines \
	--html-details $(COVERAGE_HTML_DIR)/dark/index.html \
	--html-title "salt coverage (Solarized Dark)" \
	--html-css $(COVERAGE_CSS_DARK)
	@echo "Generated: $(COVERAGE_HTML_DIR)/light/index.html"
	@echo "Generated: $(COVERAGE_HTML_DIR)/dark/index.html"

$(COV_BIN): $(TEST_SRC_FILES) $(TEST_HEADER_FILES)
	rm -f *.gcda *.gcno *.gcov
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/test_main.c -o test_main_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/salt.c -o salt_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/args.c -o cli_args_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/input.c -o cli_input_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/prepare.c -o cli_prepare_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/test_hooks.c -o cli_test_hooks_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/parse.c -o cli_parse_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/output.c -o cli_output_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/state.c -o cli_state_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/signal.c -o cli_signal_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/cli/stream.c -o cli_stream_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/src/main.c -o main_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/test_salt.c -o test_salt_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_support.c -o support_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_contract.c -o contract_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_io.c -o io_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_json.c -o json_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_runtime.c -o runtime_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_helpers.c -o helpers_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/cli/test_characterization.c -o characterization_cov.o
	$(CC) $(COV_BUILD_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" $(CMOCKA_CFLAGS) $(COV_FLAGS) -DSALT_NO_MAIN -c $(top_srcdir)/tests/test_fuzz_regressions.c -o test_fuzz_regressions_cov.o
	$(CC) $(COV_FLAGS) test_main_cov.o test_salt_cov.o \
	                   support_cov.o contract_cov.o \
	                   io_cov.o json_cov.o \
	                   runtime_cov.o helpers_cov.o \
	                   characterization_cov.o test_fuzz_regressions_cov.o \
	                   salt_cov.o cli_args_cov.o cli_input_cov.o cli_prepare_cov.o cli_test_hooks_cov.o cli_parse_cov.o \
	                   cli_state_cov.o cli_signal_cov.o cli_stream_cov.o \
	                   cli_output_cov.o main_cov.o -o $@ $(LDFLAGS) $(CMOCKA_LIBS) $(SODIUM_LIBS)

# ============================================================================
# Fuzzing
# ============================================================================

.PHONY: fuzz-build
fuzz-build: $(FUZZ_PARSE_BIN) $(FUZZ_CLI_BIN) $(FUZZ_OUTPUT_BIN) $(FUZZ_BOUNDARY_BIN) $(FUZZ_ROUNDTRIP_BIN)

$(FUZZ_PARSE_BIN): $(top_srcdir)/fuzz/fuzz_parse.c $(top_srcdir)/fuzz/fuzz_common.h $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/salt.c $(top_srcdir)/include/salt.h $(top_srcdir)/src/cli/internal.h
	$(FUZZ_CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(FUZZ_FLAGS) \
	$(top_srcdir)/fuzz/fuzz_parse.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/salt.c \
	-o $@ $(LDFLAGS) $(SODIUM_LIBS)

$(FUZZ_CLI_BIN): $(top_srcdir)/fuzz/fuzz_cli.c $(top_srcdir)/fuzz/fuzz_common.h $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c $(top_srcdir)/src/cli/signal.c $(top_srcdir)/src/cli/stream.c $(top_srcdir)/src/salt.c
	$(FUZZ_CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(FUZZ_FLAGS) -DSALT_NO_MAIN \
	$(top_srcdir)/fuzz/fuzz_cli.c $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c \
	$(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c $(top_srcdir)/src/cli/signal.c \
	$(top_srcdir)/src/cli/stream.c $(top_srcdir)/src/salt.c \
	-o $@ $(LDFLAGS) $(SODIUM_LIBS)

$(FUZZ_OUTPUT_BIN): $(top_srcdir)/fuzz/fuzz_output.c $(top_srcdir)/fuzz/fuzz_common.h $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/salt.c
	$(FUZZ_CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(FUZZ_FLAGS) \
	$(top_srcdir)/fuzz/fuzz_output.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/salt.c \
	-o $@ $(LDFLAGS) $(SODIUM_LIBS)

$(FUZZ_BOUNDARY_BIN): $(top_srcdir)/fuzz/fuzz_boundary.c $(top_srcdir)/fuzz/fuzz_common.h $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c $(top_srcdir)/src/cli/signal.c $(top_srcdir)/src/cli/stream.c $(top_srcdir)/src/salt.c
	$(FUZZ_CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(FUZZ_FLAGS) -DSALT_NO_MAIN \
	$(top_srcdir)/fuzz/fuzz_boundary.c $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c \
	$(top_srcdir)/src/cli/output.c $(top_srcdir)/src/cli/state.c $(top_srcdir)/src/cli/signal.c \
	$(top_srcdir)/src/cli/stream.c $(top_srcdir)/src/salt.c \
	-o $@ $(LDFLAGS) $(SODIUM_LIBS)

$(FUZZ_ROUNDTRIP_BIN): $(top_srcdir)/fuzz/fuzz_roundtrip.c $(top_srcdir)/fuzz/fuzz_common.h $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/salt.c
	$(FUZZ_CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(FUZZ_FLAGS) \
	$(top_srcdir)/fuzz/fuzz_roundtrip.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/salt.c \
	-o $@ $(LDFLAGS) $(SODIUM_LIBS)

define run-fuzz
@mkdir -p $(2) $(FUZZ_STATE_DIR)/$(3)/crashes
ASAN_OPTIONS=$(FUZZ_ASAN_OPTIONS) UBSAN_OPTIONS=$(FUZZ_UBSAN_OPTIONS) \
  $(1) $(FUZZ_LIBFUZZER_FLAGS) -max_len=$(5) -max_total_time=$(4) \
  -artifact_prefix=$(FUZZ_STATE_DIR)/$(3)/crashes/ \
  $(if $(6),-dict=$(top_srcdir)/$(6)) $(2) $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/$(3)
endef

define replay-fuzz
@if [ -d "$(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/$(2)" ]; then \
  ASAN_OPTIONS=$(FUZZ_ASAN_OPTIONS) UBSAN_OPTIONS=$(FUZZ_UBSAN_OPTIONS) \
    $(1) -runs=0 -max_len=$(3) $(FUZZ_LIBFUZZER_FLAGS) \
    $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/$(2) ; \
fi
@if [ -d "$(FUZZ_STATE_DIR)/$(2)/crashes" ] && [ -n "$$(ls -A $(FUZZ_STATE_DIR)/$(2)/crashes 2>/dev/null)" ]; then \
  for crash in $(FUZZ_STATE_DIR)/$(2)/crashes/*; do \
    echo "==> replay $$crash"; \
    ASAN_OPTIONS=$(FUZZ_ASAN_OPTIONS) UBSAN_OPTIONS=$(FUZZ_UBSAN_OPTIONS) \
      $(1) -max_len=$(3) "$$crash" || exit $$?; \
  done; \
fi
endef

.PHONY: fuzz-run
fuzz-run: fuzz-build
	$(call run-fuzz,./$(FUZZ_PARSE_BIN),$(FUZZ_PARSE_CORPUS_DIR),parse,$(FUZZ_MAX_TOTAL_TIME),$(FUZZ_MAX_LEN_PARSE),fuzz/dictionaries/parse.dict)
	$(call run-fuzz,./$(FUZZ_CLI_BIN),$(FUZZ_CLI_CORPUS_DIR),cli,$(FUZZ_MAX_TOTAL_TIME),$(FUZZ_MAX_LEN_CLI),fuzz/dictionaries/cli.dict)
	$(call run-fuzz,./$(FUZZ_OUTPUT_BIN),$(FUZZ_OUTPUT_CORPUS_DIR),output,$(FUZZ_MAX_TOTAL_TIME),$(FUZZ_MAX_LEN_OUTPUT),fuzz/dictionaries/output.dict)
	$(call run-fuzz,./$(FUZZ_BOUNDARY_BIN),$(FUZZ_BOUNDARY_CORPUS_DIR),boundary,$(FUZZ_MAX_TOTAL_TIME),$(FUZZ_MAX_LEN_BOUNDARY),fuzz/dictionaries/boundary.dict)
	$(call run-fuzz,./$(FUZZ_ROUNDTRIP_BIN),$(FUZZ_ROUNDTRIP_CORPUS_DIR),roundtrip,$(FUZZ_MAX_TOTAL_TIME),$(FUZZ_MAX_LEN_ROUNDTRIP),fuzz/dictionaries/roundtrip.dict)

.PHONY: fuzz-smoke
fuzz-smoke: fuzz-build
	$(call run-fuzz,./$(FUZZ_PARSE_BIN),$(FUZZ_PARSE_CORPUS_DIR),parse,$(FUZZ_SMOKE_TIME),$(FUZZ_MAX_LEN_PARSE),fuzz/dictionaries/parse.dict)
	$(call run-fuzz,./$(FUZZ_CLI_BIN),$(FUZZ_CLI_CORPUS_DIR),cli,$(FUZZ_SMOKE_TIME),$(FUZZ_MAX_LEN_CLI),fuzz/dictionaries/cli.dict)
	$(call run-fuzz,./$(FUZZ_OUTPUT_BIN),$(FUZZ_OUTPUT_CORPUS_DIR),output,$(FUZZ_SMOKE_TIME),$(FUZZ_MAX_LEN_OUTPUT),fuzz/dictionaries/output.dict)
	$(call run-fuzz,./$(FUZZ_BOUNDARY_BIN),$(FUZZ_BOUNDARY_CORPUS_DIR),boundary,$(FUZZ_SMOKE_TIME),$(FUZZ_MAX_LEN_BOUNDARY),fuzz/dictionaries/boundary.dict)
	$(call run-fuzz,./$(FUZZ_ROUNDTRIP_BIN),$(FUZZ_ROUNDTRIP_CORPUS_DIR),roundtrip,$(FUZZ_SMOKE_TIME),$(FUZZ_MAX_LEN_ROUNDTRIP),fuzz/dictionaries/roundtrip.dict)

.PHONY: fuzz-regression
fuzz-regression: fuzz-build
	$(call replay-fuzz,./$(FUZZ_PARSE_BIN),parse,$(FUZZ_MAX_LEN_PARSE))
	$(call replay-fuzz,./$(FUZZ_CLI_BIN),cli,$(FUZZ_MAX_LEN_CLI))
	$(call replay-fuzz,./$(FUZZ_OUTPUT_BIN),output,$(FUZZ_MAX_LEN_OUTPUT))
	$(call replay-fuzz,./$(FUZZ_BOUNDARY_BIN),boundary,$(FUZZ_MAX_LEN_BOUNDARY))
	$(call replay-fuzz,./$(FUZZ_ROUNDTRIP_BIN),roundtrip,$(FUZZ_MAX_LEN_ROUNDTRIP))

.PHONY: fuzz-minimize
fuzz-minimize: fuzz-build
	@for h in parse cli output boundary roundtrip; do \
	  mkdir -p $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/$$h $(FUZZ_STATE_DIR)/$$h/corpus; \
	done
	./$(FUZZ_PARSE_BIN) -max_len=$(FUZZ_MAX_LEN_PARSE) -merge=1 $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/parse $(FUZZ_PARSE_CORPUS_DIR)
	./$(FUZZ_CLI_BIN) -max_len=$(FUZZ_MAX_LEN_CLI) -merge=1 $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/cli $(FUZZ_CLI_CORPUS_DIR)
	./$(FUZZ_OUTPUT_BIN) -max_len=$(FUZZ_MAX_LEN_OUTPUT) -merge=1 $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/output $(FUZZ_OUTPUT_CORPUS_DIR)
	./$(FUZZ_BOUNDARY_BIN) -max_len=$(FUZZ_MAX_LEN_BOUNDARY) -merge=1 $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/boundary $(FUZZ_BOUNDARY_CORPUS_DIR)
	./$(FUZZ_ROUNDTRIP_BIN) -max_len=$(FUZZ_MAX_LEN_ROUNDTRIP) -merge=1 $(top_srcdir)/$(FUZZ_SEED_CORPUS_ROOT)/roundtrip $(FUZZ_ROUNDTRIP_CORPUS_DIR)

.PHONY: fuzz
fuzz: fuzz-run

# ============================================================================
# Benchmarking
# ============================================================================

.PHONY: bench-build
bench-build: $(BENCH_BIN) $(BENCH_SPAWN_BIN)

$(BENCH_BIN): $(BENCH_SRC) $(top_srcdir)/bench/bench_common.h $(top_srcdir)/bench/bench_cases.h
	$(CC) $(BENCH_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" \
	-I$(top_srcdir)/bench $(BENCH_SRC) -o $@ $(LDFLAGS) $(SODIUM_LIBS) -lm

$(BENCH_SPAWN_BIN): $(BENCH_SPAWN_SRC) $(top_srcdir)/bench/bench_common.h
	$(CC) $(BENCH_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/bench \
	$(BENCH_SPAWN_SRC) -o $@ $(LDFLAGS) -lm

.PHONY: bench
bench: bench-build build
	./$(BENCH_BIN) --text
	./$(BENCH_SPAWN_BIN) --text --binary ./salt

.PHONY: bench-check
bench-check: bench-build build
	@mkdir -p $(BENCH_RESULTS_DIR)
	@if [ ! -f $(BENCH_BASELINE) ]; then \
	echo "FAIL: baseline $(BENCH_BASELINE) missing; run 'make bench-baseline'"; \
	exit 1; \
	fi
	./$(BENCH_BIN) --text --json $(BENCH_RESULTS_MICRO)
	$(top_srcdir)/scripts/bench-compare.sh --include-regex "$(BENCH_COMPARE_MICRO_REGEX)" \
	--threshold-override '^parse_auto_json_whitespace$$:150:200' \
	--threshold-override '^output_emit_json_large$$:120:200' \
	$(BENCH_BASELINE) $(BENCH_RESULTS_MICRO) $(BENCH_REGRESSION_PCT) $(BENCH_P95_REGRESSION_PCT)

.PHONY: bench-check-advisory
bench-check-advisory: bench-build build
	@mkdir -p $(BENCH_RESULTS_DIR)
	@if [ ! -f $(BENCH_BASELINE) ]; then \
	echo "FAIL: baseline $(BENCH_BASELINE) missing; run 'make bench-baseline'"; \
	exit 1; \
	fi
	./$(BENCH_SPAWN_BIN) --text --binary ./salt --json $(BENCH_RESULTS_SPAWN)
	$(top_srcdir)/scripts/bench-compare.sh --warn-only --include-regex "$(BENCH_COMPARE_SPAWN_REGEX)" $(BENCH_BASELINE) $(BENCH_RESULTS_SPAWN) $(BENCH_COLD_START_REGRESSION_PCT) $(BENCH_COLD_START_P95_REGRESSION_PCT)

.PHONY: bench-baseline
bench-baseline: bench-build build
	@mkdir -p $(BENCH_RESULTS_DIR)
	./$(BENCH_BIN) --text --json $(BENCH_RESULTS_MICRO)
	./$(BENCH_SPAWN_BIN) --text --binary ./salt --json $(BENCH_RESULTS_SPAWN)
	cat $(BENCH_RESULTS_MICRO) $(BENCH_RESULTS_SPAWN) > $(BENCH_RESULTS)
	cp $(BENCH_RESULTS) $(BENCH_BASELINE)
	@echo "Updated $(BENCH_BASELINE) from $(BENCH_RESULTS_MICRO) and $(BENCH_RESULTS_SPAWN)"

# ============================================================================
# Linting and Static Analysis
# ============================================================================

.PHONY: install-actionlint
install-actionlint:
	@mkdir -p tools
	@tarball=tools/actionlint_$(ACTIONLINT_VERSION)_linux_amd64.tar.gz; \
	curl -fsSL -o "$$tarball" "https://github.com/rhysd/actionlint/releases/download/v$(ACTIONLINT_VERSION)/actionlint_$(ACTIONLINT_VERSION)_linux_amd64.tar.gz"; \
	echo "$(ACTIONLINT_SHA256)  $$tarball" | sha256sum -c -; \
	tar -xz -C tools -f "$$tarball" actionlint; \
	rm -f "$$tarball"; \
	chmod +x $(ACTIONLINT_BIN)

.PHONY: actionlint
actionlint: install-actionlint
	./$(ACTIONLINT_BIN) $(top_srcdir)/.github/workflows/*.yml

.PHONY: tidy
tidy:
	$(CLANG_TIDY) $(FORMAT_SRCS) -- $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" -DSALT_NO_MAIN

.PHONY: format
format:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS)

.PHONY: format-apply
format-apply:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

.PHONY: cppcheck
cppcheck:
	$(CPPCHECK) --quiet --enable=warning,performance,portability --error-exitcode=1 \
	            --std=c17 --inline-suppr --suppress=missingIncludeSystem \
	            -I $(top_srcdir)/include $(top_srcdir)/src $(top_srcdir)/include $(top_srcdir)/tests $(top_srcdir)/fuzz

.PHONY: shellcheck
shellcheck:
	@if [ -n "$$(find $(top_srcdir)/examples $(top_srcdir)/scripts -name '*.sh' 2>/dev/null)" ]; then \
	$(SHELLCHECK) $$(find $(top_srcdir)/examples $(top_srcdir)/scripts -name '*.sh'); \
	else \
	echo "No shell scripts found for shellcheck"; \
	fi

.PHONY: codespell
codespell:
	$(CODESPELL) $(CODESPELL_ARGS) $(top_srcdir)

.PHONY: lint-suppressions
lint-suppressions:
	$(top_srcdir)/scripts/validate-lint-suppressions.sh $(top_srcdir)

.PHONY: scan-build
scan-build:
	@mkdir -p scan-build-out
	scan-build-18 -o scan-build-out --status-bugs \
	  -enable-checker security \
	  -enable-checker unix \
	  -enable-checker deadcode \
	  -enable-checker nullability \
	  -disable-checker security.insecureAPI.DeprecatedOrUnsafeBufferHandling \
	  $(CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" -DSALT_NO_MAIN -fsyntax-only $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/salt.c

.PHONY: lint
lint: tidy format cppcheck shellcheck codespell scan-build
	$(CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" -fsyntax-only $(top_srcdir)/src/main.c $(top_srcdir)/src/cli/args.c $(top_srcdir)/src/cli/input.c $(top_srcdir)/src/cli/prepare.c $(top_srcdir)/src/cli/test_hooks.c $(top_srcdir)/src/cli/parse.c $(top_srcdir)/src/cli/output.c $(top_srcdir)/src/salt.c
	$(CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" -DSALT_NO_MAIN -fsyntax-only $(top_srcdir)/tests/test_salt.c

# ============================================================================
# Documentation and Man Page Validation
# ============================================================================

.PHONY: docs
docs:
	@test -f $(top_srcdir)/README.md || { echo "FAIL: README.md missing"; exit 1; }
	@test -f $(top_srcdir)/man/salt.1 || { echo "FAIL: man/salt.1 missing"; exit 1; }
	@echo "OK: documentation assets present"

.PHONY: man
man: build
	@command -v man >/dev/null 2>&1 || { echo "man command not found"; exit 1; }
	@help_text="$$(./salt --help)"; \
	for opt in --key --key-id --output --key-format --help --version; do \
	echo "$$help_text" | grep -q -- "$$opt" || { echo "FAIL: option $$opt missing from CLI help"; exit 1; }; \
	grep -q -- "$$opt" $(top_srcdir)/man/salt.1 || { echo "FAIL: option $$opt missing from man/salt.1"; exit 1; }; \
	done
	man -l $(top_srcdir)/man/salt.1 >/dev/null
	@echo "OK: man page validated"

.PHONY: validate-copilot
validate-copilot:
	@$(top_srcdir)/scripts/validate-copilot-config.sh $(top_srcdir) ; rc=$$? ; \
	if [ $$rc -eq 0 ] || [ $$rc -eq 2 ]; then exit 0 ; else exit $$rc ; fi

# ============================================================================
# QA Testing
# ============================================================================

.PHONY: verify-roundtrip-helper
verify-roundtrip-helper: $(VERIFY_ROUNDTRIP_HELPER)

$(VERIFY_ROUNDTRIP_HELPER): $(top_srcdir)/scripts/verify-roundtrip-helper.c
	$(CC) $(BASE_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include $(top_srcdir)/scripts/verify-roundtrip-helper.c -o $@ $(LDFLAGS) $(SODIUM_LIBS)

.PHONY: test-qa
test-qa: build verify-roundtrip-helper
	$(top_srcdir)/scripts/qa.sh

.PHONY: test-build
test-build: check

# ============================================================================
# Docker and Package Testing
# ============================================================================

.PHONY: test-docker
test-docker:
	@set -e; \
	for v in $(DOCKER_UBUNTU_VERSIONS); do \
	echo "==> docker/ubuntu/$$v/Dockerfile"; \
	$(DOCKER) build -f $(top_srcdir)/docker/ubuntu/$$v/Dockerfile -t salt-test:$$v $(top_srcdir); \
	done

.PHONY: package-deb-local
package-deb-local:
	@mkdir -p $(PACKAGE_DIR)
	@command -v dpkg-deb >/dev/null 2>&1 || { echo "ERROR: dpkg-deb is required for package-deb-local" >&2; exit 1; }
	$(MAKE) clean
	$(MAKE) build SALT_VERSION="$(SALT_PACKAGE_VERSION)" CPPFLAGS="-I$(top_srcdir)/include"
	$(top_srcdir)/scripts/build-deb-package.sh \
		--binary "$(PWD)/salt" \
		--manpage "$(top_srcdir)/man/salt.1" \
		--version "$(SALT_PACKAGE_VERSION)" \
		--release "$(SALT_PACKAGE_RELEASE)" \
		--output "$(PACKAGE_DIR)/salt_$(SALT_PACKAGE_VERSION)-$(SALT_PACKAGE_RELEASE)_amd64.deb"
	@echo "Generated: $(PACKAGE_DIR)/salt_$(SALT_PACKAGE_VERSION)-$(SALT_PACKAGE_RELEASE)_amd64.deb"

.PHONY: package-deb-docker
package-deb-docker:
	@set -e; \
	for v in $(DEB_UBUNTU_VERSIONS); do \
	out_dir="$(PACKAGE_DIR)/ubuntu$$v"; \
	rm -rf "$$out_dir"; \
	mkdir -p "$$out_dir"; \
	echo "==> docker/ubuntu/$$v/Dockerfile deb-artifacts"; \
	DOCKER_BUILDKIT=1 $(DOCKER) build \
	  -f $(top_srcdir)/docker/ubuntu/$$v/Dockerfile \
	  --target deb-artifacts \
	  --build-arg SALT_PACKAGE_VERSION="$(SALT_PACKAGE_VERSION)" \
	  --output "type=local,dest=$$out_dir" \
	  $(top_srcdir); \
	ls -l "$$out_dir"/*.deb; \
	done

# ============================================================================
# Release and SBOM
# ============================================================================

.PHONY: release
release: build static static-check

$(AUDIT_BIN): $(SALT_SRCS) include/salt.h
	$(CC) $(BASE_CFLAGS) $(HARDEN_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" \
	$(AUDIT_SRC_FILES) -o $@ $(HARDEN_LDFLAGS) $(LDFLAGS) $(SODIUM_LIBS)

$(AUDIT_STATIC_BIN): $(SALT_SRCS) include/salt.h
	$(CC) $(STATIC_CFLAGS) $(CPPFLAGS) -I$(top_srcdir)/include -DSALT_VERSION=\"$(SALT_VERSION)\" \
	$(AUDIT_SRC_FILES) -o $@ $(STATIC_LDFLAGS) $(STATIC_LDLIBS)

.PHONY: symbol-audit
symbol-audit: $(AUDIT_BIN) $(AUDIT_STATIC_BIN)
	@set -e; \
	for bin in $(AUDIT_BIN) $(AUDIT_STATIC_BIN); do \
	echo "==> symbol-audit $$bin"; \
	leaked=$$(nm --defined-only --extern-only "$$bin" 2>/dev/null \
	  | awk '$$2=="T" {print $$3}' \
	  | grep -E '^salt_' \
	  | grep -E '(_test_|_set_test_hooks$$|_reset_test_hooks$$)' || true); \
if [ -n "$$leaked" ]; then \
	echo "FAIL: test-only symbols leaked into $$bin:"; \
	echo "$$leaked"; \
	exit 1; \
	fi; \
	unexpected=$$(nm --defined-only --extern-only "$$bin" 2>/dev/null \
	  | awk '$$2=="T" {print $$3}' \
	  | grep -E '^(salt_|main$$)' \
	  | grep -vxF $$(echo "$(SYMBOL_AUDIT_ALLOW)" | tr ' ' '\n' | sed 's/^/-e /') || true); \
if [ -n "$$unexpected" ]; then \
	echo "FAIL: unexpected exported symbols in $$bin:"; \
	echo "$$unexpected"; \
	echo "Update SYMBOL_AUDIT_ALLOW if intentional."; \
	exit 1; \
	fi; \
	total=$$(nm --defined-only "$$bin" 2>/dev/null | wc -l); \
	echo "OK: $$bin exports only allow-listed salt symbols ($$total total defined symbols)"; \
	done

.PHONY: install-syft
install-syft:
	@mkdir -p tools
	@tarball=tools/syft_$(SYFT_VERSION)_linux_amd64.tar.gz; \
	curl -fsSL -o "$$tarball" "https://github.com/anchore/syft/releases/download/v$(SYFT_VERSION)/syft_$(SYFT_VERSION)_linux_amd64.tar.gz"; \
	echo "$(SYFT_SHA256)  $$tarball" | sha256sum -c -; \
	tar -xz -C tools -f "$$tarball" syft; \
	rm -f "$$tarball"; \
	chmod +x $(SYFT_BIN)

.PHONY: install-parlay
install-parlay:
	@mkdir -p tools
	@tarball=tools/parlay_$(PARLAY_VERSION)_Linux_x86_64.tar.gz; \
	curl -fsSL -o "$$tarball" "https://github.com/snyk/parlay/releases/download/v$(PARLAY_VERSION)/parlay_Linux_x86_64.tar.gz"; \
	echo "$(PARLAY_SHA256)  $$tarball" | sha256sum -c -; \
	rm -rf $(PARLAY_BIN); \
	tar -xz -C tools -f "$$tarball" parlay; \
	rm -f "$$tarball"; \
	chmod +x $(PARLAY_BIN)

.PHONY: install-grype
install-grype:
	@mkdir -p tools
	@tarball=tools/grype_$(GRYPE_VERSION)_linux_amd64.tar.gz; \
	curl -fsSL -o "$$tarball" "https://github.com/anchore/grype/releases/download/v$(GRYPE_VERSION)/grype_$(GRYPE_VERSION)_linux_amd64.tar.gz"; \
	echo "$(GRYPE_SHA256)  $$tarball" | sha256sum -c -; \
	tar -xz -C tools -f "$$tarball" grype; \
	rm -f "$$tarball"; \
	chmod +x $(GRYPE_BIN)

.PHONY: sbom
sbom: release install-syft install-parlay
	@rm -rf $(SBOM_STAGING_DIR)
	@mkdir -p $(SBOM_STAGING_DIR)/dynamic $(SBOM_STAGING_DIR)/static $(SBOM_DIR)
	cp salt $(SBOM_STAGING_DIR)/dynamic/salt
	cp $(top_srcdir)/man/salt.1 $(SBOM_STAGING_DIR)/dynamic/salt.1
	cp $(STATIC_BIN) $(SBOM_STAGING_DIR)/static/salt-static
	cp $(top_srcdir)/man/salt.1 $(SBOM_STAGING_DIR)/static/salt.1
	SYFT_FILE_METADATA_SELECTION="$(SBOM_FILE_METADATA_SELECTION)" ./$(SYFT_BIN) dir:$(SBOM_STAGING_DIR)/dynamic \
	  --source-name "$(SBOM_SOURCE_NAME)" \
	  --source-supplier "$(SBOM_SOURCE_SUPPLIER)" \
	  --source-version "$(SBOM_VERSION)" \
	  -o spdx-json=$(SBOM_DYNAMIC) \
	  -o cyclonedx-json=$(SBOM_DYNAMIC_CYCLONEDX)
	SYFT_FILE_METADATA_SELECTION="$(SBOM_FILE_METADATA_SELECTION)" ./$(SYFT_BIN) dir:$(SBOM_STAGING_DIR)/static \
	  --source-name "$(SBOM_SOURCE_NAME)" \
	  --source-supplier "$(SBOM_SOURCE_SUPPLIER)" \
	  --source-version "$(SBOM_VERSION)" \
	  -o spdx-json=$(SBOM_STATIC) \
	  -o cyclonedx-json=$(SBOM_STATIC_CYCLONEDX)
	$(top_srcdir)/scripts/collect-ubuntu-deps.sh --output "$(SBOM_UBUNTU_DEPS)" --root "libsodium23:libsodium-runtime" --root "libsodium-dev:libsodium-build" --root "$(SBOM_BUILD_TOOL_CC):salt-build-tool" --root "$(SBOM_BUILD_TOOL_CLANG):salt-build-tool" --root "$(SBOM_BUILD_TOOL_MAKE):salt-build-tool" --root "$(SBOM_BUILD_TOOL_LIBC_DEV):salt-build-tool" --root "$(SBOM_BUILD_TOOL_LDD):salt-build-tool" --root "$(SBOM_BUILD_TOOL_BINUTILS):salt-build-tool" --root "$(SBOM_BUILD_TOOL_AUTOCONF):salt-build-tool" --root "$(SBOM_BUILD_TOOL_AUTOMAKE):salt-build-tool" --root "$(SBOM_BUILD_TOOL_LIBTOOL):salt-build-tool" --root "$(SBOM_BUILD_TOOL_PKG_CONFIG):salt-build-tool"
	$(top_srcdir)/scripts/add-libsodium-to-sbom.sh --project-url "$(PROJECT_URL)" --license "$(SBOM_LICENSE)" --copyright "$(SBOM_COPYRIGHT)" --version "$(LIBSODIUM_UPSTREAM_VERSION)" --salt-version "$(SBOM_VERSION)" --source-ref "$(SBOM_SOURCE_REF)" --artifact-file "salt=$(SBOM_STAGING_DIR)/dynamic/salt" --artifact-file "salt.1=$(SBOM_STAGING_DIR)/dynamic/salt.1" $(SBOM_DYNAMIC) $(SBOM_DYNAMIC_CYCLONEDX)
	$(top_srcdir)/scripts/add-libsodium-to-sbom.sh --project-url "$(PROJECT_URL)" --license "$(SBOM_LICENSE)" --copyright "$(SBOM_COPYRIGHT)" --version "$(LIBSODIUM_UPSTREAM_VERSION)" --salt-version "$(SBOM_VERSION)" --source-ref "$(SBOM_SOURCE_REF)" --artifact-file "salt-static=$(SBOM_STAGING_DIR)/static/salt-static" --artifact-file "salt.1=$(SBOM_STAGING_DIR)/static/salt.1" $(SBOM_STATIC) $(SBOM_STATIC_CYCLONEDX)
	$(top_srcdir)/scripts/add-ubuntu-provenance-to-sbom.sh --deps "$(SBOM_UBUNTU_DEPS)" $(SBOM_DYNAMIC_CYCLONEDX) $(SBOM_STATIC_CYCLONEDX)
	$(MAKE) --no-print-directory sbom-parlay-run
	$(top_srcdir)/scripts/validate-sbom.sh --salt-version "$(SBOM_VERSION)" --ubuntu-deps "$(SBOM_UBUNTU_DEPS)" --spdx "$(SBOM_DYNAMIC):salt" --spdx "$(SBOM_STATIC):salt-static" --cyclonedx "$(SBOM_DYNAMIC_CYCLONEDX):salt" --cyclonedx "$(SBOM_STATIC_CYCLONEDX):salt-static"

.PHONY: sbom-parlay
sbom-parlay: install-parlay
	$(MAKE) --no-print-directory sbom-parlay-run

.PHONY: sbom-vuln
sbom-vuln: sbom install-grype
	./$(GRYPE_BIN) sbom:$(SBOM_DYNAMIC_CYCLONEDX) --fail-on "$(GRYPE_FAIL_ON)" -o json > $(SBOM_DIR)/salt-linux-amd64.vuln.json
	./$(GRYPE_BIN) sbom:$(SBOM_STATIC_CYCLONEDX) --fail-on "$(GRYPE_FAIL_ON)" -o json > $(SBOM_DIR)/salt-static-linux-amd64.vuln.json

.PHONY: sbom-parlay-run
sbom-parlay-run:
	@set -e; \
	for sbom in $(PARLAY_SBOMS); do \
	test -f "$$sbom" || { echo "FAIL: missing SBOM $$sbom" >&2; exit 1; }; \
	tmp="$$sbom.parlay.tmp"; \
	echo "==> parlay ecosystems $$sbom"; \
	./$(PARLAY_BIN) ecosystems enrich "$$sbom" > "$$tmp"; \
	mv "$$tmp" "$$sbom"; \
	echo "==> parlay scorecard $$sbom"; \
	./$(PARLAY_BIN) scorecard enrich "$$sbom" > "$$tmp"; \
	mv "$$tmp" "$$sbom"; \
if [ "$(PARLAY_SNYK_ENRICH)" = "1" ]; then \
if [ -z "$${SNYK_TOKEN:-}" ]; then \
	echo "FAIL: PARLAY_SNYK_ENRICH=1 requires SNYK_TOKEN" >&2; \
	exit 1; \
	fi; \
	echo "==> parlay snyk $$sbom"; \
	./$(PARLAY_BIN) snyk enrich "$$sbom" > "$$tmp"; \
	mv "$$tmp" "$$sbom"; \
	fi; \
	rm -f "$$tmp"; \
	done

.PHONY: sbom-verify
sbom-verify:
	@set -euo pipefail; \
	echo "Verifying SBOM component parity..."; \
	dynamic_count=$$(jq '.components | length' $(SBOM_DYNAMIC_CYCLONEDX)); \
	static_count=$$(jq '.components | length' $(SBOM_STATIC_CYCLONEDX)); \
	echo "Dynamic SBOM: $$dynamic_count components"; \
	echo "Static SBOM: $$static_count components"; \
	if [ "$$dynamic_count" -lt 90 ]; then \
		echo "FAIL: Dynamic SBOM has $$dynamic_count components (expected >= 90)" >&2; \
		exit 1; \
	fi; \
	if [ "$$static_count" -lt 90 ]; then \
		echo "FAIL: Static SBOM has $$static_count components (expected >= 90)" >&2; \
		exit 1; \
	fi; \
	echo "OK: Component counts meet minimum threshold (90)"; \
	dynamic_spdx=$$(jq '.packages | length' $(SBOM_DYNAMIC)); \
	static_spdx=$$(jq '.packages | length' $(SBOM_STATIC)); \
	echo "Dynamic SPDX: $$dynamic_spdx packages"; \
	echo "Static SPDX: $$static_spdx packages"; \
	if [ "$$dynamic_spdx" -lt 90 ]; then \
		echo "FAIL: Dynamic SPDX has $$dynamic_spdx packages (expected >= 90)" >&2; \
		exit 1; \
	fi; \
	if [ "$$static_spdx" -lt 90 ]; then \
		echo "FAIL: Static SPDX has $$static_spdx packages (expected >= 90)" >&2; \
		exit 1; \
	fi; \
	echo "OK: SBOM component parity verified"

# ============================================================================
# ============================================================================
# CodeQL Analysis
# ============================================================================

CODEQL ?= codeql
CODEQL_DB ?= codeql-db
CODEQL_QUERIES ?= $(top_srcdir)/.github/codeql
CODEQL_FORMAT ?= csv
CODEQL_OUTPUT ?= codeql-results.csv

.PHONY: codeql-build
codeql-build: clean
	$(CODEQL) database create $(CODEQL_DB) --language=cpp --source-root=$(abs_top_srcdir) --command="$(MAKE) -C $(abs_builddir)" --overwrite

.PHONY: codeql-analyze
codeql-analyze:
	$(CODEQL) database analyze $(CODEQL_DB) $(CODEQL_QUERIES)/*.ql --format=$(CODEQL_FORMAT) --output=$(CODEQL_OUTPUT) --download

.PHONY: codeql
codeql: codeql-build codeql-analyze

clean-local:
	-rm -rf $(CODEQL_DB) $(CODEQL_DB)*
	-rm -f $(CODEQL_OUTPUT) codeql-results*.csv
	-rm -rf $(COVERAGE_HTML_DIR)
	-rm -f ci.log

# CI Gates
# ============================================================================

.PHONY: ci-fast
ci-fast: validate-copilot actionlint lint-suppressions lint test build

.PHONY: ci
ci: ci-fast test-sanitize test-valgrind test-valgrind-cli coverage-check test-qa fuzz-regression docs man symbol-audit

# ============================================================================
# Cleanup
# ============================================================================

.PHONY: clean-fuzz
clean-fuzz:
	@for h in parse cli output boundary roundtrip; do \
	  rm -rf $(FUZZ_STATE_DIR)/$$h/corpus $(FUZZ_STATE_DIR)/$$h/corpus.prev \
	         $(FUZZ_STATE_DIR)/$$h/logs $(FUZZ_STATE_DIR)/$$h/artifacts; \
	done
	rm -f $(FUZZ_STATE_DIR)/run.log $(FUZZ_STATE_DIR)/summary.txt
	@echo "Preserved: $(FUZZ_STATE_DIR)/*/crashes (replayed by 'make fuzz-regression')"

.PHONY: clean-fuzz-all
clean-fuzz-all:
	rm -rf $(FUZZ_STATE_DIR)

DISTCLEAN_LOCAL_DIRS = $(CODEQL_DB) $(FUZZ_STATE_DIR) $(BENCH_RESULTS_DIR) $(PACKAGE_DIR) $(SBOM_DIR) $(COVERAGE_HTML_DIR) scan-build-out \
                       src/.deps src/cli/.deps tests/.deps tests/cli/.deps
DISTCLEAN_LOCAL_FILES = $(CODEQL_OUTPUT) gcovr.txt gcov_blocks.txt ci.log
DISTCLEAN_BUILD_ROOT_DIRS = $(top_srcdir)/build/gpg-release-secrets \
                            $(top_srcdir)/build/gpg-release-rsa \
                            $(top_srcdir)/build/gpg-release-ecc
DISTCLEAN_REPO_DIRS = $(top_srcdir)/bin $(top_srcdir)/build-autotools \
                      $(top_srcdir)/build-test $(top_srcdir)/.fuzz $(top_srcdir)/terraform \
                      $(top_srcdir)/autom4te.cache $(top_srcdir)/build-aux \
                      $(top_srcdir)/.deps $(top_srcdir)/.libs
DISTCLEAN_REPO_FILES = $(top_srcdir)/salt-demo.gif $(top_srcdir)/salt-demo.mp4 \
                       $(top_srcdir)/vhs/salt-demo.gif $(top_srcdir)/vhs/salt-demo.mp4 \
                       $(top_srcdir)/salt-linux-amd64.tar.gz \
                       $(top_srcdir)/SHA256SUMS $(top_srcdir)/SHA256SUMS.asc $(top_srcdir)/release-notes.md \
                       $(top_srcdir)/configure $(top_srcdir)/aclocal.m4 $(top_srcdir)/config.h.in \
                       $(top_srcdir)/config.cache $(top_srcdir)/config.guess $(top_srcdir)/config.log \
                       $(top_srcdir)/config.status $(top_srcdir)/config.sub $(top_srcdir)/config.h \
                       $(top_srcdir)/stamp-h1 $(top_srcdir)/autoscan.log $(top_srcdir)/configure.scan \
                       $(top_srcdir)/libtool $(top_srcdir)/ltmain.sh $(top_srcdir)/Makefile.in \
                       $(top_srcdir)/m4/libtool.m4 $(top_srcdir)/m4/ltoptions.m4 \
                       $(top_srcdir)/m4/ltsugar.m4 $(top_srcdir)/m4/ltversion.m4 \
                       $(top_srcdir)/m4/lt~obsolete.m4

distclean-local:
	rm -rf $(DISTCLEAN_LOCAL_DIRS)
	rm -f $(DISTCLEAN_LOCAL_FILES)
	rm -rf $(DISTCLEAN_BUILD_ROOT_DIRS)
	-rmdir --ignore-fail-on-non-empty $(top_srcdir)/build 2>/dev/null || true
	rm -f $(ACTIONLINT_BIN) $(SYFT_BIN) $(PARLAY_BIN) $(GRYPE_BIN)
	-rmdir --ignore-fail-on-non-empty tools src/cli tests/cli src tests 2>/dev/null || true
	@if test "$(abs_top_builddir)" = "$(abs_top_srcdir)"; then \
	  rm -f $(DISTCLEAN_REPO_FILES) $(top_srcdir)/autoscan-*.log $(top_srcdir)/fuzz-*.log \
	        $(top_srcdir)/*.gcov $(top_srcdir)/*.gcda $(top_srcdir)/*.gcno \
	        $(top_srcdir)/core $(top_srcdir)/core.* $(top_srcdir)/*~ \
	        $(top_srcdir)/*.swp $(top_srcdir)/*.backup; \
	  find "$(top_srcdir)" -path "$(top_srcdir)/.git" -prune -o \
	    \( -name ".DS_Store" -o -name "Thumbs.db" -o -name "*.gcov" -o -name "*.gcda" -o -name "*.gcno" \
	       -o -name "fuzz-*.log" -o -name "*~" -o -name "*.swp" -o -name "*.backup" \) \
	    -exec rm -f {} +; \
	  rm -rf $(DISTCLEAN_REPO_DIRS); \
	fi

# ============================================================================
# Help
# ============================================================================

.PHONY: help
help:
	@echo "Common targets:"
	@echo "  all / build      - Build the salt binary (stripped unless --enable-debug)"
	@echo "  check / test     - Run unit tests"
	@echo "  lint             - Run static analysis and style checks"
	@echo "  docs             - Validate documentation artifacts"
	@echo "  man              - Validate man page source and rendering"
	@echo "  ci-fast          - Fast CI gate (lint + test + build)"
	@echo "  ci               - Full CI gate (all quality checks)"
	@echo "  install          - Install salt and man page"
	@echo "  clean            - Remove build artifacts"
	@echo "  distclean        - Remove build artifacts and generated files"
	@echo "  help             - Show this help output"
	@echo ""
	@echo "Advanced targets:"
	@echo "  validate-copilot - Validate Copilot customization files"
	@echo "  actionlint       - Lint GitHub Actions workflows"
	@echo "  codeql           - Create database and run custom CodeQL queries"
	@echo "  test-clang       - Build and test with clang"
	@echo "  test-sanitize    - Run tests with sanitizers"
	@echo "  test-valgrind    - Run tests with valgrind"
	@echo "  test-valgrind-cli - Run valgrind on real salt CLI flows"
	@echo "  test-qa          - Run QA helper checks"
	@echo "  test-docker      - Build/test in Ubuntu containers"
	@echo "  coverage-check   - Enforce coverage threshold"
	@echo "  fuzz-regression  - Replay fuzz corpus"
	@echo "  fuzz-smoke       - Quick fuzz pass"
	@echo "  bench-check      - Stable microbenchmark regression check"
	@echo "  bench-check-advisory - Advisory cold-start benchmark check"
	@echo "  symbol-audit     - Verify exported symbols and ABI policy"
	@echo "  release          - Build stripped production binaries"
	@echo "  sbom             - Generate SPDX/CycloneDX SBOMs"
	@echo "  sbom-verify      - Verify SBOM component parity (dynamic/static >= 90)"
	@echo "  package-deb-local - Build Debian package with dpkg-deb"
	@echo "  package-deb-docker - Build Debian package in Docker"
	@echo ""
	@echo "See README.md or TESTING.md for full documentation"
