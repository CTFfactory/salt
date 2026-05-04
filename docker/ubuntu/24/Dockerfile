# syntax=docker/dockerfile:1
#
# Base image is pinned to an immutable digest per
# .github/instructions/dependency-pinning.instructions.md. To refresh, query
# the Docker Hub registry API for the current ubuntu:24.04 manifest digest
# and update UBUNTU_DIGEST below.
ARG UBUNTU_DIGEST=sha256:cdb5fd928fced577cfecf12c8966e830fcdf42ee481fb0b91904eeddc2fe5eff

FROM ubuntu:24.04@${UBUNTU_DIGEST} AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.71-3         automake=1:1.16.5-1.3ubuntu1         clang=1:18.0-59~exp2         gcc=4:13.2.0-7ubuntu1         gcovr=7.0-1         libc6-dev=2.39-0ubuntu8.7         libcmocka-dev=1.1.7-3         libtool=2.4.7-7build1         libsodium-dev=1.0.18-1ubuntu0.24.04.1         make=4.3-4.1build2         pkg-config=1.8.1-2build1         valgrind=1:3.22.0-0ubuntu3     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./
RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --enable-tests     && make build test test-sanitize test-valgrind coverage-check test-qa     && cd /src     && mkdir -p build/autotools-clang     && cd build/autotools-clang     && ../../configure --enable-tests CC=clang     && make build test test-sanitize test-valgrind test-qa

FROM ubuntu:24.04@${UBUNTU_DIGEST} AS runtime

LABEL org.opencontainers.image.title="salt"       org.opencontainers.image.description="libsodium sealed-box CLI for GitHub Actions Secrets"       org.opencontainers.image.source="https://github.com/CTFfactory/salt"       org.opencontainers.image.licenses="Unlicense"

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         libsodium23=1.0.18-1ubuntu0.24.04.1     && rm -rf /var/lib/apt/lists/*     && useradd --system --no-create-home --shell /usr/sbin/nologin --uid 65532 salt

WORKDIR /work
COPY --from=build /src/build/autotools/salt /usr/local/bin/salt

USER salt:salt

ENTRYPOINT ["salt"]
CMD ["--help"]

FROM ubuntu:24.04@${UBUNTU_DIGEST} AS deb

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends         autoconf=2.71-3         automake=1:1.16.5-1.3ubuntu1         dpkg=1.22.6ubuntu6.5         gcc=4:13.2.0-7ubuntu1         gzip=1.12-1ubuntu3.1         libc6-dev=2.39-0ubuntu8.7         libtool=2.4.7-7build1         libsodium-dev=1.0.18-1ubuntu0.24.04.1         make=4.3-4.1build2         pkg-config=1.8.1-2build1     && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . ./

ARG SALT_PACKAGE_VERSION=0.0.0

RUN ./bootstrap     && mkdir -p build/autotools     && cd build/autotools     && ../../configure --disable-tests     && make package-deb-local SALT_PACKAGE_VERSION="${SALT_PACKAGE_VERSION}" UBUNTU_VERSION=24     && dpkg -i packages/*.deb     && salt --version     && dpkg -r salt

FROM scratch AS deb-artifacts

COPY --from=deb /src/build/autotools/packages/*.deb /
