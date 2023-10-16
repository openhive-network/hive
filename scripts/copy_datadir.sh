#! /bin/bash

set -xeuo pipefail


if [ -n "${DATA_SOURCE+x}" ]
then
    if [ "$(realpath "${DATA_SOURCE}/datadir")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        sudo -En rm -rf "${DATADIR}"/*
        flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/datadir"/*  "${DATADIR}"

        if  [ -d "$(realpath "${DATA_SOURCE}/shm_dir")" ] && \
            [ "$(realpath "${DATA_SOURCE}/shm_dir")" != "$(realpath "${SHM_DIR}")" ]
        then
            echo "Creating copy of ${DATA_SOURCE}/shm_dir inside ${SHM_DIR}"
            sudo -Enu hived mkdir -p "${SHM_DIR}"
            sudo -En rm -rf "${SHM_DIR}"/*
            flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/shm_dir"/* "${SHM_DIR}"
        fi
    fi
fi
