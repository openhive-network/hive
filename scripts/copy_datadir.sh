#! /bin/bash

set -xeuo pipefail


if [ -n "${DATA_SOURCE+x}" ]
then
    echo "DATA_SOURCE: ${DATA_SOURCE}"
    echo "DATADIR: ${DATADIR}"
    if [ "$(realpath "${DATA_SOURCE}/datadir")" != "$(realpath "${DATADIR}")" ]
    then
        echo "Creating copy of ${DATA_SOURCE}/datadir inside ${DATADIR}"
        sudo -Enu hived mkdir -p "${DATADIR}"
        flock "${DATA_SOURCE}/datadir" sudo -En cp -pr "${DATA_SOURCE}/datadir"/*  "${DATADIR}"
        sudo chmod -R a+w "${DATA_SOURCE}/datadir/blockchain"
        ls -al "${DATA_SOURCE}/datadir/blockchain"
        if [ [-e "${DATA_SOURCE}/shm_dir" && "$(realpath "${DATA_SOURCE}/shm_dir")" != "$(realpath "${SHM_DIR}")" ] ]
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
