#!/bin/bash
#
# cache-manager.sh - Centralized CI cache manager with NFS backing
#
# Provides cross-builder cache sharing using NFS with LRU eviction.
# Designed for HAF replay caches and downstream project caches.
#
# Usage:
#   cache-manager.sh get <cache-type> <cache-key> <local-dest>
#   cache-manager.sh put <cache-type> <cache-key> <local-source>
#   cache-manager.sh cleanup <cache-type> [--max-size-gb N] [--max-age-days N]
#   cache-manager.sh cleanup-local [<cache-type>]  # Clean local cache by LRU
#   cache-manager.sh list <cache-type>
#   cache-manager.sh status
#   cache-manager.sh is-fast-builder    # Check if current host is a fast builder
#
# CI Tag Requirements:
#   Replay/build jobs should use: tags: [data-cache-storage, fast]
#   Fast builders (AMD 5950): hive-builder-8, hive-builder-9, hive-builder-10
#
# Cache types: hive, haf, balance_tracker, hivemind, etc.
#
# Environment variables:
#   CACHE_NFS_PATH      - NFS mount point (default: /nfs/ci-cache)
#   CACHE_LOCAL_PATH    - Local cache directory (default: /cache)
#   CACHE_MAX_SIZE_GB   - Max total NFS cache size (default: 2000)
#   CACHE_MAX_AGE_DAYS  - Max NFS cache age (default: 30)
#   CACHE_LOCAL_MAX_SIZE_GB - Max local cache size (default: auto 70% of available)
#   CACHE_LOCAL_MAX_AGE_DAYS - Max local cache age (default: 7)
#   CACHE_LOCK_TIMEOUT  - Lock timeout in seconds (default: 3600)
#   CACHE_QUIET         - Suppress verbose output (default: false)
#
# Per-builder config: Create /storage1/cache/.cache-config to override auto limits

set -euo pipefail

# Detect flock capabilities (BusyBox vs GNU coreutils)
# BusyBox flock doesn't support -w (timeout), only -n (nonblock)
_flock_supports_timeout() {
    flock --help 2>&1 | grep -q -- '-w' 2>/dev/null
}

# Wrapper for flock that handles BusyBox compatibility
# Usage: _flock_with_timeout <timeout> <mode> <lockfile> <command...>
#   mode: -s (shared) or -x (exclusive)
_flock_with_timeout() {
    local timeout="$1"
    local mode="$2"
    local lockfile="$3"
    shift 3

    if _flock_supports_timeout; then
        # GNU coreutils flock - use -w for timeout
        flock "$mode" -w "$timeout" "$lockfile" "$@"
    else
        # BusyBox flock - no timeout support, use -n (non-blocking) with retry loop
        local elapsed=0
        local interval=5
        while [[ $elapsed -lt $timeout ]]; do
            if flock "$mode" -n "$lockfile" "$@" 2>/dev/null; then
                return 0
            fi
            sleep "$interval"
            elapsed=$((elapsed + interval))
        done
        _error "Timeout waiting for lock after ${timeout}s"
        return 1
    fi
}

# Configuration with defaults
CACHE_NFS_PATH="${CACHE_NFS_PATH:-/nfs/ci-cache}"
CACHE_LOCAL_PATH="${CACHE_LOCAL_PATH:-/cache}"
CACHE_MAX_SIZE_GB="${CACHE_MAX_SIZE_GB:-2000}"
CACHE_MAX_AGE_DAYS="${CACHE_MAX_AGE_DAYS:-30}"
CACHE_LOCK_TIMEOUT="${CACHE_LOCK_TIMEOUT:-3600}"
CACHE_QUIET="${CACHE_QUIET:-false}"

# Local cache limits (will be auto-configured if not set)
CACHE_LOCAL_MAX_SIZE_GB="${CACHE_LOCAL_MAX_SIZE_GB:-}"
CACHE_LOCAL_MAX_AGE_DAYS="${CACHE_LOCAL_MAX_AGE_DAYS:-}"

# Load per-builder config if exists (overrides defaults)
LOCAL_CONFIG="${CACHE_LOCAL_PATH}/.cache-config"
if [[ -f "$LOCAL_CONFIG" ]]; then
    # shellcheck source=/dev/null
    source "$LOCAL_CONFIG"
fi

# Auto-configure local cache limits based on available space
_auto_configure_local_limits() {
    if [[ -d "$CACHE_LOCAL_PATH" ]]; then
        local avail_kb
        avail_kb=$(df --output=avail "$CACHE_LOCAL_PATH" 2>/dev/null | tail -1) || avail_kb=0
        local avail_gb=$((avail_kb / 1024 / 1024))
        # Default to 70% of available space
        : "${CACHE_LOCAL_MAX_SIZE_GB:=$((avail_gb * 70 / 100))}"
    else
        : "${CACHE_LOCAL_MAX_SIZE_GB:=500}"
    fi
    # Default age to 7 days for local cache
    : "${CACHE_LOCAL_MAX_AGE_DAYS:=7}"
}
_auto_configure_local_limits

