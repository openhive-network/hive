#!/bin/bash
#
# hiveio_api_package.sh - Build, test, and deploy the hiveio-api Python package
#
# This script handles the full package lifecycle:
#   1. Determines package version from git state (no generation needed)
#   2. Checks if the package version already exists in GitLab PyPI registry
#   3. If exists: creates env file and exits (skips generation/build entirely)
#   4. If not: generates package, runs generator tests, builds wheel, runs package tests, and deploys to registry
#
# The build_wheel.env artifact is always created with the resolved version.
# When the version exists in the registry, generation and build are skipped entirely.
#
# Usage:
#   ./hiveio_api_package.sh [options]
#
# Options:
#   --hive-project-root=PATH      Root directory of the Hive project (default: pwd)
#   --api-generation-dir=PATH     API generation directory (default: HIVE_PROJECT_ROOT/libraries/plugins/apis/api_generation)
#   --package-registry-project-id=ID  Project ID for package registry (default: CI_PROJECT_ID)
#   --skip-registry-check         Skip checking if package exists in registry (always build)
#   --skip-deploy                 Skip deployment to GitLab PyPI registry
#   --skip-tests                  Skip verifying the built wheel (generator tests always run)
#   --operations-description=PATH Description of operations from the build of hived (operations_description.json),
#                                 required when the package is generated
#   --apis=<list>                 Space-separated list of APIs to generate (default: all)
#   --env-var-name=<name>         Environment variable name in build_wheel.env (default: WHEEL_BUILD_VERSION)
#   --help                        Show this help message
#
# Environment variables (GitLab CI):
#   CI_API_V4_URL               GitLab API URL (required for registry operations)
#   CI_JOB_TOKEN                GitLab CI job token (required for registry operations)
#
# Output:
#   - Generated package:  ${API_GENERATION_DIR}/python_api_package/
#   - Wheel:              ${API_GENERATION_DIR}/python_api_package/dist/*.whl
#   - Env file:           ${HIVE_PROJECT_ROOT}/build_wheel.env
#

set -euo pipefail

# ==============================================================================
# Configuration
# ==============================================================================

# Colors for output (use defaults if not set by CI)
TXT_GREEN="${TXT_GREEN:-\e[1;32m}"
TXT_BLUE="${TXT_BLUE:-\e[1;34m}"
TXT_YELLOW="${TXT_YELLOW:-\e[1;33m}"
TXT_RED="${TXT_RED:-\e[1;31m}"
TXT_CLEAR="${TXT_CLEAR:-\e[0m}"

# Project paths (set via arguments, with defaults)
HIVE_PROJECT_ROOT=""
API_GENERATION_DIR=""

# Derived paths (set after argument parsing)
GENERATOR_PYPROJECT_DIR=""
GENERATED_PACKAGE_DIR=""
WHEEL_OUTPUT_DIR=""

# Package configuration
PACKAGE_NAME="hiveio-api"
PACKAGE_REGISTRY_PROJECT_ID=""

# Script options
OPT_SKIP_DEPLOY=""
OPT_SKIP_TESTS=""
OPT_SKIP_REGISTRY_CHECK=""
OPT_APIS=""
OPT_OPERATIONS_DESCRIPTION=""
OPT_ENV_VAR_NAME="WHEEL_BUILD_VERSION"

# ==============================================================================
# Logging
# ==============================================================================

log_info()    { echo -e "${TXT_BLUE}$*${TXT_CLEAR}"; }
log_success() { echo -e "${TXT_GREEN}$*${TXT_CLEAR}"; }
log_warning() { echo -e "${TXT_YELLOW}$*${TXT_CLEAR}"; }
log_error()   { echo -e "${TXT_RED}$*${TXT_CLEAR}"; }

# ==============================================================================
# Helpers
# ==============================================================================

# Prints usage information extracted from script header comments.
show_help() {
    head -53 "$0" | grep -E '^#' | sed 's/^# \?//'
    exit 0
}

