#! /bin/bash

set -euo pipefail 

exec > >(tee -i "${LOG_FILE}") 2>&1

log_exec_params() {
  echo
  echo -n "$0 parameters: "
  for arg in "$@"; do echo -n "$arg "; done
  echo
}

do_clone_commit() {
  local commit="$1"
  local src_dir=$2
  local repo_url=$3
  
  echo "Cloning commit: $commit from $repo_url into: $src_dir ..."
  mkdir -p "$src_dir"
  pushd "$src_dir"
  
  git init
  git remote add origin "$repo_url"
  git fetch --depth 1 origin "$commit"
  git checkout FETCH_HEAD
  git submodule update --init --recursive
  
  popd
}

do_clone_branch() {
  local branch=$1
  local src_dir="$2"
  local repo_url="$3"
  echo "Cloning branch: $branch from $repo_url ..."
  git clone --recurse-submodules --shallow-submodules --single-branch --depth=1 --branch "$branch" -- "$repo_url" "$src_dir"
}


do_clone() {
  local branch=$1
  local src_dir="$2"
  local repo_url="$3"
  local commit="$4"

  if [[ "$commit" != "" ]]; then
    do_clone_commit $commit "$src_dir" $repo_url
  else
    do_clone_branch "$branch" "$src_dir" $repo_url
  fi
}