# Logging
_log() {
    if [[ "$CACHE_QUIET" != "true" ]]; then
        echo "[cache-manager] $1" >&2
    fi
}

_error() {
    echo "[cache-manager] ERROR: $1" >&2
}

# Check if running on the NFS host (where NFS path is local, not a mount)
# On NFS host: /nfs/ci-cache is a symlink to /storage1/ci-cache (local storage)
# On clients: /nfs/ci-cache is an NFS mount point
_is_nfs_host() {
    # If it's a symlink, we're on the NFS host
    if [[ -L "$CACHE_NFS_PATH" ]]; then
        return 0
    fi
    # If it exists but is NOT a mount point, we're on the NFS host
    if [[ -d "$CACHE_NFS_PATH" ]] && ! mountpoint -q "$CACHE_NFS_PATH" 2>/dev/null; then
        return 0
    fi
    return 1
}

# Check if NFS is mounted and accessible (or we're on the NFS host)
_nfs_available() {
    # On NFS host, the path is local (symlink or direct), not a mount
    if _is_nfs_host; then
        [[ -d "$CACHE_NFS_PATH" ]]
        return $?
    fi
    # On clients, check for mount
    [[ -d "$CACHE_NFS_PATH" ]] && mountpoint -q "$CACHE_NFS_PATH" 2>/dev/null
}

# Get paths for a cache entry
_get_paths() {
    local cache_type="$1"
    local cache_key="$2"

    NFS_CACHE_DIR="${CACHE_NFS_PATH}/${cache_type}/${cache_key}"

    # On NFS host, use NFS path as local path to avoid redundant copies
    if _is_nfs_host; then
        LOCAL_CACHE_DIR="$NFS_CACHE_DIR"
    else
        # Use subdirectory structure matching NFS (not underscore-separated)
        LOCAL_CACHE_DIR="${CACHE_LOCAL_PATH}/${cache_type}/${cache_key}"
    fi

    LOCK_FILE="${NFS_CACHE_DIR}/.lock"
    METADATA_FILE="${NFS_CACHE_DIR}/.metadata"
    LRU_INDEX="${CACHE_NFS_PATH}/.lru_index"
    GLOBAL_LOCK="${CACHE_NFS_PATH}/.global_lock"
    LOCAL_LOCK_FILE="${LOCAL_CACHE_DIR}/.lock"
}

# Update LRU index with access timestamp
_update_lru() {
    local cache_type="$1"
    local cache_key="$2"
    local timestamp=$(date +%s)
    local entry="${cache_type}/${cache_key}"

    # Acquire global lock for index update
    touch "$GLOBAL_LOCK"
    _flock_with_timeout 30 -x "$GLOBAL_LOCK" -c "
        # Create or update LRU index (simple format: timestamp|path per line)
        if [[ -f '$LRU_INDEX' ]]; then
            # Remove old entry and add new one
            grep -v '^[0-9]*|${entry}\$' '$LRU_INDEX' > '${LRU_INDEX}.tmp' 2>/dev/null || true
            echo '${timestamp}|${entry}' >> '${LRU_INDEX}.tmp'
            mv '${LRU_INDEX}.tmp' '$LRU_INDEX'
        else
            echo '${timestamp}|${entry}' > '$LRU_INDEX'
        fi
    " || _error "Failed to acquire global lock for LRU update"
}

# Update local cache LRU index with access timestamp
_update_local_lru() {
    local cache_type="$1"
    local cache_key="$2"
    local timestamp
    timestamp=$(date +%s)
    local entry="${cache_type}/${cache_key}"
    local local_lru_index="${CACHE_LOCAL_PATH}/.lru_index"
    local local_lru_lock="${CACHE_LOCAL_PATH}/.lru_lock"

    mkdir -p "$CACHE_LOCAL_PATH"
    touch "$local_lru_lock"
    _flock_with_timeout 30 -x "$local_lru_lock" -c "
        if [[ -f '$local_lru_index' ]]; then
            grep -v '^[0-9]*|${entry}\$' '$local_lru_index' > '${local_lru_index}.tmp' 2>/dev/null || true
            echo '${timestamp}|${entry}' >> '${local_lru_index}.tmp'
            mv '${local_lru_index}.tmp' '$local_lru_index'
        else
            echo '${timestamp}|${entry}' > '$local_lru_index'
        fi
    " || _error "Failed to update local LRU"
}

# Write metadata for a cache entry
_write_metadata() {
    local cache_type="$1"
    local cache_key="$2"
    local source_dir="$3"

    local timestamp=$(date -Iseconds)
    local size=$(du -sb "$source_dir" 2>/dev/null | cut -f1 || echo 0)
    local hostname=$(hostname)

    cat > "$METADATA_FILE" <<EOF
{
    "cache_type": "${cache_type}",
    "cache_key": "${cache_key}",
    "created_at": "${timestamp}",
    "size_bytes": ${size},
    "source_builder": "${hostname}",
    "ci_pipeline_id": "${CI_PIPELINE_ID:-unknown}",
    "ci_job_id": "${CI_JOB_ID:-unknown}"
}
EOF
}

