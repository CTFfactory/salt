# Placeholder Variables

Use these placeholders when converting TEMPLATE files to project-agnostic form.

## Repository Identity

- {{PROJECT_NAME}}: short repository or product name.
- {{PROJECT_DISPLAY_NAME}}: reader-friendly project name.
- {{GITHUB_ORG}}: GitHub organization or owner.
- {{REPO_NAME}}: GitHub repository name.
- {{MODULE_PATH}}: Go module path or equivalent package namespace.

## Tooling

- {{TOOL_LIST}}: comma-separated list of first-party CLI tools.
- {{TOOL_COUNT}}: numeric count of tools included in {{TOOL_LIST}}.
- {{PRIMARY_LANGUAGE}}: main implementation language.
- {{BUILD_SYSTEM}}: build system used by the project.

## CI and Release

- {{COVERAGE_THRESHOLD}}: enforced minimum test coverage.
- {{CI_FAST_TARGET}}: default fast CI target name.
- {{CI_FULL_TARGET}}: full CI target name.
- {{RELEASE_VALIDATE_TARGET}}: release-readiness target name.
- {{RETENTION_DAYS}}: artifact retention period in days.
- {{ORG_STORAGE_POLICY}}: organization-specific storage guidance.

## Infrastructure and Security

- {{ENV_PREFIX}}: prefix for environment variables.
- {{API_ENDPOINT_VAR}}: primary API URL environment variable.
- {{AUTH_TOKEN_ID_VAR}}: token ID variable name.
- {{AUTH_TOKEN_SECRET_VAR}}: token secret variable name.
- {{TLS_INSECURE_FLAG}}: insecure transport override flag.
- {{CONFIG_FILE_PATH}}: default local config file path.

## Monitoring Overlay

- {{MONITORING_PLATFORM}}: monitoring stack name.
- {{CHECK_COMMAND_DIR}}: plugin directory path.
- {{ERROR_PATTERN_SOURCE}}: canonical error taxonomy source.
- {{HOST_VAR_KEY}}: host variable key used in monitoring templates.

## Usage Rules

1. Do not mix literal prox-ops values with placeholders in the same sentence.
2. Keep placeholder names uppercase snake case with double braces.
3. When a value is optional, document fallback behavior next to the placeholder.
4. After substitution, run validation to ensure no unresolved placeholders remain.