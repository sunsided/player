#!/bin/bash

# extracts authors from an SVN repository
# creates a boilerplate authors.txt file
# to be used by git-svn

set -e

if [ ! $# -eq 1 ];
then
	echo "Usage:"
	echo "$0 <svn-url>"
	exit 1
fi

SVN_URL="$1"

SVN_ROOT=`svn info "${SVN_URL}" | grep '^Repository.Root' | sed -e 's/^Repository.Root: //'`

# file format:
# username = User Name <user@ema.il>

svn log -q "${SVN_ROOT}" \
	| awk -F '|' '/^r/ {sub("^ ", "", $2); sub(" $", "", $2); print $2}' \
	| sort -u
#	| ./name-lookup.py