# Relax PostgreSQL pgdata permissions for caching
# Makes pgdata readable so it can be copied to NFS
_relax_pgdata_permissions() {
    local source_dir="$1"
    local pgdata_path="${source_dir}/datadir/haf_db_store/pgdata"

    if [[ -d "$pgdata_path" ]]; then
        _log "Relaxing pgdata permissions for caching"
        # Make readable for copying (PostgreSQL creates mode 700)
        sudo chmod -R a+rX "$pgdata_path" 2>/dev/null || chmod -R a+rX "$pgdata_path" 2>/dev/null || true
    fi
}

# Restore PostgreSQL pgdata permissions after cache retrieval
# pgdata must be mode 700 or 750, owned by postgres user for PostgreSQL to start
_restore_pgdata_permissions() {
    local dest_dir="$1"
    local pgdata_path="${dest_dir}/datadir/haf_db_store/pgdata"

    if [[ -d "$pgdata_path" ]]; then
        _log "Restoring pgdata permissions to mode 700"
        # Restore strict permissions required by PostgreSQL
        sudo chmod 700 "$pgdata_path" 2>/dev/null || chmod 700 "$pgdata_path" 2>/dev/null || true
        # Restore ownership to postgres user (uid 105 in HAF containers)
        sudo chown -R 105:105 "$pgdata_path" 2>/dev/null || true
    fi
}

# Build tar exclusion arguments for HAF caches to reduce size
# Excludes: unnecessary pg_wal files, blockchain (use shared block_log)
# Saves ~7.5GB (38%) on typical HAF cache
_build_haf_tar_excludes() {
    local source_dir="$1"
    local excludes=""

    # Exclude blockchain directory - tests should use /nfs/ci-cache/hive/block_log_5m
    # Saves ~1.7GB
    if [[ -d "${source_dir}/datadir/blockchain" ]]; then
        excludes="--exclude=./datadir/blockchain"
        _log "Excluding datadir/blockchain (use shared block_log instead)"
    fi

    # Exclude pg_wal files except the checkpoint WAL file
    # PostgreSQL only needs the WAL file containing the latest checkpoint
    # Saves ~5.8GB (375 files × 16MB)
    local pgdata_path="${source_dir}/datadir/haf_db_store/pgdata"
    local pg_wal_path="${pgdata_path}/pg_wal"

    if [[ -d "$pg_wal_path" ]]; then
        # Find checkpoint WAL file using pg_controldata via docker
        local checkpoint_wal=""
        if command -v docker &>/dev/null; then
            checkpoint_wal=$(docker run --rm -v "${pgdata_path}:/pgdata:ro" postgres:17 \
                pg_controldata /pgdata 2>/dev/null | \
                grep "REDO WAL file" | awk '{print $NF}')
        fi

        if [[ -n "$checkpoint_wal" ]]; then
            _log "Keeping checkpoint WAL: $checkpoint_wal, excluding others"
            # Build exclusions for all WAL files except checkpoint
            local wal_count=0
            local excluded_count=0
            for wal_file in "$pg_wal_path"/0*; do
                if [[ -f "$wal_file" ]]; then
                    wal_count=$((wal_count + 1))
                    local wal_name=$(basename "$wal_file")
                    if [[ "$wal_name" != "$checkpoint_wal" ]]; then
                        excludes="$excludes --exclude=./datadir/haf_db_store/pgdata/pg_wal/$wal_name"
                        excluded_count=$((excluded_count + 1))
                    fi
                fi
            done
            _log "Excluding $excluded_count of $wal_count WAL files"
        else
            _log "Could not determine checkpoint WAL, keeping all WAL files"
        fi
    fi

    echo "$excludes"
}

