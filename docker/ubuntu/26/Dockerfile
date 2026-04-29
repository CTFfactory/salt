# syntax=docker/dockerfile:1
#
# Base image is pinned to an immutable digest per
# .github/instructions/dependency-pinning.instructions.md. To refresh, query
# the Docker Hub registry API for the current ubuntu:26.04 manifest digest
# and update UBUNTU_DIGEST below.
ARG UBUNTU_DIGEST=sha256:0a8aa7e675b331b09dfe80f7a44a02642035aefca58b0034ad17ca2db8f952e6

FROM ubuntu:26.04@${UBUNTU_DIGEST} AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.72-3.1ubuntu2         automake=1:1.18.1-3build1         clang=1:21.1.6-71         gcc=4:15.2.0-5ubuntu1         gcovr=7.2+really-2         libc6-dev=2.43-2ubuntu2         libcmocka-dev=2.0.2-1         libtool=2.5.4-9         libsodium-dev=1.0.18-2         make=4.4.1-3         pkg-config=2.5.1-4         valgrind=1:3.26.0-0ubuntu1     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./
RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --enable-tests     && make build test test-sanitize test-valgrind coverage-check test-qa     && cd /src     && mkdir -p build/autotools-clang     && cd build/autotools-clang     && ../../configure --enable-tests CC=clang     && make build test test-sanitize test-valgrind test-qa

FROM ubuntu:26.04@${UBUNTU_DIGEST} AS runtime

LABEL org.opencontainers.image.title="salt"       org.opencontainers.image.description="libsodium sealed-box CLI for GitHub Actions Secrets"       org.opencontainers.image.source="https://github.com/CTFfactory/salt"       org.opencontainers.image.licenses="Unlicense"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         libsodium23=1.0.18-2     && rm -rf /var/lib/apt/lists/*     && useradd --system --no-create-home --shell /usr/sbin/nologin --uid 65532 salt

WORKDIR /work
COPY --from=build /src/build/autotools/salt /usr/local/bin/salt

USER salt:salt

ENTRYPOINT ["salt"]
CMD ["--help"]

FROM ubuntu:26.04@${UBUNTU_DIGEST} AS deb

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.72-3.1ubuntu2         automake=1:1.18.1-3build1         ca-certificates=20260223         curl=8.18.0-1ubuntu2         dpkg=1.23.7ubuntu1         gcc=4:15.2.0-5ubuntu1         gzip=1.14-1~exp2ubuntu1         libc6-dev=2.43-2ubuntu2         libtool=2.5.4-9         libsodium-dev=1.0.18-2         make=4.4.1-3         pkg-config=2.5.1-4     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./

ARG SALT_PACKAGE_VERSION=0.0.0

RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --disable-tests     && make package-deb-local SALT_PACKAGE_VERSION="${SALT_PACKAGE_VERSION}" UBUNTU_VERSION=26     && dpkg -i packages/*.deb     && salt --version     && dpkg -r salt

FROM scratch AS deb-artifacts

COPY --from=deb /src/build/autotools/packages/*.deb /
