#! /bin/bash

function build_image_name() {
  local image=$1
  local tag=$2
  local registry=${3}
  echo "${registry}${image}:${image}-${tag}"
}

function build_image_registry_path() {
  local image=$1
  local tag=$2
  local registry=${3}
  echo "${registry}${image}"
}

function build_image_registry_tag() {
  local image=$1
  local tag=$2
  local registry=${3}
  echo "${image}-${tag}"
}

function docker_image_exists() {
  local image=$1
  local tag=$2
  local registry=${3}
  local  __resultvar=${4:?"Missing required retval variable name"}
  local look_to_registry=${5:-1}

  local imgname=$( build_image_name $image $tag $registry )

  local result=0

  local old_set_e=0

  [[ $- = *e* ]] && old_set_e=1
  set +e

  echo "Looking for: $imgname"
  
  if [ "${look_to_registry}" -eq "1" ];
  then
    docker manifest inspect "$imgname" >/dev/null 2>&1
    result=$?
    echo "docker manifest inspect returned $result"
  else
    docker image inspect --format '{{.Id}}' "$imgname" >/dev/null 2>&1
    result=$?
    echo "docker image inspect returned $result"
  fi

  ((old_set_e)) && set -e


  local EXISTS=0

  if [ "$result" -eq "0" ];
  then
    EXISTS=1
  fi

  if [[ "$__resultvar" ]]; then
      eval $__resultvar="'$EXISTS'"
  else
      echo "$EXISTS"
  fi
}