# GET: Check local cache, then NFS, populate local cache from NFS if needed
# Two-tier caching: local cache has real data (not symlinks), NFS is backing store
cmd_get() {
    local cache_type="$1"
    local cache_key="$2"
    local local_dest="$3"

    _get_paths "$cache_type" "$cache_key"

    local is_nfs_host=false
    _is_nfs_host && is_nfs_host=true

    # 1. Check local cache first (real data, not symlinks)
    # On NFS host, LOCAL_CACHE_DIR == NFS_CACHE_DIR
    if [[ -d "$LOCAL_CACHE_DIR" && ! -L "$LOCAL_CACHE_DIR" ]]; then
        _log "Local cache hit: $LOCAL_CACHE_DIR"

        if [[ "$LOCAL_CACHE_DIR" != "$local_dest" ]]; then
            # Use shared lock during copy to prevent cleanup from deleting while reading
            mkdir -p "$(dirname "$LOCAL_LOCK_FILE")"
            touch "$LOCAL_LOCK_FILE" 2>/dev/null || true

            if _flock_with_timeout "$CACHE_LOCK_TIMEOUT" -s "$LOCAL_LOCK_FILE" -c "
                echo '[cache-manager] Copying from local cache to destination: $local_dest' >&2
                mkdir -p '$local_dest'
                cp -r '$LOCAL_CACHE_DIR'/. '$local_dest'/
            "; then
                _log "Copied from local cache successfully"
            else
                _log "Failed to acquire shared lock on local cache, falling back to NFS"
                # Fall through to NFS path
                if [[ "$is_nfs_host" == "true" ]]; then
                    return 1
                fi
            fi
        else
            _log "Destination is cache dir, no copy needed"
        fi

        # Restore pgdata permissions for HAF caches
        if [[ "$cache_type" == "haf" || "$cache_type" == "haf_sync" ]]; then
            _restore_pgdata_permissions "$local_dest"
        fi

        # Update LRU indices
        _update_local_lru "$cache_type" "$cache_key" || true
        if _nfs_available; then
            _update_lru "$cache_type" "$cache_key" || true
        fi
        return 0
    fi

    # On NFS host, local and NFS are the same - if local miss, it's a miss
    if [[ "$is_nfs_host" == "true" ]]; then
        _log "NFS host cache miss: $NFS_CACHE_DIR"
        return 1
    fi

    # 2. Not in local cache - check NFS (only for NFS clients)
    if ! _nfs_available; then
        _log "NFS not available, cache miss"
        return 1
    fi

    # Check for tar archive first (preferred format), then directory (legacy)
    local NFS_TAR_FILE="${NFS_CACHE_DIR}.tar"
    local use_tar=false

    if [[ -f "$NFS_TAR_FILE" ]]; then
        use_tar=true
        _log "NFS cache hit (tar archive): $NFS_TAR_FILE"
    elif [[ -d "$NFS_CACHE_DIR" ]]; then
        _log "NFS cache hit (directory): $NFS_CACHE_DIR"
    else
        _log "NFS cache miss: $NFS_CACHE_DIR (no tar or dir)"
        return 1
    fi

    # 3. Populate local cache from NFS (with exclusive lock to prevent races)
    mkdir -p "$(dirname "$LOCAL_CACHE_DIR")"
    mkdir -p "$(dirname "$LOCAL_LOCK_FILE")"
    local local_entry_lock="${LOCAL_CACHE_DIR}.lock"
    touch "$local_entry_lock" 2>/dev/null || true

    if ! _flock_with_timeout "$CACHE_LOCK_TIMEOUT" -x "$local_entry_lock" -c "
        # Double-check after acquiring lock (another job may have populated)
        if [[ -d '$LOCAL_CACHE_DIR' && ! -L '$LOCAL_CACHE_DIR' ]]; then
            echo '[cache-manager] Local cache populated while waiting for lock' >&2
            exit 0
        fi

        # Remove any stale symlinks
        [[ -L '$LOCAL_CACHE_DIR' ]] && rm -f '$LOCAL_CACHE_DIR'

        # Extract from NFS to local cache
        mkdir -p '$LOCAL_CACHE_DIR'
        if [[ '$use_tar' == 'true' ]]; then
            echo '[cache-manager] Extracting NFS tar to local cache: $LOCAL_CACHE_DIR' >&2
            tar xf '$NFS_TAR_FILE' -C '$LOCAL_CACHE_DIR'
        else
            echo '[cache-manager] Copying NFS directory to local cache: $LOCAL_CACHE_DIR' >&2
            (cd '$NFS_CACHE_DIR' && tar cf - .) | (cd '$LOCAL_CACHE_DIR' && tar xf -)
        fi
    "; then
        _log "Populated local cache from NFS"
    else
        _error "Failed to populate local cache from NFS"
        return 1
    fi

    # 4. Copy from local cache to destination
    if [[ "$LOCAL_CACHE_DIR" != "$local_dest" ]]; then
        mkdir -p "$local_dest"
        cp -r "$LOCAL_CACHE_DIR"/. "$local_dest"/
        _log "Copied to destination: $local_dest"
    fi

    # Restore pgdata permissions for HAF caches
    if [[ "$cache_type" == "haf" || "$cache_type" == "haf_sync" ]]; then
        _restore_pgdata_permissions "$local_dest"
    fi

    # Update LRU indices
    _update_local_lru "$cache_type" "$cache_key" || true
    _update_lru "$cache_type" "$cache_key" || true

    return 0
}

