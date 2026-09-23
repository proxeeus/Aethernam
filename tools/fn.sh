#!/bin/sh
# usage: fn.sh listing name...   print function blocks
L=$1; shift
for n in "$@"; do awk -v n="$n" '/^;=====/{p=0} $0 ~ "^; "n" " {p=1} p' "$L"; done
