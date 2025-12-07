#! /bin/bash

set -xeuo pipefail

# Resolve DATA_SOURCE to an existing path
# Checks local cache first, then NFS cache as fallback
resolve_data_source() {
    local original_source="$1"
    local nfs_prefix="${DATA_SOURCE_NFS_PREFIX:-/nfs/ci-cache}"

    # If the original path exists, use it
    if [[ -d "${original_source}/datadir" ]]; then
        echo "$original_source"
        return 0
    fi

    # Try to find equivalent path on NFS
    # Convert /cache/replay_data_TYPE_COMMIT to /nfs/ci-cache/TYPE/COMMIT
    if [[ "$original_source" =~ ^/cache/replay_data_([^_]+)_(.+)$ ]]; then
        local cache_type="${BASH_REMATCH[1]}"
        local cache_key="${BASH_REMATCH[2]}"
        local nfs_path="${nfs_prefix}/${cache_type}/${cache_key}"

        if [[ -d "${nfs_path}/datadir" ]]; then
            echo "Found data on NFS: $nfs_path" >&2
            echo "$nfs_path"
            return 0
        fi

        # Also try with _datadir suffix pattern used by cache-manager
        local alt_nfs_path="${nfs_prefix}/${cache_type}_${cache_key}"
        if [[ -d "${alt_nfs_path}/datadir" ]]; then
            echo "Found data on NFS (alt): $alt_nfs_path" >&2
            echo "$alt_nfs_path"
            return 0
        fi
    fi

    # Return original (will fail later with clear error)
    echo "$original_source"
    return 1
}

if [ -n "${DATA_SOURCE+x}" ]
then
    echo "DATA_SOURCE: ${DATA_SOURCE}"
    echo "DATADIR: ${DATADIR}"

    # Resolve DATA_SOURCE to an existing path (local or NFS)
    RESOLVED_SOURCE=$(resolve_data_source "${DATA_SOURCE}") || true
    if [[ "$RESOLVED_SOURCE" != "$DATA_SOURCE" ]]; then
        echo "Resolved DATA_SOURCE to: ${RESOLVED_SOURCE}"
        DATA_SOURCE="$RESOLVED_SOURCE"
    fi

    if [ "$(realpath "${DATA_SOURCE}/datadir" 2>/dev/null || echo "")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/datadir"/*  "${DATADIR}"
        sudo chmod -R a+w "${DATA_SOURCE}/datadir/blockchain"
        ls -al "${DATA_SOURCE}/datadir/blockchain"
        if [[ -e "${DATA_SOURCE}/shm_dir" && "$(realpath "${DATA_SOURCE}/shm_dir")" != "$(realpath "${SHM_DIR}")" ]]
        then
            echo "Creating copy of ${DATA_SOURCE}/shm_dir inside ${SHM_DIR}"
            sudo -Enu hived mkdir -p "${SHM_DIR}"
            flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/shm_dir"/* "${SHM_DIR}"
            sudo chmod -R a+w "${SHM_DIR}"
            ls -al "${SHM_DIR}"
        else
            echo "Skipping shm_dir processing."
        fi
        ls -al "${DATA_SOURCE}/datadir"
    fi
fi
