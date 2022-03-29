#! /bin/bash

function build_image_name() {
  local image=$1
  local tag=$2
  local registry=${3}
  echo "${registry}${image}:${tag}"
}

function docker_image_exists() {
  local image=$1
  local tag=$2
  local registry=${3}
  local imgname=$( build_image_name $image $tag $registry )

  echo "Looking for: $imgname"
  docker manifest inspect "$imgname" >/dev/null 2>&1
  local result=$?

  echo "docker manifest inspect returned $result"

  if [ "$result" -eq "0" ];
  then
    result=1
  else
    result=0
  fi
  
  return $result
}