# PUT: Store cache in local cache, then push to NFS as tar archive
# Two-tier caching: local cache has real data, NFS is backing store (tar format)
cmd_put() {
    local cache_type="$1"
    local cache_key="$2"
    local local_source="$3"

    if [[ ! -d "$local_source" ]]; then
        _error "Source directory does not exist: $local_source"
        return 1
    fi

    # Relax pgdata permissions for HAF caches so they can be copied
    if [[ "$cache_type" == "haf" || "$cache_type" == "haf_sync" ]]; then
        _relax_pgdata_permissions "$local_source"
    fi

    _get_paths "$cache_type" "$cache_key"

    local is_nfs_host=false
    _is_nfs_host && is_nfs_host=true

    # On NFS host, LOCAL_CACHE_DIR == NFS_CACHE_DIR, so one copy does both
    if [[ "$is_nfs_host" == "true" ]]; then
        # Check if already exists
        if [[ -d "$NFS_CACHE_DIR" && -f "$METADATA_FILE" ]]; then
            _log "Cache already exists on NFS host, updating timestamp"
            _update_lru "$cache_type" "$cache_key"
            return 0
        fi

        # Copy directly to NFS path (which is local storage on this host)
        if [[ "$local_source" != "$NFS_CACHE_DIR" ]]; then
            _log "Storing cache on NFS host: $NFS_CACHE_DIR"
            mkdir -p "$NFS_CACHE_DIR"
            touch "$LOCK_FILE"
            _flock_with_timeout "$CACHE_LOCK_TIMEOUT" -x "$LOCK_FILE" -c "
                (cd '$local_source' && tar cf - .) | (cd '$NFS_CACHE_DIR' && tar xf -)
            " || { _error "Failed to store cache"; return 1; }
        else
            _log "Source is already at NFS path, no copy needed"
            mkdir -p "$(dirname "$METADATA_FILE")"
        fi

        _write_metadata "$cache_type" "$cache_key" "$NFS_CACHE_DIR"
        _update_lru "$cache_type" "$cache_key"
        _log "Cache stored successfully on NFS host"
        _maybe_cleanup &
        return 0
    fi

    # NFS client path: copy to local cache first, then push to NFS

    # 1. Copy to local cache (if source is different from local cache path)
    if [[ "$local_source" != "$LOCAL_CACHE_DIR" ]]; then
        # Check if local cache already has this entry
        if [[ -d "$LOCAL_CACHE_DIR" && ! -L "$LOCAL_CACHE_DIR" ]]; then
            _log "Local cache already exists: $LOCAL_CACHE_DIR"
        else
            _log "Copying to local cache: $LOCAL_CACHE_DIR"
            mkdir -p "$(dirname "$LOCAL_CACHE_DIR")"
            local local_entry_lock="${LOCAL_CACHE_DIR}.lock"
            touch "$local_entry_lock" 2>/dev/null || true

            if ! _flock_with_timeout "$CACHE_LOCK_TIMEOUT" -x "$local_entry_lock" -c "
                # Remove any stale symlinks
                [[ -L '$LOCAL_CACHE_DIR' ]] && rm -f '$LOCAL_CACHE_DIR'

                if [[ -d '$LOCAL_CACHE_DIR' ]]; then
                    echo '[cache-manager] Local cache already exists' >&2
                    exit 0
                fi

                mkdir -p '$LOCAL_CACHE_DIR'
                cp -r '$local_source'/. '$LOCAL_CACHE_DIR'/
            "; then
                _error "Failed to write to local cache"
                # Continue to try NFS anyway
            fi
        fi
        _update_local_lru "$cache_type" "$cache_key" || true
    fi

    # 2. Push to NFS (if available)
    if ! _nfs_available; then
        _log "NFS not available, cached locally only"
        return 0
    fi

    # Check if already exists on NFS (either as directory or tar archive)
    local NFS_TAR_FILE="${NFS_CACHE_DIR}.tar"
    if [[ -f "$NFS_TAR_FILE" ]] || { [[ -d "$NFS_CACHE_DIR" ]] && [[ -f "$METADATA_FILE" ]]; }; then
        _log "Cache already exists on NFS, updating timestamp"
        _update_lru "$cache_type" "$cache_key"
        return 0
    fi

    # Copy to NFS as single tar archive for 3x faster writes
    # Benchmark: cp -a 19GB/1844 files = 74s, tar archive = 25s
    mkdir -p "$(dirname "$NFS_TAR_FILE")"
    local NFS_TAR_LOCK="${NFS_TAR_FILE}.lock"
    touch "$NFS_TAR_LOCK"

    # Build exclusions for caches to reduce size and speed up NFS writes
    local tar_excludes=""
    if [[ "$cache_type" == "hive" ]]; then
        # Exclude blockchain - CI runners mount /blockchain locally via services_volumes
        if [[ -d "${local_source}/datadir/blockchain" ]]; then
            tar_excludes="--exclude=./datadir/blockchain"
            _log "Excluding datadir/blockchain (services use local /blockchain/block_log_5m)"
        fi
    elif [[ "$cache_type" == "haf" || "$cache_type" == "haf_sync" ]]; then
        tar_excludes=$(_build_haf_tar_excludes "$local_source")
    fi

    # Write exclusions to temp file for use in subshell
    local excludes_file=""
    if [[ -n "$tar_excludes" ]]; then
        excludes_file=$(mktemp)
        echo "$tar_excludes" > "$excludes_file"
    fi

    if ! _flock_with_timeout "$CACHE_LOCK_TIMEOUT" -x "$NFS_TAR_LOCK" -c "
        # Double-check after acquiring lock
        if [[ -f '$NFS_TAR_FILE' ]]; then
            echo '[cache-manager] Cache was created while waiting for lock' >&2
            exit 0
        fi

        echo '[cache-manager] Creating tar archive on NFS: $NFS_TAR_FILE' >&2
        # Read exclusions from temp file if present
        excludes=''
        if [[ -f '$excludes_file' ]]; then
            excludes=\$(cat '$excludes_file')
        fi
        # Write tar archive directly to NFS (single file = fast)
        # shellcheck disable=SC2086
        tar cf '$NFS_TAR_FILE.tmp' \$excludes -C '$local_source' .
        mv '$NFS_TAR_FILE.tmp' '$NFS_TAR_FILE'
    "; then
        [[ -n "$excludes_file" ]] && rm -f "$excludes_file"
        _error "Failed to acquire exclusive lock"
        return 1
    fi
    [[ -n "$excludes_file" ]] && rm -f "$excludes_file"

    # Write metadata next to tar file
    local TAR_METADATA="${NFS_TAR_FILE%.tar}/.metadata"
    mkdir -p "$(dirname "$TAR_METADATA")"
    _write_metadata "$cache_type" "$cache_key" "$local_source"
    mv "$METADATA_FILE" "$TAR_METADATA" 2>/dev/null || true

    _update_lru "$cache_type" "$cache_key"
    _log "Cache stored successfully (tar archive)"

    # Trigger async cleanup check
    _maybe_cleanup &
    _maybe_cleanup_local &

    return 0
}