# Parses command line arguments and sets configuration variables.
parse_args() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            --hive-project-root=*)
                HIVE_PROJECT_ROOT="${1#*=}"; shift ;;
            --api-generation-dir=*)
                API_GENERATION_DIR="${1#*=}"; shift ;;
            --package-registry-project-id=*)
                PACKAGE_REGISTRY_PROJECT_ID="${1#*=}"; shift ;;
            --skip-deploy)
                OPT_SKIP_DEPLOY="true"; shift ;;
            --skip-tests)
                OPT_SKIP_TESTS="true"; shift ;;
            --skip-registry-check)
                OPT_SKIP_REGISTRY_CHECK="true"; shift ;;
            --apis=*)
                OPT_APIS="${1#*=}"; shift ;;
            --operations-description=*)
                OPT_OPERATIONS_DESCRIPTION="${1#*=}"; shift ;;
            --env-var-name=*)
                OPT_ENV_VAR_NAME="${1#*=}"; shift ;;
            --help|-h)
                show_help ;;
            *)
                log_error "Unknown option: $1"; show_help ;;
        esac
    done

    # Set defaults for paths if not provided
    HIVE_PROJECT_ROOT="${HIVE_PROJECT_ROOT:-$(pwd)}"
    API_GENERATION_DIR="${API_GENERATION_DIR:-${HIVE_PROJECT_ROOT}/libraries/plugins/apis/api_generation}"
    PACKAGE_REGISTRY_PROJECT_ID="${PACKAGE_REGISTRY_PROJECT_ID:-${CI_PROJECT_ID:-}}"

    # Set derived paths
    GENERATOR_PYPROJECT_DIR="${API_GENERATION_DIR}/api_generation"
    GENERATED_PACKAGE_DIR="${API_GENERATION_DIR}/python_api_package"
    WHEEL_OUTPUT_DIR="${GENERATED_PACKAGE_DIR}/dist"
}

# Returns 0 if all required CI variables for registry access are set.
has_registry_access() {
    [[ -n "${CI_API_V4_URL:-}" ]] && \
    [[ -n "${CI_JOB_TOKEN:-}" ]] && \
    [[ -n "${PACKAGE_REGISTRY_PROJECT_ID:-}" ]]
}

# Returns the GitLab API base URL for package operations.
get_registry_api_url() {
    echo "${CI_API_V4_URL}/projects/${PACKAGE_REGISTRY_PROJECT_ID}/packages"
}

# Returns the PyPI simple index URL for pip operations.
get_pypi_index_url() {
    echo "${CI_API_V4_URL}/projects/${PACKAGE_REGISTRY_PROJECT_ID}/packages/pypi/simple"
}

# Extracts hostname from CI_API_V4_URL for --trusted-host flag.
get_registry_host() {
    echo "${CI_API_V4_URL}" | sed 's|https\?://||' | cut -d'/' -f1
}

