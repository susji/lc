#!/bin/bash

GREEN='\e[1;32m'
RED='\e[0;31m'
ORANGE="\e[0;33m"
NC='\e[0m'

runtest () {
    _cmd="$1"
    _name="$2"
    _wanted="$3"

    _got="$($1)"
    if [ "$_got" = "$_wanted" ]; then
        echo -e "[${GREEN}OK${NC}] $_name"
    else
        echo -e "[${RED}!!${NC}] $_name"
        diff \
            --color=always \
            -u999 \
            <(echo "$_wanted") <(echo "$_got") \
            | tail -n +3
    fi
}

for testfile in $(ls tests/*.lc); do
    _name="${testfile#tests/}"
    _name="${_name%.lc}"
    _res=$(sed '1,/OUTSTART/d;/OUTEND/,$d' "$testfile")
    if [ -z "$_res" ]; then
        echo -e "[${ORANGE}--${NC}] $_name: no result found"
        continue
    fi
    _exec="${testfile%.lc}.test"
    if [ ! -x "$_exec" ]; then
        echo -e "[${RED}!!${NC}] $_exec: executable not found"
        continue
    fi
    runtest "$_exec" "$_name" "$_res"
done

