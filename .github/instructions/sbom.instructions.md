---
description: 'Rules for Syft installation, SPDX/CycloneDX SBOM generation, post-processing, validation, and release parity'
applyTo: '**/Makefile*,**/.github/workflows/*.yml,**/.github/workflows/*.yaml,**/scripts/*sbom*.sh,**/README.md,**/CHANGELOG.md'
---

# SBOM Instructions

These instructions apply when editing Syft installation, Syft CLI invocation, Syft configuration, SBOM generation, SBOM post-processing, release assets, or documentation that describes SBOM output.

## Syft Installation and Pinning

- Keep Syft pinned by exact `SYFT_VERSION` and `SYFT_SHA256`.
- Download Syft from the versioned Anchore release URL and verify the SHA256 checksum before extraction.
- Do not use unpinned Syft actions, package-manager floating versions, or `latest` URLs.
- Keep Makefile Syft variables and release workflow Syft environment values synchronized.

## Syft Invocation

- Scan release staging directories with `dir:<path>` sources.
- Emit both standard JSON formats for release SBOMs:
  - `-o spdx-json=<path>`
  - `-o cyclonedx-json=<path>`
- Pass source metadata explicitly:
  ```sh
  --source-name salt
  --source-supplier CTFfactory
  --source-version <release-version>
  ```
- Do not scan the full repository, build directory, or host filesystem for release SBOMs.
- Do not depend on host OS package databases for release archive dependency metadata.
- Enable all-file metadata for release staging scans with `SYFT_FILE_METADATA_SELECTION=all` or equivalent shared configuration so Syft, not post-processing, supplies shipped file entries, hashes, and executable metadata.

## Syft Configuration

- Prefer Syft CLI flags or explicit environment variables for values that must be obvious in Makefile and workflow logs.
- If adding a Syft config file, document why CLI flags are insufficient and ensure local and release automation load the same config.
- Use Syft file digest settings only when the generated SPDX output consumes them deterministically.
- Keep Syft cataloger changes minimal; explain any cataloger override in README or CHANGELOG if it affects user-visible SBOM content.
- Inspect `syft cataloger list`, `--select-catalogers`, and generated Syft JSON descriptor data before adding post-processing for package/component metadata.
- Use Syft templates for supplemental reports or targeted JSON summaries. Do not replace Syft's maintained SPDX/CycloneDX encoders with templates unless the template is validated against the target standard and avoids schema drift.

## Syft vs Post-Processing

- Use Syft for source identity, standard output formats, file discovery, file hashes, executable metadata, and raw cataloging.
- Use SBOM post-processing only for package/component identity, dependency relationships, or project metadata Syft cannot infer from the minimal release staging layout.
- Run Parlay enrichment only after Syft output and deterministic post-processing so third-party enrichment never replaces Syft file metadata or local dependency relationships.
- Route package/component/file SBOM metadata policy changes to `SBOM Expert`.
- Keep `scripts/add-libsodium-to-sbom.sh` deterministic and independent from network access.

## Required SBOM Metadata

- Keep project metadata consistent:
  - Project URL: `https://github.com/CTFfactory/salt`
  - License: `Unlicense`
  - Copyright: `Copyright 2026 CTFfactory`
- Keep Syft and Parlay pinned by exact version and SHA256 checksum in Makefile/workflow installation code.

## Artifact Scope

- Generate four SBOMs:
  - `salt-linux-amd64.spdx.json` for the dynamic release layout containing `salt` and `salt.1`
  - `salt-linux-amd64.cyclonedx.json` for the dynamic release layout containing `salt` and `salt.1`
  - `salt-static-linux-amd64.spdx.json` for the static release layout containing `salt-static` and `salt.1`
  - `salt-static-linux-amd64.cyclonedx.json` for the static release layout containing `salt-static` and `salt.1`
- Scan the release staging directories, not the whole repository or host filesystem.
- Include all four SBOMs in release checksums and release assets when release workflow assets change.

## SBOM Post-Processing

- Use `scripts/add-libsodium-to-sbom.sh` after Syft generation only for fields Syft cannot infer from the minimal staging directory.
- Do not overwrite Syft-generated file hashes or file entries when `SYFT_FILE_METADATA_SELECTION=all` provides them.
- Run Parlay after deterministic metadata augmentation and before validation.
- Use no-token Parlay enrichment (`ecosystems enrich` and `scorecard enrich`) by default; gate `snyk enrich` behind an explicit `PARLAY_SNYK_ENRICH=1` and `SNYK_TOKEN`.
- Ensure the `salt` package has:
  - `downloadLocation: https://github.com/CTFfactory/salt`
  - `licenseDeclared: Unlicense`
  - `licenseConcluded: Unlicense`
  - `copyrightText: Copyright 2026 CTFfactory`
  - GitHub purl external reference `pkg:github/CTFfactory/salt`
- Ensure SBOMs include a `libsodium` package with `licenseDeclared` and `licenseConcluded` set to `ISC`.
- Ensure relationships include `salt DEPENDS_ON libsodium`.
- For shipped file entries (`salt`, `salt-static`, `salt.1`), populate:
  - real SHA1 and SHA256 checksums from Syft file metadata
  - `licenseConcluded: Unlicense`
  - `licenseInfoInFiles: ["Unlicense"]`
  - `copyrightText: Copyright 2026 CTFfactory`
- `salt` and `salt-static` use `fileTypes: ["BINARY"]`; `salt.1` uses `fileTypes: ["DOCUMENTATION"]`.
- Ensure CycloneDX JSON includes:
  - metadata component `salt` with supplier `CTFfactory`, release-aligned version metadata, purl `pkg:github/CTFfactory/salt`, Unlicense, project URL, and copyright
  - file components for `salt`/`salt-static` and `salt.1` with Syft-generated SHA-1/SHA-256 hashes
  - `libsodium` library component with ISC license and version
  - `salt` dependency entry that depends on `libsodium`

## Validation

Run these checks after SBOM or Syft changes:

```sh
bash -n scripts/add-libsodium-to-sbom.sh
bash -n scripts/validate-sbom.sh
make install-parlay
make install-syft
make sbom
make actionlint
make docs
make codespell
```

When Syft output shape changes, inspect the generated JSON before editing post-processing:

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
    assert data.get("spdxVersion") == "SPDX-2.3" or data.get("bomFormat") == "CycloneDX"
PY
```

## Documentation

- Update `README.md` when SBOM output files, metadata, or release asset behavior changes.
- Update `CHANGELOG.md` for user-visible SBOM generation, release asset, checksum, or metadata changes.
- Keep documentation phrased around the released artifacts, not internal implementation history.
