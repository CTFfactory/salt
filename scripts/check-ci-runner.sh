#!/usr/bin/env bash
set -euo pipefail

expected_ubuntu_version="${SALT_CI_UBUNTU_VERSION:-24.04}"

if [[ ! -r /etc/os-release ]]; then
  echo "ERROR: /etc/os-release not found; cannot validate runner OS." >&2
  exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "${expected_ubuntu_version}" ]]; then
  echo "ERROR: runner must be Ubuntu ${expected_ubuntu_version}; found ${PRETTY_NAME:-unknown}." >&2
  exit 1
fi

declare -a required_cmds=(
  autoconf
  automake
  autoreconf
  libtoolize
  pkg-config
  make
  gcc
  clang-18
  clang-format-18
  clang-tidy-18
  scan-build-18
  cppcheck
  shellcheck
  codespell
  valgrind
  gcovr
  man
  jq
  curl
  git
  python3
  tar
  gzip
  sha256sum
  awk
  sed
  grep
  realpath
  strip
  nm
  ldd
)

for gcc_version in 9 10 11 12 13; do
  required_cmds+=("gcc-${gcc_version}")
done

for clang_version in 14 15 16 17 18; do
  required_cmds+=("clang-${clang_version}")
done

declare -a missing_cmds=()
for cmd in "${required_cmds[@]}"; do
  if ! command -v "$cmd" >/dev/null 2>&1; then
    missing_cmds+=("$cmd")
  fi
done

declare -a missing_pkg_config=()
for module in libsodium cmocka; do
  if ! pkg-config --exists "$module"; then
    missing_pkg_config+=("$module")
  fi
done

if ((${#missing_cmds[@]} > 0)) || ((${#missing_pkg_config[@]} > 0)); then
  {
    echo "ERROR: runner prerequisite validation failed."
    if ((${#missing_cmds[@]} > 0)); then
      echo "Missing commands:"
      printf '  - %s\n' "${missing_cmds[@]}"
    fi
    if ((${#missing_pkg_config[@]} > 0)); then
      echo "Missing pkg-config modules:"
      printf '  - %s\n' "${missing_pkg_config[@]}"
    fi
    echo "Provision required packages from scripts/install-ci-runner.sh or scripts/ci-runner-ubuntu22-ansible.yml."
  } >&2
  exit 1
fi

echo "Runner prerequisites verified for Ubuntu ${expected_ubuntu_version}."
