# SBOM Parlay Enrichment

## Overview

The Salt project uses [Parlay](https://github.com/snyk/parlay) to enrich Software Bill of Materials (SBOMs) with additional metadata from external sources. This enrichment process enhances the quality and utility of SBOMs by adding vulnerability, ecosystem, and security risk scoring data.

## Enrichment Data Sources

### 1. npm Ecosystem Data (ecosystems enrich)

**Purpose**: Add package ecosystem metadata including download counts, publishing history, and popularity metrics.

**Data Included**:
- Package registry metadata (PyPI, npm, Maven, etc.)
- Download statistics and usage trends
- Package maintainer information
- License declarations from registries
- Repository and documentation links

**Example**: Enrich libsodium package with ecosystem data:
```bash
parlay ecosystems enrich salt-linux-amd64.spdx.json
```

**Validation**: The enriched SBOM will include ecosystem objects with component references and metadata.

### 2. GitHub Scorecard (scorecard enrich)

**Purpose**: Add security risk assessments for projects based on automated checks.

**Data Included**:
- Repository security score (0-10)
- Binary artifacts detection
- Branch protection checks
- CII Best Practices badge status
- Code review policy
- Dependency update frequency
- Fuzzing enabled/disabled
- License information
- Maintained status
- SAST testing integration
- Security policy presence
- Signed releases

**Example**: Score libsodium security posture:
```bash
parlay scorecard enrich salt-linux-amd64.spdx.json
```

**Validation**: The enriched SBOM will include scorecard objects with numeric scores for each check.

### 3. Snyk Vulnerability Database (snyk enrich)

**Purpose**: Cross-reference package versions against known vulnerabilities in Snyk's database.

**Data Included**:
- Known CVEs for package versions
- Vulnerability severity (critical, high, medium, low)
- CVSS scores
- Snyk vulnerability IDs
- Remediation guidance
- Affected version ranges
- Published/disclosed dates

**Example**: Identify known vulnerabilities:
```bash
SNYK_TOKEN=<token> parlay snyk enrich salt-linux-amd64.spdx.json
```

**Note**: Requires `SNYK_TOKEN` environment variable. Controlled by `PARLAY_SNYK_ENRICH=1` in release workflow.

**Validation**: The enriched SBOM will include vulnerability objects with CVE and Snyk reference data.

## Enrichment Flow

The release workflow applies enrichment in this order:

1. **Ecosystems Enrich** - Adds package metadata
2. **Scorecard Enrich** - Adds security assessment
3. **Snyk Enrich** (Optional) - Adds vulnerability data (requires token)

## SBOM Formats Supported

- **SPDX 2.3 JSON**: Full enrichment support
- **CycloneDX 1.6 JSON**: Full enrichment support

## Validation

After enrichment, SBOMs are validated with:

```bash
./scripts/validate-sbom.sh \
  --spdx salt-linux-amd64.spdx.json:salt \
  --cyclonedx salt-linux-amd64.cyclonedx.json:salt \
  --verify-deterministic \
  --component-count-min 90
```

Validation checks:
- SBOM format compliance
- Schema validation
- Required metadata presence
- Component relationship integrity
- Minimum component count (≥90)
- Deterministic namespace/timestamps

## Configuration

### Environment Variables

- `PARLAY_VERSION`: Parlay tool version (default: 0.11.0)
- `PARLAY_SNYK_ENRICH`: Enable Snyk enrichment (0=disabled, 1=enabled)
- `SNYK_TOKEN`: Snyk API token (required if PARLAY_SNYK_ENRICH=1)

### Make Targets

```bash
make sbom                # Generate and enrich SBOMs
make sbom-parlay        # Run only Parlay enrichment
make sbom-verify        # Verify component counts (>= 90)
```

## Enriched SBOM Examples

### SPDX with Ecosystems Metadata
```json
{
  "SPDXID": "SPDXRef-Package-libsodium",
  "name": "libsodium",
  "externalRefs": [
    {
      "referenceType": "purl",
      "referenceLocator": "pkg:generic/libsodium@1.0.18"
    }
  ]
  // Ecosystems enrichment adds:
  // "downloadCount", "lastPublish", "repository", etc.
}
```

### CycloneDX with Scorecard Metadata
```json
{
  "type": "library",
  "name": "libsodium",
  "properties": [
    {
      "name": "security-scorecard-score",
      "value": "7.5"
    },
    {
      "name": "security-scorecard-maintained",
      "value": "true"
    }
  ]
}
```

## Troubleshooting

### Missing Enrichment Data

**Issue**: Parlay fails silently or enrichment data is not added.

**Resolution**:
- Verify SBOM files exist: `ls -la sbom/salt-*.json`
- Check Parlay installation: `./tools/parlay --version`
- Review enrichment logs: Parlay outputs to stderr

### Snyk Enrichment Failures

**Issue**: "FAIL: PARLAY_SNYK_ENRICH=1 requires SNYK_TOKEN"

**Resolution**:
- Set SNYK_TOKEN: `export SNYK_TOKEN=...`
- Verify token validity with Snyk CLI
- Or disable: `PARLAY_SNYK_ENRICH=0 make sbom`

### Component Count Too Low

**Issue**: "component count X is below minimum 90"

**Resolution**:
- Verify binaries were copied to staging: `ls sbom/staging/*/salt*`
- Check that Syft correctly scanned files
- Ensure no build failures in Syft invocation

## References

- [Parlay Documentation](https://github.com/snyk/parlay)
- [SPDX Specification](https://spdx.dev/)
- [CycloneDX Specification](https://cyclonedx.org/)
- [Snyk Vulnerability Database](https://snyk.io/vuln/)
- [GitHub Security Scorecards](https://securityscorecards.dev/)

