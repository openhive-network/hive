#! /bin/bash

set -xeuo pipefail

# Default shared block_log location (used when blockchain not in cache)
# Try NFS cache first, fall back to local cache
SHARED_BLOCK_LOG_DIR="${SHARED_BLOCK_LOG_DIR:-/nfs/ci-cache/hive/block_log_5m}"

# Validate that a cache directory contains essential PostgreSQL data
# Returns 0 if valid, 1 if incomplete or missing
is_cache_valid() {
    local source="$1"
    # Check for datadir and PostgreSQL data directory
    # haf_db_store/pgdata must exist and contain pg_version (indicates complete PG data)
    if [[ -d "${source}/datadir" && -d "${source}/datadir/haf_db_store/pgdata" && -f "${source}/datadir/haf_db_store/pgdata/PG_VERSION" ]]; then
        return 0
    fi
    return 1
}

# Wait for local cache with NFS fallback for cross-builder scenarios
# When DATA_SOURCE points to a local cache path that doesn't exist yet,
# wait briefly for another job's before_script to populate it, then fall back to NFS tar
#
# NFS path structure (via cache-manager): /nfs/ci-cache/haf_pipeline/${PIPELINE_ID}.tar
# Local path structure: /cache/haf_pipeline_${PIPELINE_ID}
resolve_data_source() {
    local source="$1"
    local max_wait="${LOCAL_CACHE_WAIT_SECONDS:-30}"
    local wait_interval=5

    # If source directory exists with valid PostgreSQL data, use it directly
    if is_cache_valid "$source"; then
        echo "$source"
        return 0
    fi

    # For local cache paths, wait briefly for another job to populate it
    # This allows jobs running in parallel on the same builder to share cache
    # Supports cache-manager format: /cache/{cache_type}_{cache_key}
    # Examples: /cache/haf_pipeline_142820, /cache/haf_sync_abc123, /cache/haf_pipeline_142820_filtered
    if [[ "$source" =~ ^/cache/([a-z_]+)_(.+)$ ]]; then
        local cache_type="${BASH_REMATCH[1]}"
        local cache_key="${BASH_REMATCH[2]}"
        # NFS tar path follows cache-manager structure: /nfs/ci-cache/{cache_type}/{key}.tar
        local nfs_tar="/nfs/ci-cache/${cache_type}/${cache_key}.tar"
        local waited=0

        if [[ -d "${source}/datadir" ]]; then
            echo "Local cache at ${source} exists but is incomplete, waiting up to ${max_wait}s for it to be ready..." >&2
        else
            echo "Local cache not found at ${source}, waiting up to ${max_wait}s for another job to populate it..." >&2
        fi

        while [[ $waited -lt $max_wait ]]; do
            sleep "$wait_interval"
            waited=$((waited + wait_interval))

            if is_cache_valid "$source"; then
                echo "Local cache ready after ${waited}s, using: ${source}" >&2
                echo "$source"
                return 0
            fi
            echo "Still waiting for valid local cache... (${waited}s/${max_wait}s)" >&2
        done

        # Fall back to NFS tar file - extract to local cache
        if [[ -f "${nfs_tar}" ]]; then
            echo "FALLBACK: Extracting NFS tar ${nfs_tar} to ${source}..." >&2
            sudo mkdir -p "${source}"
            sudo tar xf "${nfs_tar}" -C "${source}"
            echo "$source"
            return 0
        fi
    fi

    # Check if source is already an NFS path (directory or tar)
    if [[ "$source" =~ ^/nfs/ ]]; then
        # Check for tar file first
        if [[ -f "${source}.tar" ]]; then
            local extract_dir="${source}"
            echo "Extracting NFS tar ${source}.tar to ${extract_dir}..." >&2
            sudo mkdir -p "${extract_dir}"
            sudo tar xf "${source}.tar" -C "${extract_dir}"
            echo "$extract_dir"
            return 0
        fi
        # Otherwise use directory directly
        echo "$source"
        return 0
    fi

    # No fallback available, return original (will fail later with clear error)
    echo "$source"
    return 0
}

if [ -n "${DATA_SOURCE+x}" ]
then
    # Resolve DATA_SOURCE with NFS fallback if needed
    DATA_SOURCE=$(resolve_data_source "$DATA_SOURCE")

    echo "DATA_SOURCE: ${DATA_SOURCE}"
    echo "DATADIR: ${DATADIR}"
    if [ "$(realpath "${DATA_SOURCE}/datadir")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        # Use cp without -p to avoid "Operation not supported" errors when copying from NFS
        flock "${DATA_SOURCE}/datadir" sudo -En cp -r --no-preserve=mode,ownership "${DATA_SOURCE}/datadir"/*  "${DATADIR}"

        # Handle blockchain directory - may be excluded from cache for efficiency
        if [[ -d "${DATA_SOURCE}/datadir/blockchain" ]]; then
            sudo chmod -R a+w "${DATA_SOURCE}/datadir/blockchain"
            ls -al "${DATA_SOURCE}/datadir/blockchain"
        elif [[ -d "${SHARED_BLOCK_LOG_DIR}" ]]; then
            # Blockchain not in cache - create symlinks to shared block_log
            echo "Blockchain not in cache, linking to shared block_log at ${SHARED_BLOCK_LOG_DIR}"
            sudo -Enu hived mkdir -p "${DATADIR}/blockchain"
            for block_file in "${SHARED_BLOCK_LOG_DIR}"/block_log* ; do
                if [[ -f "$block_file" ]]; then
                    local_name=$(basename "$block_file")
                    sudo -Enu hived ln -sf "$block_file" "${DATADIR}/blockchain/${local_name}"
                    echo "Linked: ${local_name}"
                fi
            done
            ls -al "${DATADIR}/blockchain"
        else
            echo "WARNING: No blockchain in cache and shared block_log not found at ${SHARED_BLOCK_LOG_DIR}"
        fi

        if [[ -e "${DATA_SOURCE}/shm_dir" && "$(realpath "${DATA_SOURCE}/shm_dir")" != "$(realpath "${SHM_DIR}")" ]]
        then
            echo "Creating copy of ${DATA_SOURCE}/shm_dir inside ${SHM_DIR}"
            sudo -Enu hived mkdir -p "${SHM_DIR}"
            # Use cp without -p to avoid "Operation not supported" errors when copying from NFS
            flock "${DATA_SOURCE}/datadir" sudo -En cp -r --no-preserve=mode,ownership "${DATA_SOURCE}/shm_dir"/* "${SHM_DIR}"
            sudo chmod -R a+w "${SHM_DIR}"
            ls -al "${SHM_DIR}"
        else
            echo "Skipping shm_dir processing."
        fi
        ls -al "${DATA_SOURCE}/datadir"
    fi
fi
