---
name: audit-sbom
description: 'Audit generated SPDX/CycloneDX JSON SBOMs for salt release metadata, checksums, relationships, and release parity'
argument-hint: '[path to SBOM or blank for build/sbom outputs]'
---

# Audit SBOM Skill

Use this skill to review generated SBOMs and confirm the release workflow and local `make sbom` target produce complete, artifact-scoped SPDX JSON and CycloneDX JSON, and that outputs remain usable for vulnerability monitoring.

## Workflow

1. **Generate fresh SBOMs unless the user supplied specific files.**
   ```sh
   make sbom
   ```

2. **Inspect all expected SBOMs.**
   - `build/sbom/salt-linux-amd64.spdx.json`
   - `build/sbom/salt-linux-amd64.cyclonedx.json`
   - `build/sbom/salt-static-linux-amd64.spdx.json`
   - `build/sbom/salt-static-linux-amd64.cyclonedx.json`

3. **Validate document shape.**
   - `spdxVersion` is `SPDX-2.3`.
   - CycloneDX `bomFormat` is `CycloneDX` and `specVersion` is `1.6`.
   - `documentNamespace` is non-empty.
   - `creationInfo` exists.
   - The document describes the release staging layout, not the whole repository.

4. **Validate `salt` package/component metadata.**
   - `name: salt`
   - `supplier: Organization: CTFfactory`
    - `versionInfo` equals the release version being validated (often `0.0.0` locally)
   - `downloadLocation: https://github.com/CTFfactory/salt`
   - `licenseDeclared: Unlicense`
    - `licenseConcluded: Unlicense`
    - `copyrightText: Copyright 2026 CTFfactory`
    - external purl reference: `pkg:github/CTFfactory/salt`
    - minimum metadata quality is present: component name, version, supplier, hash, and license fields are non-empty where policy requires.

5. **Validate shipped file metadata.**
   - Dynamic SBOM has a `files[]` entry for `salt`.
   - Static SBOM has a `files[]` entry for `salt-static`.
   - Each SBOM has a shipped-file entry/component for `salt.1`.
   - Each shipped file has non-zero SHA1/SHA256 or SHA-1/SHA-256 hashes from Syft all-file metadata.
   - SPDX binary entries have `fileTypes: ["BINARY"]`; SPDX `salt.1` has `fileTypes: ["DOCUMENTATION"]`.
   - File entries/components have Unlicense and `Copyright 2026 CTFfactory` metadata.

6. **Validate libsodium dependency metadata.**
   - A package named `libsodium` exists.
   - The package has `licenseDeclared: ISC` and `licenseConcluded: ISC`.
   - A `DEPENDS_ON` relationship links the `salt` package SPDX ID to `SPDXRef-Package-libsodium`.
   - CycloneDX has a `libsodium` library component with ISC license.
   - CycloneDX dependencies link the `salt` metadata component to `libsodium`.

7. **Validate release parity.**
    - `.github/workflows/release.yml` invokes Syft with the same source metadata as `make sbom`.
    - Release workflow invokes `scripts/add-libsodium-to-sbom.sh` and `scripts/add-ubuntu-provenance-to-sbom.sh`.
    - Release workflow includes all four SBOMs in `SHA256SUMS` and release assets.
    - Release workflow runs `scripts/validate-sbom.sh`.

8. **Validate vulnerability-monitoring readiness.**
    - Confirm SBOM outputs are consumable by the project vulnerability-monitoring workflow (for example `make sbom-vuln`, Snyk, or Trivy usage documented for release triage).
    - If capability is missing or drifting, report it as an operational gap even when schema/policy checks pass.

## Audit Snippet

```sh
python3 - <<'PY'
import json
import sys

expected = {
    "build/sbom/salt-linux-amd64.spdx.json": "salt",
    "build/sbom/salt-static-linux-amd64.spdx.json": "salt-static",
}
expected_version = "0.0.0"

for path, file_name in expected.items():
    with open(path, encoding="utf-8") as handle:
        data = json.load(handle)
    assert data.get("spdxVersion") == "SPDX-2.3", path
    packages = data.get("packages", [])
    salt = next(package for package in packages if package.get("name") == "salt")
    assert salt.get("supplier") == "Organization: CTFfactory", path
    assert salt.get("versionInfo") == expected_version, path
    assert salt.get("downloadLocation") == "https://github.com/CTFfactory/salt", path
    assert salt.get("licenseDeclared") == "Unlicense", path
    assert salt.get("copyrightText") == "Copyright 2026 CTFfactory", path
    assert any(package.get("name") == "libsodium" for package in packages), path
    file_entry = next(file for file in data.get("files", []) if file.get("fileName") == file_name)
    checksums = {item["algorithm"]: item["checksumValue"] for item in file_entry["checksums"]}
    assert len(checksums.get("SHA1", "")) == 40 and set(checksums["SHA1"]) != {"0"}, path
    assert len(checksums.get("SHA256", "")) == 64, path
    assert file_entry.get("licenseConcluded") == "Unlicense", path
    assert file_entry.get("licenseInfoInFiles") == ["Unlicense"], path
    assert file_entry.get("copyrightText") == "Copyright 2026 CTFfactory", path

print("OK: SBOM metadata is complete")
PY
```

Prefer the repository validator for full SPDX/CycloneDX coverage:

```sh
./scripts/validate-sbom.sh \
  --salt-version 0.0.0 \
  --spdx build/sbom/salt-linux-amd64.spdx.json:salt \
  --spdx build/sbom/salt-static-linux-amd64.spdx.json:salt-static \
  --cyclonedx build/sbom/salt-linux-amd64.cyclonedx.json:salt \
  --cyclonedx build/sbom/salt-static-linux-amd64.cyclonedx.json:salt-static
```

## Report Format

Summarize findings in this order:

1. **Document validity** — SPDX/CycloneDX version and namespace/serial.
2. **Package/component metadata** — `salt`, purl, license, copyright, supplier, version.
3. **File metadata** — shipped-file checksums/hashes, license, copyright, file type.
4. **Dependency metadata** — libsodium package and relationship.
5. **Release parity** — Makefile, workflow, checksums, release assets.
6. **Vulnerability-monitoring readiness** — SBOM outputs remain actionable for CVE triage tooling.
