# syntax=docker/dockerfile:1
#
# Base image is pinned to an immutable digest per
# .github/instructions/dependency-pinning.instructions.md. To refresh, query
# the Docker Hub registry API for the current ubuntu:22.04 manifest digest
# and update UBUNTU_DIGEST below.
ARG UBUNTU_DIGEST=sha256:14be402d3f1eeeb5e7da73d3260322c68e7b51c88388f53e88eb21d6450bd520

FROM ubuntu:22.04@${UBUNTU_DIGEST} AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.71-2         automake=1:1.16.5-1.3         clang=1:14.0-55~exp2         gcc=4:11.2.0-1ubuntu1         libc6-dev=2.35-0ubuntu3.13         libcmocka-dev=1.1.5-2         libtool=2.4.6-15build2         libsodium-dev=1.0.18-1ubuntu0.22.04.1         make=4.3-4.1build1         pkg-config=0.29.2-1ubuntu3         valgrind=1:3.18.1-1ubuntu2     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./
# Ubuntu 22.04 ships gcovr 5.0, which predates the --txt-summary flag used by
# coverage-check (added in 5.1), so coverage-check is skipped here; the
# canonical CI workflow enforces the coverage gate on Ubuntu 24. The clang-14
# libasan also segfaults at startup against modern host-kernel ASLR layout, so
# test-sanitize is only run under gcc on this release. Jammy's valgrind also
# cannot reliably parse the clang-built test binary's debug info, so this image
# keeps valgrind coverage on the gcc pass only.
RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --enable-tests     && make build test test-sanitize test-valgrind test-qa     && cd /src     && mkdir -p build/autotools-clang     && cd build/autotools-clang     && ../../configure --enable-tests CC=clang     && make build test test-qa

FROM ubuntu:22.04@${UBUNTU_DIGEST} AS runtime

LABEL org.opencontainers.image.title="salt"       org.opencontainers.image.description="libsodium sealed-box CLI for GitHub Actions Secrets"       org.opencontainers.image.source="https://github.com/CTFfactory/salt"       org.opencontainers.image.licenses="Unlicense"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         libsodium23=1.0.18-1ubuntu0.22.04.1     && rm -rf /var/lib/apt/lists/*     && useradd --system --no-create-home --shell /usr/sbin/nologin --uid 65532 salt

WORKDIR /work
COPY --from=build /src/build/autotools/salt /usr/local/bin/salt

USER salt:salt

ENTRYPOINT ["salt"]
CMD ["--help"]

FROM ubuntu:22.04@${UBUNTU_DIGEST} AS deb

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.71-2         automake=1:1.16.5-1.3         dpkg=1.21.1ubuntu2.6         gcc=4:11.2.0-1ubuntu1         gzip=1.10-4ubuntu4.1         libc6-dev=2.35-0ubuntu3.13         libtool=2.4.6-15build2         libsodium-dev=1.0.18-1ubuntu0.22.04.1         make=4.3-4.1build1         pkg-config=0.29.2-1ubuntu3     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./

ARG SALT_PACKAGE_VERSION=0.0.0

RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --disable-tests     && make package-deb-local SALT_PACKAGE_VERSION="${SALT_PACKAGE_VERSION}" UBUNTU_VERSION=22     && dpkg -i packages/*.deb     && salt --version     && dpkg -r salt

FROM scratch AS deb-artifacts

COPY --from=deb /src/build/autotools/packages/*.deb /
