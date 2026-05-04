#!/usr/bin/env bash
set -euo pipefail

expected_ubuntu_version="${SALT_CI_UBUNTU_VERSION:-24.04}"
expected_ubuntu_codename="${SALT_CI_UBUNTU_CODENAME:-noble}"
allow_dynamic_apt_repos="${SALT_CI_ALLOW_DYNAMIC_APT_REPOS:-0}"
apt_pin_mode="${SALT_CI_APT_PIN_MODE:-major}"

if [[ ! -r /etc/os-release ]]; then
  echo "ERROR: /etc/os-release not found; cannot install runner prerequisites." >&2
  exit 1
fi

# shellcheck disable=SC1091
source /etc/os-release

if [[ "${ID:-}" != "ubuntu" || "${VERSION_ID:-}" != "${expected_ubuntu_version}" ]]; then
  echo "ERROR: runner must be Ubuntu ${expected_ubuntu_version}; found ${PRETTY_NAME:-unknown}." >&2
  exit 1
fi

if ! command -v add-apt-repository >/dev/null 2>&1; then
  sudo apt-get update
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends software-properties-common
fi

sudo apt-get update
if ! apt-cache show gcc-13 >/dev/null 2>&1; then
  if [[ "${allow_dynamic_apt_repos}" != "1" ]]; then
    echo "ERROR: gcc-13 is unavailable in configured APT sources and dynamic repo setup is disabled." >&2
    echo "Set SALT_CI_ALLOW_DYNAMIC_APT_REPOS=1 only on trusted ephemeral runners if fallback repos are required." >&2
    exit 1
  fi
  sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test
  sudo apt-get update
fi

declare -a clang_versions=(14 15 16 17 18)
install_llvm_repo=0
for clang_version in "${clang_versions[@]}"; do
  if ! apt-cache show "clang-${clang_version}" >/dev/null 2>&1; then
    install_llvm_repo=1
    break
  fi
done

if ((install_llvm_repo)); then
  if [[ "${allow_dynamic_apt_repos}" != "1" ]]; then
    echo "ERROR: one or more required clang packages are unavailable in configured APT sources and dynamic repo setup is disabled." >&2
    echo "Set SALT_CI_ALLOW_DYNAMIC_APT_REPOS=1 only on trusted ephemeral runners if fallback repos are required." >&2
    exit 1
  fi

  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gnupg

  llvm_keyring="/usr/share/keyrings/llvm.gpg"
  llvm_list="/etc/apt/sources.list.d/llvm.list"
  apt_architecture="$(dpkg --print-architecture)"
  ubuntu_codename="${VERSION_CODENAME:-${UBUNTU_CODENAME:-}}"
  if [[ "${ubuntu_codename}" != "${expected_ubuntu_codename}" ]]; then
    echo "ERROR: expected Ubuntu codename ${expected_ubuntu_codename}, found ${ubuntu_codename:-unknown}." >&2
    exit 1
  fi

  if [[ ! -f "${llvm_keyring}" ]]; then
    curl -fsSL "https://apt.llvm.org/llvm-snapshot.gpg.key" \
      | gpg --dearmor \
      | sudo tee "${llvm_keyring}" >/dev/null
  fi

  missing_llvm_repo_entries=0
  if [[ ! -f "${llvm_list}" ]]; then
    missing_llvm_repo_entries=1
  else
    for clang_version in "${clang_versions[@]}"; do
      if ! grep -Fq "llvm-toolchain-noble-${clang_version} main" "${llvm_list}"; then
        missing_llvm_repo_entries=1
        break
      fi
    done
  fi

  if ((missing_llvm_repo_entries)); then
    {
      for clang_version in "${clang_versions[@]}"; do
        printf 'deb [arch=%s signed-by=%s] https://apt.llvm.org/%s/ llvm-toolchain-%s-%s main\n' "${apt_architecture}" "${llvm_keyring}" "${expected_ubuntu_codename}" "${expected_ubuntu_codename}" "${clang_version}"
      done
    } | sudo tee "${llvm_list}" >/dev/null
  fi

  sudo apt-get update
fi

declare -a pinned_compiler_packages=(
  'gcc-9=9.*'
  'gcc-10=10.*'
  'gcc-11=11.*'
  'gcc-12=12.*'
  'gcc-13=13.*'
  'clang-14=1:14.*'
  'clang-15=1:15.*'
  'clang-16=1:16.*'
  'clang-17=1:17.*'
  'clang-18=1:18.*'
  'clang-format-18=1:18.*'
  'clang-tidy-18=1:18.*'
  'clang-tools-18=1:18.*'
)

declare -a compiler_packages=(
  gcc-9
  gcc-10
  gcc-11
  gcc-12
  gcc-13
  clang-14
  clang-15
  clang-16
  clang-17
  clang-18
  clang-format-18
  clang-tidy-18
  clang-tools-18
)

declare -a base_packages=(
  autoconf
  automake
  libtool
  pkg-config
  build-essential
  gcc
  libsodium-dev
  libcmocka-dev
  cppcheck
  shellcheck
  codespell
  valgrind
  gcovr
  man-db
  binutils
  curl
  git
  jq
  python3
  python3-yaml
  libc6-dev
  libc-bin
  debsigs
)

declare -a resolved_compiler_packages=()
if [[ "${apt_pin_mode}" == "major" ]]; then
  resolved_compiler_packages=("${pinned_compiler_packages[@]}")
elif [[ "${apt_pin_mode}" == "none" ]]; then
  resolved_compiler_packages=("${compiler_packages[@]}")
else
  echo "ERROR: unsupported SALT_CI_APT_PIN_MODE='${apt_pin_mode}'. Supported values: major, none." >&2
  exit 1
fi

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  "${base_packages[@]}" \
  "${resolved_compiler_packages[@]}"