# Verifies that a wheel file exists in WHEEL_OUTPUT_DIR.
# Arguments: $1 - action description for log message (e.g., "built", "downloaded")
verify_wheel_exists() {
    local action_description="$1"

    if ls "${WHEEL_OUTPUT_DIR}"/*.whl 1>/dev/null 2>&1; then
        log_success "Wheel ${action_description}:"
        ls -la "${WHEEL_OUTPUT_DIR}"/*.whl
        return 0
    else
        log_error "No wheel found in ${WHEEL_OUTPUT_DIR}"
        return 1
    fi
}

# ==============================================================================
# Package Generation
# ==============================================================================

# Generates the hiveio_api package from API definitions using the generator script.
# Infrastructure (pyproject.toml, poetry.lock, .gitignore) is committed in the repo.
# API subpackages, __init__.py, and README.md are generated and gitignored.
generate_package() {
    log_info "Generating hiveio_api API subpackages..."
    cd "${API_GENERATION_DIR}"
    poetry -C "${GENERATOR_PYPROJECT_DIR}" install

    if [[ -n "${OPT_APIS}" ]]; then
        for api in ${OPT_APIS}; do
            if [[ ! "${api}" =~ ^[a-z_][a-z0-9_]*$ ]]; then
                log_error "Invalid API name: '${api}' (must match [a-z_][a-z0-9_]*)"
                exit 1
            fi
        done
        # shellcheck disable=SC2086
        "${API_GENERATION_DIR}/generate_api_packages.sh" "--operations-description=${OPT_OPERATIONS_DESCRIPTION}" ${OPT_APIS}
    else
        "${API_GENERATION_DIR}/generate_api_packages.sh" "--operations-description=${OPT_OPERATIONS_DESCRIPTION}"
    fi

    log_success "Package generated successfully"
}

# Runs tests of the generator and of the generated package (api_generation/tests).
# Not affected by --skip-tests: they need only the generator environment.
run_generator_tests() {
    log_info "Running generator tests..."
    cd "${GENERATOR_PYPROJECT_DIR}"
    # missing generated package fails the tests instead of skipping them
    HIVEIO_API_REQUIRE_GENERATED_PACKAGE=1 poetry -C "${GENERATOR_PYPROJECT_DIR}" run python -m pytest tests
    log_success "Generator tests passed"
}

# Returns the version string from the generated package's pyproject.toml.
get_package_version() {
    cd "${GENERATED_PACKAGE_DIR}"
    # Workaround: installs plugins from [tool.poetry.requires-plugins] without installing project dependencies.
    # See: https://github.com/python-poetry/poetry/issues/9990#issuecomment-2737176168
    poetry install --dry-run >&2
    poetry version -s
}

# ==============================================================================
# Registry Operations
# ==============================================================================

# Checks if the specified package version exists in GitLab PyPI registry.
# Arguments: $1 - version string to check
# Returns: 0 if package exists, 1 otherwise
check_package_exists_in_registry() {
    local version="$1"
    local api_url
    api_url=$(get_registry_api_url)

    log_info "Checking if ${PACKAGE_NAME}==${version} exists in registry..."

    local response
    response=$(curl -s --header "JOB-TOKEN: ${CI_JOB_TOKEN}" \
        "${api_url}?package_type=pypi&package_name=${PACKAGE_NAME}&package_version=${version}" 2>/dev/null || echo "[]")

    local package_count
    package_count=$(echo "${response}" | python3 -c "import sys, json; data=json.load(sys.stdin); print(len(data) if isinstance(data, list) else 0)" 2>/dev/null || echo "0")

    if [[ "${package_count}" -gt 0 ]]; then
        log_success "Package found in registry"
        return 0
    else
        log_info "Package not found in registry"
        return 1
    fi
}

# Downloads the wheel for specified version from GitLab PyPI registry.
# Arguments: $1 - version string to download
download_wheel_from_registry() {
    local version="$1"
    local pypi_url
    local registry_host
    pypi_url=$(get_pypi_index_url)
    registry_host=$(get_registry_host)

    log_info "Downloading ${PACKAGE_NAME}==${version} from registry..."

    mkdir -p "${WHEEL_OUTPUT_DIR}"

    pip download "${PACKAGE_NAME}==${version}" \
        --index-url "${pypi_url}" \
        --no-deps \
        --only-binary=:all: \
        --dest "${WHEEL_OUTPUT_DIR}" \
        --trusted-host "${registry_host}"

    verify_wheel_exists "downloaded"
}

# Deploys the built wheel to GitLab PyPI registry.
deploy_wheel_to_registry() {
    local registry_url
    registry_url="$(get_registry_api_url)/pypi"

    log_info "Deploying to GitLab PyPI registry..."
    cd "${GENERATED_PACKAGE_DIR}"

    poetry config repositories.gitlab "${registry_url}"
    poetry config http-basic.gitlab gitlab-ci-token "${CI_JOB_TOKEN}"
    poetry publish --skip-existing --repository gitlab

    log_success "Deployment completed"
}

# ==============================================================================
# Build & Test
# ==============================================================================

# Builds the wheel using poetry.
build_wheel() {
    log_info "Building hiveio-api wheel..."
    cd "${GENERATED_PACKAGE_DIR}"

    log_info "Validating poetry.lock consistency..."
    poetry check --lock

    poetry version
    poetry build --format wheel

    verify_wheel_exists "built"
}

# Installs the built wheel - not the sources, checked by the generator tests - and imports everything it exports: every
# public name of every API and every validation module (catches files missing in the wheel).
run_package_tests() {
    log_info "Installing the built wheel..."
    poetry -C "${GENERATED_PACKAGE_DIR}" install --no-root
    python3 -m pip install --no-deps --force-reinstall "${WHEEL_OUTPUT_DIR}"/*.whl

    log_info "Verifying the wheel..."
    (cd / && python3 - <<'PY'
from __future__ import annotations

import importlib
import sys

import hiveio_api

errors: list[str] = []
if "site-packages" not in hiveio_api.__file__:
    errors.append(f"hiveio_api imported from {hiveio_api.__file__}, not from the installed wheel")

for module_name in [f"hiveio_api.{api}" for api in hiveio_api.APIS] + ["hiveio_api.operations"]:
    module = importlib.import_module(module_name)
    for symbol in module.__all__:
        try:
            getattr(module, symbol)
        except Exception as exception:
            errors.append(f"{module_name}.{symbol}: {type(exception).__name__}: {exception}")

# public modules do not load validation models - only an explicit validate_schema call does
loaded = [name for name in sys.modules if name.startswith("hiveio_api._validation.")]
if loaded:
    errors.append(f"Validation models loaded without validate_schema call: {loaded}")

for api in hiveio_api.APIS:
    try:
        importlib.import_module(f"hiveio_api._validation.{api}").ENDPOINT_RESULTS
    except Exception as exception:
        errors.append(f"hiveio_api._validation.{api}: {type(exception).__name__}: {exception}")

if errors:
    raise SystemExit("\n".join(errors))
PY
    )
    log_success "Wheel verified"
}

# ==============================================================================
# Output
# ==============================================================================

# Creates build_wheel.env file with version and metadata for CI artifacts.
# Arguments: $1 - version, $2 - whether package was downloaded (true/false)
create_build_env_file() {
    local version="$1"
    local was_downloaded="${2:-false}"
    local env_file="${HIVE_PROJECT_ROOT}/build_wheel.env"

    log_info "Creating ${env_file}..."

    cat > "${env_file}" << EOF
${OPT_ENV_VAR_NAME}=${version}
PACKAGE_DOWNLOADED=${was_downloaded}
EOF

    log_success "Created ${env_file}:"
    cat "${env_file}"
}

# ==============================================================================
# Main Workflow
# ==============================================================================

# Attempts to download existing package from registry.
# Returns 0 on success, 1 if download should be skipped or failed.
try_download_existing_package() {
    local version="$1"

    if [[ -n "${OPT_SKIP_REGISTRY_CHECK}" ]]; then
        log_info "Registry check skipped (--skip-registry-check)"
        return 1
    fi

    if ! has_registry_access; then
        log_warning "No registry access configured, skipping registry check"
        return 1
    fi

    if ! check_package_exists_in_registry "${version}"; then
        return 1
    fi

    if download_wheel_from_registry "${version}"; then
        log_success "Using existing package from registry"
        return 0
    else
        log_warning "Download failed, will build locally"
        return 1
    fi
}

# Builds, tests, and deploys the package.
build_test_and_deploy() {
    build_wheel

    if [[ -z "${OPT_SKIP_TESTS}" ]]; then
        run_package_tests
    else
        log_warning "Tests skipped (--skip-tests)"
    fi

    if [[ -z "${OPT_SKIP_DEPLOY}" ]]; then
        if has_registry_access; then
            deploy_wheel_to_registry
        else
            log_warning "No registry access configured, skipping deployment"
        fi
    else
        log_warning "Deployment skipped (--skip-deploy)"
    fi
}

main() {
    parse_args "$@"

    if [[ ! "${OPT_ENV_VAR_NAME}" =~ ^[A-Za-z_][A-Za-z0-9_]*$ ]]; then
        log_error "Invalid --env-var-name: '${OPT_ENV_VAR_NAME}' (must be a valid shell variable name)"
        exit 1
    fi

    log_info "=========================================="
    log_info "hiveio-api package build"
    log_info "=========================================="
    log_info "HIVE_PROJECT_ROOT: ${HIVE_PROJECT_ROOT}"
    log_info "API_GENERATION_DIR: ${API_GENERATION_DIR}"

    # Step 1: Get version (before generation - only needs git state + pyproject.toml)
    local version
    version=$(get_package_version)
    log_info "Package version: ${version}"

    # Step 2: Try to use existing package from registry (skip generation entirely)
    if try_download_existing_package "${version}"; then
        create_build_env_file "${version}" "true"
        log_success "=========================================="
        log_success "hiveio-api package build completed (from registry)"
        log_success "=========================================="
        return
    fi

    # Step 3: Package not in registry - generate, build, test, deploy
    generate_package
    run_generator_tests
    build_test_and_deploy

    # Step 4: Create env file
    create_build_env_file "${version}" "false"

    log_success "=========================================="
    log_success "hiveio-api package build completed (built locally)"
    log_success "=========================================="
}

main "$@"