# CLEANUP: Remove old caches using LRU eviction
cmd_cleanup() {
    local cache_type="${1:-}"
    local max_size_gb="$CACHE_MAX_SIZE_GB"
    local max_age_days="$CACHE_MAX_AGE_DAYS"

    # Parse options
    shift || true
    while [[ $# -gt 0 ]]; do
        case $1 in
            --max-size-gb)
                max_size_gb="$2"
                shift 2
                ;;
            --max-age-days)
                max_age_days="$2"
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done

    if ! _nfs_available; then
        _error "NFS not available for cleanup"
        return 1
    fi

    _log "Starting cleanup (max_size=${max_size_gb}GB, max_age=${max_age_days}days)"

    local max_size_bytes=$((max_size_gb * 1024 * 1024 * 1024))
    local cutoff_timestamp=$(($(date +%s) - max_age_days * 86400))

    local lru_index="${CACHE_NFS_PATH}/.lru_index"

    # Calculate current total size
    local search_path="$CACHE_NFS_PATH"
    [[ -n "$cache_type" ]] && search_path="$CACHE_NFS_PATH/$cache_type"

    local total_size=$(du -sb "$search_path" 2>/dev/null | cut -f1 || echo 0)
    _log "Current cache size: $((total_size / 1024 / 1024 / 1024))GB"

    if [[ ! -f "$lru_index" ]]; then
        _log "No LRU index found, nothing to clean"
        return 0
    fi

    # Sort by timestamp (oldest first) and process
    local removed=0
    while IFS='|' read -r timestamp entry; do
        # Skip if filtering by type and doesn't match
        if [[ -n "$cache_type" && ! "$entry" =~ ^${cache_type}/ ]]; then
            continue
        fi

        local entry_path="$CACHE_NFS_PATH/$entry"

        # Skip if doesn't exist
        [[ -d "$entry_path" ]] || continue

        # Check if should remove (age or size)
        local should_remove=false

        if [[ $timestamp -lt $cutoff_timestamp ]]; then
            _log "Entry $entry is older than ${max_age_days} days"
            should_remove=true
        elif [[ $total_size -gt $max_size_bytes ]]; then
            _log "Total size exceeds limit, removing oldest: $entry"
            should_remove=true
        fi

        if [[ "$should_remove" == "true" ]]; then
            # Check if locked (skip if in use)
            local lock_file="$entry_path/.lock"
            if [[ -f "$lock_file" ]] && ! flock -n "$lock_file" -c "true" 2>/dev/null; then
                _log "Skipping $entry - currently locked"
                continue
            fi

            local entry_size=$(du -sb "$entry_path" 2>/dev/null | cut -f1 || echo 0)
            _log "Removing: $entry (${entry_size} bytes)"
            rm -rf "$entry_path"
            total_size=$((total_size - entry_size))
            removed=$((removed + 1))

            # Remove from LRU index
            grep -v "|${entry}$" "$lru_index" > "${lru_index}.tmp" 2>/dev/null || true
            mv "${lru_index}.tmp" "$lru_index"
        fi

        # Stop if under limit
        [[ $total_size -le $max_size_bytes ]] && break

    done < <(sort -t'|' -k1 -n "$lru_index")

    _log "Cleanup complete, removed $removed entries"
}

# Maybe trigger cleanup if size is getting large
_maybe_cleanup() {
    if ! _nfs_available; then
        return 0
    fi

    local total_size
    total_size=$(du -sb "$CACHE_NFS_PATH" 2>/dev/null | awk '{print $1}') || total_size=0
    [[ -z "$total_size" ]] && total_size=0
    local max_bytes=$((CACHE_MAX_SIZE_GB * 1024 * 1024 * 1024))
    local threshold=$((max_bytes * 90 / 100))  # 90% threshold

    if [[ "$total_size" =~ ^[0-9]+$ ]] && [[ $total_size -gt $threshold ]]; then
        _log "Cache size at 90% capacity, triggering cleanup"
        cmd_cleanup "" --max-size-gb "$CACHE_MAX_SIZE_GB" --max-age-days "$CACHE_MAX_AGE_DAYS"
    fi
}

