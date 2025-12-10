#! /bin/bash

set -xeuo pipefail

# Default shared block_log location (used when blockchain not in cache)
# Try NFS cache first, fall back to local cache
SHARED_BLOCK_LOG_DIR="${SHARED_BLOCK_LOG_DIR:-/nfs/ci-cache/hive/block_log_5m}"

# NFS fallback for cross-builder scenarios in CI pipelines
# When DATA_SOURCE points to a local cache path that doesn't exist (because the
# local_copy job ran on a different builder), fall back to NFS source
resolve_data_source() {
    local source="$1"

    # If source exists, use it directly
    if [[ -d "${source}/datadir" ]]; then
        echo "$source"
        return 0
    fi

    # Try NFS fallback: convert /cache/haf_pipeline_XXX to /nfs/ci-cache/haf/pipeline_XXX
    if [[ "$source" =~ ^/cache/haf_pipeline_([0-9]+)(_filtered)?$ ]]; then
        local pipeline_id="${BASH_REMATCH[1]}"
        local suffix="${BASH_REMATCH[2]}"
        local nfs_path="/nfs/ci-cache/haf/pipeline_${pipeline_id}${suffix}"

        if [[ -d "${nfs_path}/datadir" ]]; then
            echo "FALLBACK: Local cache not found at ${source}, using NFS: ${nfs_path}" >&2
            echo "$nfs_path"
            return 0
        fi
    fi

    # Check if source is already an NFS path
    if [[ "$source" =~ ^/nfs/ ]]; then
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
