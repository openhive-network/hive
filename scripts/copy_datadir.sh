#! /bin/bash

set -xeuo pipefail

# Default shared block_log location (used when blockchain not in cache)
# Try NFS cache first, fall back to local cache
SHARED_BLOCK_LOG_DIR="${SHARED_BLOCK_LOG_DIR:-/nfs/ci-cache/hive/block_log_5m}"

# NFS cache configuration
CACHE_NFS_PATH="${CACHE_NFS_PATH:-/nfs/ci-cache}"

# Fix pg_tblspc symlinks to point to the correct tablespace location
# PostgreSQL stores tablespace symlinks with absolute paths, which break when data is copied
fix_pg_tblspc_symlinks() {
    local datadir="$1"
    local pg_tblspc="${datadir}/haf_db_store/pgdata/pg_tblspc"
    local tablespace="${datadir}/haf_db_store/tablespace"

    if [[ ! -d "$pg_tblspc" ]]; then
        return 0
    fi

    for link in "$pg_tblspc"/*; do
        if [[ -L "$link" ]]; then
            local target
            target=$(readlink "$link")
            # Fix if symlink contains 'tablespace' (relative or wrong absolute path)
            if [[ "$target" == *"tablespace"* ]] && [[ -d "$tablespace" ]]; then
                echo "Fixing pg_tblspc symlink: $(basename "$link") -> $tablespace"
                sudo rm -f "$link"
                sudo ln -s "$tablespace" "$link"
            fi
        fi
    done
}

# Function to extract NFS cache if local DATA_SOURCE doesn't exist
# Derives cache type and key from DATA_SOURCE path pattern: /cache/{type}_{key}
extract_nfs_cache_if_needed() {
    local data_source="$1"

    # Skip if already exists
    if [[ -d "${data_source}/datadir" ]]; then
        echo "Local cache exists at ${data_source}/datadir"
        return 0
    fi

    # Parse DATA_SOURCE to derive cache type and key
    # Pattern: /cache/{type}_{key} -> NFS tar: /nfs/ci-cache/{type}/{key}.tar
    local basename
    basename=$(basename "$data_source")

    # Split by first underscore: haf_pipeline_12345_filtered -> type=haf_pipeline, key=12345_filtered
    local cache_type cache_key
    if [[ "$basename" =~ ^([^_]+_[^_]+)_(.+)$ ]]; then
        cache_type="${BASH_REMATCH[1]}"
        cache_key="${BASH_REMATCH[2]}"
    elif [[ "$basename" =~ ^([^_]+)_(.+)$ ]]; then
        cache_type="${BASH_REMATCH[1]}"
        cache_key="${BASH_REMATCH[2]}"
    else
        echo "Cannot parse DATA_SOURCE path for NFS fallback: $data_source"
        return 1
    fi

    local nfs_tar="${CACHE_NFS_PATH}/${cache_type}/${cache_key}.tar"
    echo "Checking for NFS cache: $nfs_tar"

    if [[ -f "$nfs_tar" ]]; then
        echo "Found NFS cache, extracting to $data_source"
        mkdir -p "$data_source"
        chmod 777 "$data_source" 2>/dev/null || true

        if tar xf "$nfs_tar" -C "$data_source"; then
            echo "NFS cache extracted successfully"

            # Restore pgdata permissions for PostgreSQL
            local pgdata="${data_source}/datadir/haf_db_store/pgdata"
            local tablespace="${data_source}/datadir/haf_db_store/tablespace"
            if [[ -d "$pgdata" ]]; then
                chmod 700 "$pgdata" 2>/dev/null || true
                chown -R 105:105 "$pgdata" 2>/dev/null || true
            fi
            if [[ -d "$tablespace" ]]; then
                chmod 700 "$tablespace" 2>/dev/null || true
                chown -R 105:105 "$tablespace" 2>/dev/null || true
            fi
            # Fix pg_tblspc symlinks after extraction
            fix_pg_tblspc_symlinks "${data_source}/datadir"
            return 0
        else
            echo "ERROR: Failed to extract NFS cache"
            return 1
        fi
    else
        echo "NFS cache not found at $nfs_tar"
        return 1
    fi
}

if [ -n "${DATA_SOURCE+x}" ]
then
    echo "DATA_SOURCE: ${DATA_SOURCE}"
    echo "DATADIR: ${DATADIR}"

    # Try NFS fallback if local DATA_SOURCE doesn't exist
    if [[ ! -d "${DATA_SOURCE}/datadir" ]]; then
        echo "Local DATA_SOURCE not found, attempting NFS fallback..."
        extract_nfs_cache_if_needed "${DATA_SOURCE}" || true
    fi

    if [ "$(realpath "${DATA_SOURCE}/datadir")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        # Use cp without -p to avoid "Operation not supported" errors when copying from NFS
        flock "${DATA_SOURCE}/datadir" sudo -En cp -r --no-preserve=mode,ownership "${DATA_SOURCE}/datadir"/*  "${DATADIR}"

        # Fix pg_tblspc symlinks after copying to DATADIR
        fix_pg_tblspc_symlinks "${DATADIR}"

        # Handle blockchain directory - may be excluded from cache for efficiency
        # Check if directory exists AND has content (empty dirs can be created by Docker bind mounts)
        if [[ -d "${DATA_SOURCE}/datadir/blockchain" ]] && ls "${DATA_SOURCE}/datadir/blockchain"/block_log* 1>/dev/null 2>&1; then
            sudo chmod -R a+w "${DATA_SOURCE}/datadir/blockchain"
            ls -al "${DATA_SOURCE}/datadir/blockchain"
        elif [[ -d "${SHARED_BLOCK_LOG_DIR}" ]]; then
            # Remove empty blockchain dir if it exists (leftover from Docker bind mounts)
            if [[ -d "${DATADIR}/blockchain" ]] && [[ -z "$(ls -A "${DATADIR}/blockchain")" ]]; then
                rmdir "${DATADIR}/blockchain"
            fi
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