# CLEANUP_LOCAL: Remove old entries from local cache using LRU eviction
cmd_cleanup_local() {
    local cache_type="${1:-}"
    local max_size_gb="$CACHE_LOCAL_MAX_SIZE_GB"
    local max_age_days="$CACHE_LOCAL_MAX_AGE_DAYS"

    # Parse options
    shift || true
    while [[ $# -gt 0 ]]; do
        case $1 in
            --max-size-gb)
                max_size_gb="$2"
                shift 2
                ;;
            --max-age-days)
                max_age_days="$2"
                shift 2
                ;;
            *)
                shift
                ;;
        esac
    done

    # Skip on NFS host (local cache IS NFS cache)
    if _is_nfs_host; then
        _log "On NFS host, local cleanup skipped (use NFS cleanup)"
        return 0
    fi

    if [[ ! -d "$CACHE_LOCAL_PATH" ]]; then
        _log "Local cache path does not exist: $CACHE_LOCAL_PATH"
        return 0
    fi

    _log "Starting local cleanup (max_size=${max_size_gb}GB, max_age=${max_age_days}days)"

    local max_size_bytes=$((max_size_gb * 1024 * 1024 * 1024))
    local cutoff_timestamp=$(($(date +%s) - max_age_days * 86400))

    local lru_index="${CACHE_LOCAL_PATH}/.lru_index"

    # Calculate current total size
    local search_path="$CACHE_LOCAL_PATH"
    [[ -n "$cache_type" ]] && search_path="$CACHE_LOCAL_PATH/$cache_type"

    local total_size
    total_size=$(du -sb "$search_path" 2>/dev/null | cut -f1 || echo 0)
    _log "Current local cache size: $((total_size / 1024 / 1024 / 1024))GB"

    if [[ ! -f "$lru_index" ]]; then
        _log "No local LRU index found, nothing to clean"
        return 0
    fi

    # Sort by timestamp (oldest first) and process
    local removed=0
    while IFS='|' read -r timestamp entry; do
        # Skip if filtering by type and doesn't match
        if [[ -n "$cache_type" && ! "$entry" =~ ^${cache_type}/ ]]; then
            continue
        fi

        local entry_path="$CACHE_LOCAL_PATH/$entry"

        # Skip if doesn't exist or is a symlink
        [[ -d "$entry_path" && ! -L "$entry_path" ]] || continue

        # Check if should remove (age or size)
        local should_remove=false

        if [[ $timestamp -lt $cutoff_timestamp ]]; then
            _log "Local entry $entry is older than ${max_age_days} days"
            should_remove=true
        elif [[ $total_size -gt $max_size_bytes ]]; then
            _log "Local cache exceeds limit, removing oldest: $entry"
            should_remove=true
        fi

        if [[ "$should_remove" == "true" ]]; then
            # Check if locked (skip if in use)
            local lock_file="$entry_path/.lock"
            if [[ -f "$lock_file" ]] && ! flock -n "$lock_file" -c "true" 2>/dev/null; then
                _log "Skipping local $entry - currently locked"
                continue
            fi

            local entry_size
            entry_size=$(du -sb "$entry_path" 2>/dev/null | cut -f1 || echo 0)
            _log "Removing local: $entry (${entry_size} bytes)"
            rm -rf "$entry_path"
            total_size=$((total_size - entry_size))
            removed=$((removed + 1))

            # Remove from local LRU index
            local lru_lock="${CACHE_LOCAL_PATH}/.lru_lock"
            touch "$lru_lock"
            _flock_with_timeout 30 -x "$lru_lock" -c "
                grep -v '|${entry}\$' '$lru_index' > '${lru_index}.tmp' 2>/dev/null || true
                mv '${lru_index}.tmp' '$lru_index'
            " || true
        fi

        # Stop if under limit
        [[ $total_size -le $max_size_bytes ]] && break

    done < <(sort -t'|' -k1 -n "$lru_index")

    _log "Local cleanup complete, removed $removed entries"
}

# Maybe trigger local cleanup if size is getting large
_maybe_cleanup_local() {
    # Skip on NFS host
    if _is_nfs_host; then
        return 0
    fi

    if [[ ! -d "$CACHE_LOCAL_PATH" ]]; then
        return 0
    fi

    local total_size
    total_size=$(du -sb "$CACHE_LOCAL_PATH" 2>/dev/null | awk '{print $1}') || total_size=0
    [[ -z "$total_size" ]] && total_size=0
    local max_bytes=$((CACHE_LOCAL_MAX_SIZE_GB * 1024 * 1024 * 1024))
    local threshold=$((max_bytes * 90 / 100))  # 90% threshold

    if [[ "$total_size" =~ ^[0-9]+$ ]] && [[ $total_size -gt $threshold ]]; then
        _log "Local cache at 90% capacity, triggering cleanup"
        cmd_cleanup_local "" --max-size-gb "$CACHE_LOCAL_MAX_SIZE_GB" --max-age-days "$CACHE_LOCAL_MAX_AGE_DAYS"
    fi
}

