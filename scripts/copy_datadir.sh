#! /bin/bash

set -xeuo pipefail


if [ -n "${DATA_SOURCE+x}" ]
then
    if [ "$(realpath "${DATA_SOURCE}/datadir")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/datadir"/*  "${DATADIR}"
        sudo -En chmod --recursive a+rw "${DATADIR}"
        if [ "$(realpath "${DATA_SOURCE}/shm_dir")" != "$(realpath "${SHM_DIR}")" ]
        then
            echo "Creating copy of ${DATA_SOURCE}/shm_dir inside ${SHM_DIR}"
            sudo -Enu hived mkdir -p "${SHM_DIR}"
            flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/shm_dir"/* "${SHM_DIR}"
        fi
    fi
fi
