---
name: audit-syft
description: 'Audit Syft installation, configuration, catalogers, file selection, CLI options, and raw generated output before SBOM post-processing'
argument-hint: '[Makefile, release workflow, or generated SBOM path]'
---

# Audit Syft Skill

Use this skill to review Syft-specific behavior before making SBOM metadata or release automation changes.

## Workflow

1. **Inspect Syft installation.**
   - Confirm `SYFT_VERSION` is exact.
   - Confirm `SYFT_SHA256` is present and used with `sha256sum -c`.
   - Confirm Syft is installed from a versioned Anchore release URL.

2. **Inspect Syft invocation.**
   - Confirm local and release paths use `dir:<release-staging-dir>`.
   - Confirm output is SPDX JSON and CycloneDX JSON.
    - Confirm source metadata is:
      - `--source-name salt`
      - `--source-supplier CTFfactory`
      - `--source-version <release-version>`

   - Confirm all-file metadata selection is enabled for release staging scans.
   - Confirm cataloger changes were inspected before adding post-processing. Note: Syft catalogers detect packages from package-manager databases and ecosystem manifests; the minimal release staging directories deliberately omit host package metadata to keep SBOMs artifact-scoped, so limited cataloger discovery is expected and post-processing scripts supply explicit `libsodium` metadata.

3. **Generate fresh output.**
   ```sh
   make sbom
   ```

4. **Review raw Syft behavior.**
   - Identify which package/component/file fields Syft emitted directly.
    - Identify which fields were populated by `scripts/add-libsodium-to-sbom.sh`.
   - Verify shipped file entries and hashes come from Syft all-file metadata.
   - Do not treat missing host package metadata as a Syft bug when scanning minimal release staging directories.

5. **Check parity.**
   - Compare Makefile Syft command lines to release workflow command lines.
   - Ensure both paths run the same post-processing script after Syft generation.
   - Ensure both paths validate generated SPDX and CycloneDX JSON.

## Audit Snippet

```sh
python3 - <<'PY'
import json

for path in (
    "build/sbom/salt-linux-amd64.spdx.json",
    "build/sbom/salt-static-linux-amd64.spdx.json",
    "build/sbom/salt-linux-amd64.cyclonedx.json",
    "build/sbom/salt-static-linux-amd64.cyclonedx.json",
):
    with open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    print(path)
    print("  format:", data.get("spdxVersion") or data.get("bomFormat"))
    print("  namespace:", data.get("documentNamespace") or data.get("serialNumber"))
    print("  packages:", [package.get("name") for package in data.get("packages", [])])
    print("  components:", [component.get("name") for component in data.get("components", [])])
    print("  files:", [file.get("fileName") for file in data.get("files", [])])
PY
```

## Report Format

Report Syft findings in this order:

1. **Pinning** — version, checksum, download source.
2. **Invocation** — source type, output format, source metadata.
3. **Configuration** — config file, file selection, cataloger, or environment variable effects.
4. **Raw output** — what Syft emits before post-processing, including file hashes.
5. **Parity** — Makefile and release workflow consistency.
