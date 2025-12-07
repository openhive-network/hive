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
#   cache-manager.sh list <cache-type>
#   cache-manager.sh status
#   cache-manager.sh is-fast-builder    # Check if current host is a fast builder
#
# CI Tag Requirements:
#   Replay/build jobs should use: tags: [data-cache-storage, fast]
#   Fast builders (AMD 5950): hive-builder-8, hive-builder-9, hive-builder-10
#
# Cache types: haf, balance_tracker, hivemind, etc.
#
# Environment variables:
#   CACHE_NFS_PATH      - NFS mount point (default: /nfs/ci-cache)
#   CACHE_LOCAL_PATH    - Local cache directory (default: /cache)
#   CACHE_MAX_SIZE_GB   - Max total NFS cache size (default: 2000)
#   CACHE_MAX_AGE_DAYS  - Max cache age (default: 30)
#   CACHE_LOCK_TIMEOUT  - Lock timeout in seconds (default: 3600)
#   CACHE_QUIET         - Suppress verbose output (default: false)

set -euo pipefail

# Configuration with defaults
CACHE_NFS_PATH="${CACHE_NFS_PATH:-/nfs/ci-cache}"
CACHE_LOCAL_PATH="${CACHE_LOCAL_PATH:-/cache}"
CACHE_MAX_SIZE_GB="${CACHE_MAX_SIZE_GB:-2000}"
CACHE_MAX_AGE_DAYS="${CACHE_MAX_AGE_DAYS:-30}"
CACHE_LOCK_TIMEOUT="${CACHE_LOCK_TIMEOUT:-3600}"
CACHE_QUIET="${CACHE_QUIET:-false}"

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
        LOCAL_CACHE_DIR="${CACHE_LOCAL_PATH}/${cache_type}_${cache_key}"
    fi

    LOCK_FILE="${NFS_CACHE_DIR}/.lock"
    METADATA_FILE="${NFS_CACHE_DIR}/.metadata"
    LRU_INDEX="${CACHE_NFS_PATH}/.lru_index"
    GLOBAL_LOCK="${CACHE_NFS_PATH}/.global_lock"
}

