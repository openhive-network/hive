#! /bin/bash

set -euo pipefail 

exec > >(tee "${LOG_FILE}") 2>&1

log_exec_params() {
  echo
  echo -n "$0 parameters: "
  for arg in "$@"; do echo -n "$arg "; done
  echo
}

do_clone() {
  local branch=$1
  local src_dir="$2"
  local repo_url="$3"
  echo "Cloning branch: $branch from $repo_url ..."
  git clone --recurse-submodules --shallow-submodules --single-branch --depth=1 --branch "$branch" -- "$repo_url" "$src_dir"
}

