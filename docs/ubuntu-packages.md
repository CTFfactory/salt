# Ubuntu Packages for `salt` Development

Curated list of Ubuntu (24.04+) packages useful when developing, testing,
fuzzing, benchmarking, and packaging the `salt` CLI. Versions are intentionally
unpinned here; CI and release/package container definitions pin exact versions in
[.github/workflows/ci.yml](../.github/workflows/ci.yml),
[.github/workflows/release.yml](../.github/workflows/release.yml), and
[`docker/ubuntu/*/Dockerfile`](../docker/ubuntu/24/Dockerfile).

## Required (build + test)

```bash
sudo apt-get install -y \
    build-essential \
    libsodium-dev \
    libcmocka-dev \
    make \
    pkg-config
```

## Strongly recommended (matches `make ci` gates)

```bash
sudo apt-get install -y \
    clang clang-18 clang-tidy-18 clang-format-18 \
    cppcheck \
    shellcheck \
    codespell \
    valgrind \
    gcovr \
    man-db
```

The [Makefile](../Makefile) references `clang-18`, `clang-tidy-18`, and
`clang-format-18` by exact name; the unversioned `clang` package alone is not
sufficient for `make lint`.

## GCC version matrix

Mirrors the matrix in [.github/workflows/ci.yml](../.github/workflows/ci.yml):

```bash
sudo apt-get install -y gcc-9 gcc-10 gcc-11 gcc-12 gcc-13
```

## Developer ergonomics

```bash
sudo apt-get install -y \
    bear \
    ccache \
    clangd-18 \
    gdb \
    lldb-18 \
    strace ltrace \
    binutils \
    file \
    bsdmainutils \
    xxd \
    jq \
    git \
    curl ca-certificates \
    coreutils findutils
```

- `bear` — generates `compile_commands.json` for `clangd` / `clang-tidy`.
- `ccache` — speeds up repeated builds across CI matrix runs locally.
- `clangd-18` — LSP server for editor integration.
- `xxd` / `hexdump` — inspect ciphertext bytes during fuzz triage.
- `jq` — read/edit [bench/baseline.json](../bench/baseline.json) and GitHub API JSON.

## Fuzzing

libFuzzer is bundled with clang; install the symbolizer for readable traces.

```bash
sudo apt-get install -y \
    clang-18 \
    llvm-18 \
    afl++
```

Set `LLVM_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-18` for symbolicated
ASan/UBSan/libFuzzer stack traces.

## Docs / man pages / release artifacts

```bash
sudo apt-get install -y \
    man-db groff \
    pandoc \
    asciidoctor
```

`man-db` and `groff` render [man/salt.1](../man/salt.1). `pandoc` is useful
for converting Markdown release notes.

## Containers (matches [docker/ubuntu](../docker/ubuntu/24/Dockerfile))

```bash
sudo apt-get install -y \
    docker.io \
    docker-buildx \
    docker-compose-v2
```

## CI / workflow hygiene

```bash
sudo apt-get install -y \
    shellcheck \
    yamllint \
    python3-yaml
```

`actionlint` and `vhs` are not in Ubuntu repos:

- `actionlint` is downloaded and pinned by the Makefile under
  `build/tools/actionlint`.
- `vhs` should be installed from the Charmbracelet release tarball.

## Optional but handy

```bash
sudo apt-get install -y \
    entr \
    moreutils \
    parallel \
    time \
    hyperfine
```

- `entr` — re-run `make test` on file changes.
- `moreutils` — `sponge`, `ts`, `chronic` for scripting.
- `time` — GNU `/usr/bin/time -v` for memory stats.
- `hyperfine` — sanity-check microbenchmarks against
  [bench/baseline.json](../bench/baseline.json).

## Notes

- `libsodium23` is the runtime shared library; it is pulled in transitively by
  `libsodium-dev`. The runtime stage of the Dockerfile installs it standalone.
- For local development, unpinned `apt-get install` is fine. CI uses pinned
  `=VERSION` strings to guarantee reproducibility.
