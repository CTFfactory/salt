#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'EOF'
Usage: build-deb-package.sh --binary PATH --manpage PATH --version X.Y.Z --release NubuntuXX --output PATH
EOF
  exit 2
}

binary_path=''
manpage_path=''
package_version=''
package_release=''
output_path=''

while (($# > 0)); do
  case "$1" in
    --binary)
      [[ $# -ge 2 ]] || usage
      binary_path="$2"
      shift 2
      ;;
    --manpage)
      [[ $# -ge 2 ]] || usage
      manpage_path="$2"
      shift 2
      ;;
    --version)
      [[ $# -ge 2 ]] || usage
      package_version="$2"
      shift 2
      ;;
    --release)
      [[ $# -ge 2 ]] || usage
      package_release="$2"
      shift 2
      ;;
    --output)
      [[ $# -ge 2 ]] || usage
      output_path="$2"
      shift 2
      ;;
    --help|-h)
      usage
      ;;
    *)
      usage
      ;;
  esac
done

[[ -n "$binary_path" && -n "$manpage_path" && -n "$package_version" && -n "$package_release" && -n "$output_path" ]] || usage
[[ -f "$binary_path" ]] || { echo "ERROR: binary not found: $binary_path" >&2; exit 1; }
[[ -f "$manpage_path" ]] || { echo "ERROR: man page not found: $manpage_path" >&2; exit 1; }
command -v dpkg-deb >/dev/null 2>&1 || { echo 'ERROR: dpkg-deb is required' >&2; exit 1; }
command -v gzip >/dev/null 2>&1 || { echo 'ERROR: gzip is required' >&2; exit 1; }

tmp_dir="$(mktemp -d)"
cleanup() {
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

package_root="$tmp_dir/salt"
mkdir -p "$package_root/DEBIAN" "$package_root/usr/bin" "$package_root/usr/share/man/man1"

install -m 0755 "$binary_path" "$package_root/usr/bin/salt"
gzip -cn "$manpage_path" > "$package_root/usr/share/man/man1/salt.1.gz"

installed_size_kib="$(du -sk "$package_root" | awk '{print $1}')"
cat > "$package_root/DEBIAN/control" <<EOF
Package: salt
Version: ${package_version}-${package_release}
Section: utils
Priority: optional
Architecture: amd64
Maintainer: CTFfactory <noreply@ctffactory.com>
Depends: libsodium23
Installed-Size: ${installed_size_kib}
Homepage: https://github.com/CTFfactory/salt
Description: libsodium sealed-box CLI for GitHub Actions Secrets workflows
 salt encrypts plaintext as a libsodium sealed box suitable for GitHub Actions
 Secrets API workflows.
EOF

mkdir -p "$(dirname "$output_path")"
dpkg-deb --build --root-owner-group "$package_root" "$output_path" >/dev/null
echo "Wrote: $output_path"
