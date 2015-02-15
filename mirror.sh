#!/bin/bash

set -e

if [ ! $# -eq 1 ];
then
  echo "Usage:"
  echo "$0 <configfile>"
  exit 1
fi

CONFIG_FILE="$1"

. "${CONFIG_FILE}" || exit 1

SVN_CLONE="${PROJECT_ROOT}/svn-clone"
GIT_BARE="${PROJECT_ROOT}/git-bare-tmp"

if [ ! -d "${PROJECT_ROOT}" ];
then
  mkdir -p "${PROJECT_ROOT}"
fi

cd "${PROJECT_ROOT}"
PWD = `pwd`

echo $AUTHORS_FILE

if [ ! -d "${SVN_CLONE}" ];
then
  git svn clone \
    "${SVN_REPO}" \
    -A "${AUTHORS_FILE}" \
    ${SVN_LAYOUT} \
    "${SVN_CLONE}" \
    --no-minimize-url
  cd "${SVN_CLONE}"
else
  cd "${SVN_CLONE}"
  git remote rm bare || echo "failed to delete remote:bare, proceeding anyway"
  git svn rebase \
    --fetch-all \
    -A "../../${AUTHORS_FILE}"
fi

git remote add bare "${GIT_BARE}"
git config remote.bare.push 'refs/remotes/*:refs/heads/*'

if [ -d "${GIT_BARE}" ];
then
  rm -rf "${GIT_BARE}"
fi

mkdir -p "${GIT_BARE}"
cd "${GIT_BARE}"
git init --bare .
git symbolic-ref HEAD refs/heads/trunk

cd "${SVN_CLONE}"
git push bare

cd "${GIT_BARE}"
git branch -m trunk master
git for-each-ref --format='%(refname)' refs/heads/tags | \
   cut -d / -f 4 | \
   while read ref;
   do
      git tag "$ref" "refs/heads/tags/$ref"
      git branch -D "tags/$ref"
   done
git remote add origin "${GIT_REPO}"
git config branch.master.remote origin
git config branch.master.merge refs/heads/master
git push --tags origin master
git push --all

rm -rf "${GIT_BARE}"
