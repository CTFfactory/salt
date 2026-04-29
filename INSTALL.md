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
sudo apt-get install -y libsodium-dev libcmocka-dev autoconf automake libtool pkg-config make man-db jq curl
```

Then run the canonical flow above.

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

