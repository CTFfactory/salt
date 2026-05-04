---
name: generate-sbom
description: 'Generate or update Syft SPDX/CycloneDX JSON SBOM automation for salt release artifacts'
argument-hint: '[local, release, or both]'
---

# Generate SBOM Skill

Use this skill when adding, updating, or troubleshooting SBOM generation in the Makefile, release workflow, or SBOM post-processing script.

## C Tool Best-Practice Requirements

Apply these requirements when in scope for this repository:

- Automate SBOM generation inside Make/CI paths (no manual-only workflow).
- Preserve transitive dependency visibility, including static/dynamic linkage evidence where applicable.
- Emit standardized machine-readable formats (SPDX and CycloneDX).
- Maintain minimum metadata elements per component (name, version, supplier, hash, license).
- Pair generation with ongoing vulnerability-monitoring capability.
- Prefer artifact/build-aware analysis over repository-only scanning.

## Workflow

1. **Read current SBOM surfaces.**
   - Read `Makefile` SBOM variables, `install-syft`, and `sbom`.
   - Read `.github/workflows/release.yml` Syft install, SBOM generation, checksum, and release asset steps.
   - Read `scripts/add-libsodium-to-sbom.sh`, `scripts/collect-ubuntu-deps.sh`, `scripts/add-ubuntu-provenance-to-sbom.sh`, and `scripts/validate-sbom.sh`.
   - Read README and CHANGELOG SBOM entries.

2. **Keep Syft and Parlay pinned and deterministic.**
   - Use exact `SYFT_VERSION`.
   - Verify downloaded Syft archives with `SYFT_SHA256`.
   - Avoid unpinned Syft GitHub Actions or `latest` downloads.
   - Use exact `PARLAY_VERSION`.
   - Verify downloaded Parlay archives with `PARLAY_SHA256`.
   - Run Parlay only after Syft generation and deterministic metadata augmentation.

3. **Generate artifact-scoped standard JSON SBOMs.**
   - Scan release staging directories with `syft dir:<staging-dir>`.
   - Enable all-file metadata with `SYFT_FILE_METADATA_SELECTION=all` or equivalent shared config.
   - Output SPDX JSON with `-o spdx-json=<output>`.
   - Output CycloneDX JSON with `-o cyclonedx-json=<output>`.
   - Pass:
     ```sh
      --source-name salt
      --source-supplier CTFfactory
      --source-version <release-version>
      ```

4. **Post-process only metadata Syft cannot infer.**
    - Prefer Syft file selection, cataloger selection, CLI flags, config, and official encoders before adding custom logic.
    - Run `scripts/add-libsodium-to-sbom.sh` for generated SPDX/CycloneDX SBOMs.
    - Run `scripts/collect-ubuntu-deps.sh` and `scripts/add-ubuntu-provenance-to-sbom.sh` for deterministic Ubuntu dependency provenance and usage/license evidence metadata.
    - Pass project URL, license, copyright, libsodium version, and staged artifact paths:
      ```sh
       ./scripts/add-libsodium-to-sbom.sh \
        --project-url "https://github.com/CTFfactory/salt" \
        --license "Unlicense" \
        --copyright "Copyright 2026 CTFfactory" \
        --version "$LIBSODIUM_VERSION" \
        --salt-version "$SBOM_VERSION" \
        --artifact-file "salt=<staging>/salt" \
        --artifact-file "salt-static=<staging>/salt-static" \
        --artifact-file "salt.1=<staging>/salt.1" \
        <output>.spdx.json <output>.cyclonedx.json
      ```
   - Do not overwrite Syft-generated hashes or file entries.

5. **Enrich with Parlay after deterministic metadata.**
   - Run Parlay on every generated SPDX/CycloneDX SBOM before validation:
     ```sh
     make sbom-parlay
     ```
   - Use no-token enrichment by default:
     - `parlay ecosystems enrich <sbom>`
     - `parlay scorecard enrich <sbom>`
   - Gate Snyk enrichment behind explicit opt-in:
     ```sh
     PARLAY_SNYK_ENRICH=1 SNYK_TOKEN=... make sbom
     ```
   - Do not let Parlay replace Syft-generated file hashes or deterministic `salt`/`libsodium` relationships.

6. **Keep local and release paths equivalent.**
   - `make sbom` and `.github/workflows/release.yml` must produce matching metadata.
   - Release workflow must include all SBOMs in `SHA256SUMS`.
   - Release upload must include all SPDX and CycloneDX files.

7. **Validate.**
    - Run:
      ```sh
        bash -n scripts/add-libsodium-to-sbom.sh scripts/validate-sbom.sh
        make install-parlay
        make sbom
        make sbom-vuln
        make actionlint
        make docs
        make codespell
      ```
   - Inspect generated SPDX/CycloneDX JSON for package/component metadata, file metadata, and relationships/dependencies.

## Required Outputs

| Artifact | Output |
|---|---|
| Dynamic release layout | `build/sbom/salt-linux-amd64.spdx.json` |
| Dynamic release layout | `build/sbom/salt-linux-amd64.cyclonedx.json` |
| Static release layout | `build/sbom/salt-static-linux-amd64.spdx.json` |
| Static release layout | `build/sbom/salt-static-linux-amd64.cyclonedx.json` |

## Common Issues

- Syft source metadata set differently in Makefile and release workflow.
- SBOMs generated for the repository instead of the release staging layout.
- Shipped file entries are missing because all-file metadata selection was not enabled.
- `salt` file entry has zero SHA1/SHA256 or `NOASSERTION` license/copyright.
- Static SBOM omits explicit libsodium metadata because the dependency is linked into the binary.
- Release workflow uploads SBOMs but omits them from `SHA256SUMS`.
