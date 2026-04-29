---
name: update-syft
description: 'Update Syft pins, installation commands, CLI flags, and local/release invocation parity'
argument-hint: '[version, flags, config, or blank for full Syft review]'
---

# Update Syft Skill

Use this skill when changing the Syft version, checksum, installation path, CLI flags, configuration, or invocation parity between `make sbom` and the GitHub Actions release workflow.

## Workflow

1. **Inventory Syft usage.**
   - Read `Makefile` variables `SYFT_VERSION`, `SYFT_SHA256`, `SYFT_BIN`, `install-syft`, and `sbom`.
   - Read `.github/workflows/release.yml` Syft env vars, install step, and SBOM generation step.
   - Read `scripts/add-libsodium-to-sbom.sh` only to understand which fields Syft output feeds into post-processing.

2. **Update pins safely.**
   - Fetch the intended Syft release metadata from Anchore.
   - Update the exact version string and SHA256 checksum together.
   - Keep Makefile and release workflow values consistent.
   - Do not replace checksum verification with trust in TLS or GitHub Actions caching.

3. **Review CLI flags.**
   - Use `dir:<release-staging-dir>` sources.
   - Emit `spdx-json` and `cyclonedx-json` output.
   - Enable all-file metadata for release staging scans with `SYFT_FILE_METADATA_SELECTION=all` or equivalent shared config.
   - Preserve:
     ```sh
      --source-name salt
      --source-supplier CTFfactory
      --source-version <release-version>
      ```
   - Avoid broadening scans to the host filesystem or repository root.
   - **Inspect cataloger behavior** with `syft cataloger list` and generated Syft JSON before adding metadata post-processing. Syft catalogers auto-detect packages from package manager metadata (e.g., dpkg, RPM, npm), language ecosystems (e.g., Python, Go), and container images. The minimal release staging directories contain no package-manager databases, so cataloger-based package discovery is limited by design; SBOM post-processing scripts supply `libsodium` and Ubuntu dependency metadata explicitly to compensate for the minimal staging layout.

4. **Handle Syft configuration deliberately.**
   - Prefer CLI flags for release-critical metadata.
   - Add a config file only when it applies identically to local and CI generation.
   - If config changes affect output content, document the user-visible impact.
   - Use templates only for supplemental reports or carefully validated custom outputs; do not hand-roll SPDX/CycloneDX templates when Syft's official encoders suffice.

5. **Validate.**
   ```sh
   make install-syft
   make sbom
   make actionlint
   ```
   Run `make validate-copilot` if Syft customization files changed.

## Expected Result

Local `make sbom` and release workflow Syft invocations should use the same Syft version, checksum, source metadata, output formats, file metadata selection, and release staging scope.

## Common Issues

- Updating `SYFT_VERSION` without updating `SYFT_SHA256`.
- Using different source names in local and release paths.
- Replacing `dir:<staging>` with a repository-root scan.
- Assuming Syft can infer package metadata that is absent from the minimal release archive layout.
