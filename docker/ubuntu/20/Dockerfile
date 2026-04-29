# syntax=docker/dockerfile:1
#
# Base image is pinned to an immutable digest per
# .github/instructions/dependency-pinning.instructions.md. To refresh, query
# the Docker Hub registry API for the current ubuntu:20.04 manifest digest
# and update UBUNTU_DIGEST below.
#
# Ubuntu 20.04 (Focal) is EOL but archive.ubuntu.com still serves the final
# 20.04.6 point release. libsodium 1.0.18 ships in focal main with soname
# libsodium.so.23.
ARG UBUNTU_DIGEST=sha256:c664f8f86ed5a386b0a340d981b8f81714e21a8b9c73f658c4bea56aa179d54a

FROM ubuntu:20.04@${UBUNTU_DIGEST} AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.69-11.1         automake=1:1.16.1-4ubuntu6         clang=1:10.0-50~exp1         gcc=4:9.3.0-1ubuntu2         libc6-dev=2.31-0ubuntu9.18         libcmocka-dev=1.1.5-2         libtool=2.4.6-14         libsodium-dev=1.0.18-1         make=4.2.1-1.2         pkg-config=0.29.1-0ubuntu4         valgrind=1:3.15.0-1ubuntu9.1     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./
# Ubuntu 20.04's GCC 9 libasan/libubsan loops on DEADLYSIGNAL when run against a
# modern host-kernel ASLR layout (no in-container workaround), so test-sanitize
# is excluded here. gcovr 4.2 also predates --txt-summary, so coverage-check is
# skipped; the canonical CI workflow enforces those gates on Ubuntu 24. Focal's
# valgrind also cannot reliably parse the clang-built test binary's debug info,
# so this image keeps valgrind coverage on the gcc pass only.
RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --enable-tests     && make build test test-valgrind test-qa     && cd /src     && mkdir -p build/autotools-clang     && cd build/autotools-clang     && ../../configure --enable-tests CC=clang     && make build test test-qa

FROM ubuntu:20.04@${UBUNTU_DIGEST} AS runtime

LABEL org.opencontainers.image.title="salt"       org.opencontainers.image.description="libsodium sealed-box CLI for GitHub Actions Secrets"       org.opencontainers.image.source="https://github.com/CTFfactory/salt"       org.opencontainers.image.licenses="Unlicense"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         libsodium23=1.0.18-1     && rm -rf /var/lib/apt/lists/*     && useradd --system --no-create-home --shell /usr/sbin/nologin --uid 65532 salt

WORKDIR /work
COPY --from=build /src/build/autotools/salt /usr/local/bin/salt

USER salt:salt

ENTRYPOINT ["salt"]
CMD ["--help"]