# Update LRU index with access timestamp
_update_lru() {
    local cache_type="$1"
    local cache_key="$2"
    local timestamp=$(date +%s)
    local entry="${cache_type}/${cache_key}"

    # Acquire global lock for index update
    touch "$GLOBAL_LOCK"
    flock -w 30 "$GLOBAL_LOCK" -c "
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

# GET: Check local, then NFS, copy to local if found on NFS
cmd_get() {
    local cache_type="$1"
    local cache_key="$2"
    local local_dest="$3"

    _get_paths "$cache_type" "$cache_key"

    local is_nfs_host=false
    _is_nfs_host && is_nfs_host=true

    # 1. Check local cache first (on NFS host, this IS the NFS cache)
    if [[ -d "$LOCAL_CACHE_DIR" ]]; then
        _log "Cache hit: $LOCAL_CACHE_DIR"
        if [[ "$LOCAL_CACHE_DIR" != "$local_dest" ]]; then
            _log "Copying to destination: $local_dest"
            mkdir -p "$(dirname "$local_dest")"
            cp -a "$LOCAL_CACHE_DIR" "$local_dest"
        else
            _log "Destination is cache dir, no copy needed"
        fi
        # Update LRU if NFS available
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

    # 2. Check NFS cache (only for NFS clients)
    if ! _nfs_available; then
        _log "NFS not available, cache miss"
        return 1
    fi

    if [[ ! -d "$NFS_CACHE_DIR" ]]; then
        _log "NFS cache miss: $NFS_CACHE_DIR"
        return 1
    fi

    # 3. Copy from NFS to local (with shared lock) - NFS clients only
    _log "NFS cache hit: $NFS_CACHE_DIR"
    mkdir -p "$(dirname "$LOCK_FILE")"
    touch "$LOCK_FILE"

    # Acquire shared lock and copy
    if flock -s -w "$CACHE_LOCK_TIMEOUT" "$LOCK_FILE" -c "
        echo '[cache-manager] Copying from NFS to local: $local_dest' >&2
        mkdir -p '$(dirname "$local_dest")'
        rsync -a '$NFS_CACHE_DIR/' '$local_dest/' 2>/dev/null || cp -a '$NFS_CACHE_DIR' '$local_dest'
    "; then
        # Also cache locally for future use
        if [[ "$LOCAL_CACHE_DIR" != "$local_dest" ]]; then
            mkdir -p "$(dirname "$LOCAL_CACHE_DIR")"
            cp -a "$local_dest" "$LOCAL_CACHE_DIR" 2>/dev/null || true
        fi
    else
        _error "Failed to acquire shared lock"
        return 1
    fi

    _update_lru "$cache_type" "$cache_key"
    return 0
}

# PUT: Copy local cache to NFS
cmd_put() {
    local cache_type="$1"
    local cache_key="$2"
    local local_source="$3"

    if [[ ! -d "$local_source" ]]; then
        _error "Source directory does not exist: $local_source"
        return 1
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
            flock -x -w "$CACHE_LOCK_TIMEOUT" "$LOCK_FILE" -c "
                rsync -a '$local_source/' '$NFS_CACHE_DIR/' 2>/dev/null || cp -a '$local_source'/* '$NFS_CACHE_DIR/'
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

    # NFS client path: store locally AND on NFS

    # Store in local cache
    if [[ "$LOCAL_CACHE_DIR" != "$local_source" ]]; then
        _log "Caching locally: $LOCAL_CACHE_DIR"
        mkdir -p "$(dirname "$LOCAL_CACHE_DIR")"
        cp -a "$local_source" "$LOCAL_CACHE_DIR" 2>/dev/null || true
    fi

    if ! _nfs_available; then
        _log "NFS not available, cached locally only"
        return 0
    fi

    # Check if already exists on NFS
    if [[ -d "$NFS_CACHE_DIR" && -f "$METADATA_FILE" ]]; then
        _log "Cache already exists on NFS, updating timestamp"
        _update_lru "$cache_type" "$cache_key"
        return 0
    fi

    # Copy to NFS with exclusive lock
    mkdir -p "$NFS_CACHE_DIR"
    touch "$LOCK_FILE"

    if ! flock -x -w "$CACHE_LOCK_TIMEOUT" "$LOCK_FILE" -c "
        # Double-check after acquiring lock
        if [[ -f '$METADATA_FILE' ]]; then
            echo '[cache-manager] Cache was created while waiting for lock' >&2
            exit 0
        fi

        echo '[cache-manager] Copying to NFS: $NFS_CACHE_DIR' >&2
        rsync -a '$local_source/' '$NFS_CACHE_DIR/' 2>/dev/null || cp -a '$local_source'/* '$NFS_CACHE_DIR/'
    "; then
        _error "Failed to acquire exclusive lock"
        return 1
    fi

    _write_metadata "$cache_type" "$cache_key" "$NFS_CACHE_DIR"

    _update_lru "$cache_type" "$cache_key"
    _log "Cache stored successfully"

    # Trigger async cleanup check
    _maybe_cleanup &

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

    local total_size=$(du -sb "$CACHE_NFS_PATH" 2>/dev/null | cut -f1 || echo 0)
    local max_bytes=$((CACHE_MAX_SIZE_GB * 1024 * 1024 * 1024))
    local threshold=$((max_bytes * 90 / 100))  # 90% threshold

    if [[ $total_size -gt $threshold ]]; then
        _log "Cache size at 90% capacity, triggering cleanup"
        cmd_cleanup "" --max-size-gb "$CACHE_MAX_SIZE_GB" --max-age-days "$CACHE_MAX_AGE_DAYS"
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
    echo "NFS Path:     $CACHE_NFS_PATH"
    echo "Local Path:   $CACHE_LOCAL_PATH"
    echo "Max Size:     ${CACHE_MAX_SIZE_GB}GB"
    echo "Max Age:      ${CACHE_MAX_AGE_DAYS} days"
    echo "NFS Host:     $(_is_nfs_host && echo "YES (local storage)" || echo "NO (NFS client)")"
    echo ""

    local lru_index="${CACHE_NFS_PATH}/.lru_index"

    if _nfs_available; then
        echo "NFS Status:   AVAILABLE"
        local total=$(du -sh "$CACHE_NFS_PATH" 2>/dev/null | cut -f1 || echo "?")
        echo "NFS Usage:    $total"

        if [[ -f "$lru_index" ]]; then
            local count=$(wc -l < "$lru_index")
            echo "Cache Entries: $count"
        fi
    else
        echo "NFS Status:   NOT AVAILABLE"
    fi

    echo ""
    echo "Local Usage:"
    du -sh "${CACHE_LOCAL_PATH}"/* 2>/dev/null | head -10 || echo "  (empty)"
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