# LIST: Show caches of a given type
cmd_list() {
    local cache_type="${1:-}"

    echo "=== Local Caches (${CACHE_LOCAL_PATH}) ==="
    local pattern="${CACHE_LOCAL_PATH}/${cache_type}*"
    for dir in $pattern; do
        [[ -d "$dir" ]] || continue
        local size=$(du -sh "$dir" 2>/dev/null | cut -f1 || echo "?")
        local mtime=$(stat -c %y "$dir" 2>/dev/null | cut -d. -f1 || echo "?")
        echo "  $(basename "$dir") - ${size} - ${mtime}"
    done

    if _nfs_available; then
        echo ""
        echo "=== NFS Caches (${CACHE_NFS_PATH}) ==="
        local nfs_path="$CACHE_NFS_PATH"
        [[ -n "$cache_type" ]] && nfs_path="$CACHE_NFS_PATH/$cache_type"

        if [[ -d "$nfs_path" ]]; then
            for dir in "$nfs_path"/*/; do
                [[ -d "$dir" ]] || continue
                local size=$(du -sh "$dir" 2>/dev/null | cut -f1 || echo "?")
                local key=$(basename "$dir")
                local meta=""
                if [[ -f "$dir/.metadata" ]]; then
                    meta=$(jq -r '.created_at // "?"' "$dir/.metadata" 2>/dev/null || echo "?")
                fi
                echo "  $key - ${size} - ${meta}"
            done
        fi
    else
        echo ""
        echo "=== NFS not available ==="
    fi
}

# IS-FAST-BUILDER: Check if running on a fast builder (5950 CPU)
cmd_is_fast_builder() {
    # Check CPU model
    local cpu_model=$(cat /proc/cpuinfo 2>/dev/null | grep "model name" | head -1 || echo "")

    if echo "$cpu_model" | grep -qE "5950|5900|EPYC"; then
        _log "Fast builder detected: $cpu_model"
        return 0
    fi

    # Fallback: check hostname
    local hostname=$(hostname)
    case "$hostname" in
        hive-builder-8|hive-builder-9|hive-builder-10)
            _log "Fast builder (by hostname): $hostname"
            return 0
            ;;
    esac

    _log "Not a fast builder: ${cpu_model:-unknown CPU}"
    return 1
}

# STATUS: Show overall cache status
cmd_status() {
    echo "Cache Manager Status"
    echo "===================="
    echo "NFS Path:       $CACHE_NFS_PATH"
    echo "Local Path:     $CACHE_LOCAL_PATH"
    echo "NFS Max Size:   ${CACHE_MAX_SIZE_GB}GB"
    echo "NFS Max Age:    ${CACHE_MAX_AGE_DAYS} days"
    echo "Local Max Size: ${CACHE_LOCAL_MAX_SIZE_GB}GB"
    echo "Local Max Age:  ${CACHE_LOCAL_MAX_AGE_DAYS} days"
    echo "NFS Host:       $(_is_nfs_host && echo "YES (local storage)" || echo "NO (NFS client)")"
    echo ""

    local lru_index="${CACHE_NFS_PATH}/.lru_index"
    local local_lru_index="${CACHE_LOCAL_PATH}/.lru_index"

    if _nfs_available; then
        echo "NFS Status:     AVAILABLE"
        local total
        total=$(du -sh "$CACHE_NFS_PATH" 2>/dev/null | cut -f1 || echo "?")
        echo "NFS Usage:      $total"

        if [[ -f "$lru_index" ]]; then
            local count
            count=$(wc -l < "$lru_index")
            echo "NFS Entries:    $count"
        fi
    else
        echo "NFS Status:     NOT AVAILABLE"
    fi

    echo ""
    echo "Local Cache:"
    if [[ -d "$CACHE_LOCAL_PATH" ]]; then
        local local_total
        local_total=$(du -sh "$CACHE_LOCAL_PATH" 2>/dev/null | cut -f1 || echo "?")
        echo "  Usage:        $local_total"
        if [[ -f "$local_lru_index" ]]; then
            local local_count
            local_count=$(wc -l < "$local_lru_index")
            echo "  LRU Entries:  $local_count"
        fi
        echo "  Directories:"
        du -sh "${CACHE_LOCAL_PATH}"/*/ 2>/dev/null | head -10 || echo "    (empty)"
    else
        echo "  (not initialized)"
    fi
}

# Main
usage() {
    head -20 "$0" | tail -18 | sed 's/^# //'
    exit 1
}

[[ $# -lt 1 ]] && usage

cmd="$1"
shift

case "$cmd" in
    get)
        [[ $# -lt 3 ]] && { _error "get requires: <cache-type> <cache-key> <local-dest>"; exit 1; }
        cmd_get "$@"
        ;;
    put)
        [[ $# -lt 3 ]] && { _error "put requires: <cache-type> <cache-key> <local-source>"; exit 1; }
        cmd_put "$@"
        ;;
    cleanup)
        cmd_cleanup "$@"
        ;;
    cleanup-local)
        cmd_cleanup_local "$@"
        ;;
    list)
        cmd_list "$@"
        ;;
    status)
        cmd_status
        ;;
    is-fast-builder)
        cmd_is_fast_builder
        ;;
    *)
        _error "Unknown command: $cmd"
        usage
        ;;
esac
