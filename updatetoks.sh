#!/bin/sh

ed -s "./lc.c" <<EOF
/\/* Generated token stuff begins/+,/\/* Generated token stuff ends/-d
/\/* Generated token stuff begins/a
$(cat toks.c.inc)
.
wq
EOF
