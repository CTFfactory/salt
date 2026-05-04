# Installation Guide

`INSTALL.md` is the source of truth for building and installing `salt`.
For testing, fuzzing, and quality gates, see **[TESTING.md](TESTING.md)**.

## Prerequisites

- C compiler (C11)
- GNU Make
- Autotools (`autoconf`, `automake`, `libtool`) when building from Git
- libsodium development package
- cmocka development package (when running tests)
- `man` command for man-page checks
- `jq` and `curl` for GitHub Actions Secrets API helper workflows

For fuller Ubuntu package coverage (lint/fuzz/container tooling), see
`docs/ubuntu-packages.md`.

## Canonical GNU Build Flow

Maintainers regenerate GNU build outputs only when build inputs change:

```sh
./bootstrap
```

Users and contributors build with out-of-tree Autotools:

```sh
mkdir -p build/autotools
cd build/autotools
../../configure --enable-tests
make
make check
sudo make install
salt --version
```

Canonical GNU equivalent:

```sh
./configure --enable-tests
make
make check
sudo make install
```

## Platform Setup

### Ubuntu/Debian

```sh
sudo apt-get update
sudo apt-get install -y build-essential libsodium-dev libcmocka-dev autoconf automake libtool pkg-config make man-db jq curl
```

Then run the canonical flow above.

## CI, Release, and Actions Requirements

Running the full CI matrix locally (`make ci`), generating releases, or editing GitHub Actions workflows requires additional tooling.

### Ubuntu/Debian CI Tools

```sh
sudo apt-get install -y \
    clang-18 clang-tidy-18 clang-format-18 \
    cppcheck \
    shellcheck \
    codespell \
    valgrind \
    gcovr \
    yamllint \
    python3-yaml
```

External binary dependencies:
- **[actionlint](https://github.com/rhysd/actionlint)**: Run automatically by `make lint` via the Makefile, pinned under `build/tools/actionlint`.
- **[vhs](https://github.com/charmbracelet/vhs)**: Used for deterministic terminal demo tape execution.
- **[syft](https://github.com/anchore/syft)**: Used by `make sbom` for release cycle metadata generation.

See [`docs/ubuntu-packages.md`](docs/ubuntu-packages.md) for more details on fuzzing (AFL++ / libFuzzer), compiler matrices, and optional developer ergonomics.

### Generic Linux

Install equivalents for:
- libsodium development headers/libs
- cmocka development headers/libs (for tests)
- Autotools toolchain (`autoconf`, `automake`, `libtool`)

Then run the canonical flow above.

## Build Variants and Artifacts

### Container build (Ubuntu matrix images)

```sh
docker build -f docker/ubuntu/24/Dockerfile -t salt:local .
docker run --rm salt:local --version
```

Replace `24` with `20`, `22`, or `26` as needed.

### Static binary

From an Autotools build directory:

```sh
make static
make static-check
```

### Debian packages

```sh
make package-deb-local SALT_PACKAGE_VERSION=0.0.0
make package-deb-docker SALT_PACKAGE_VERSION=0.0.0
```

### Release artifacts and SBOMs

Run release preparation from the configured out-of-tree build so local release
validation matches the GitHub tag workflow's GNU build flow:

```sh
cd build/autotools
make ci
make release SALT_VERSION=0.0.0
make coverage-html
make sbom SBOM_VERSION=0.0.0 SBOM_SOURCE_REF=refs/tags/v0.0.0
make package-deb-docker SALT_PACKAGE_VERSION=0.0.0
make sbom-vuln SBOM_VERSION=0.0.0 SBOM_SOURCE_REF=refs/tags/v0.0.0
```

`make release` builds the validated dynamic and static binaries (including
`make static-check`). `make coverage-html`, `make sbom`, and
`make package-deb-docker` produce the coverage, SBOM, and Debian-package inputs
that the GitHub `v*` release workflow publishes as archives, `.deb` packages,
and checksums.

Local SBOM outputs:
- `build/autotools/sbom/salt-linux-amd64.spdx.json`
- `build/autotools/sbom/salt-linux-amd64.cyclonedx.json`
- `build/autotools/sbom/salt-static-linux-amd64.spdx.json`
- `build/autotools/sbom/salt-static-linux-amd64.cyclonedx.json`
- `build/autotools/sbom/ubuntu-build-provenance.json`

## Advanced Build/Install Targets

Run from `build/autotools`:

```sh
make lint
make docs
make man
make ci-fast
make ci
make test-docker
make bench-check
```

Detailed test and fuzz workflows are maintained in **[TESTING.md](TESTING.md)**.

## Release Verification Logs

When building releases locally or debugging CI/CD release workflows, capture comprehensive build logs for reliable debugging and audit trails.

### Capturing Full Release Logs

To capture the complete release build including all tool versions and compiler output:

```sh
cd build/autotools
make ci 2>&1 | tee release.log
make release SALT_VERSION=0.0.0 2>&1 | tee -a release.log
make sbom SBOM_VERSION=0.0.0 SBOM_SOURCE_REF=refs/tags/v0.0.0 2>&1 | tee -a release.log
```

The `2>&1` redirects both stdout and stderr, and `tee` writes to both the log file and terminal so you can monitor progress while preserving all output.

### Including in Bug Reports

When reporting release or build issues, include:

1. Full release log (especially `make ci` and `make release` output)
2. Build environment details:
   ```sh
   uname -a
   gcc --version
   autoconf --version
   automake --version
   libtool --version
   pkg-config --version
   ```
3. Key tool versions:
   ```sh
   libsodium --version  # or pkg-config --modversion libsodium
   ```
4. Output of configuration step:
   ```sh
   cd build/autotools && ../../configure --enable-tests --version
   ```

### Example: Debugging a Failing Release

```sh
cd build/autotools
# Run with verbose output
make clean
../../configure --enable-tests --enable-debug
make V=1 2>&1 | tee build.log
make check 2>&1 | tee -a build.log
make ci 2>&1 | tee -a build.log
# Attach build.log to your issue
```

This captures Autotools configuration details, compiler flags, test output, and any warnings or errors in a single comprehensive log.

## Install Prefixes

Use standard Autotools prefixes as needed:

```sh
../../configure --enable-tests --prefix=/usr/local
make
make install
```

Use a user-local prefix to avoid `sudo`:

```sh
../../configure --enable-tests --prefix="$HOME/.local"
make
make install
```

