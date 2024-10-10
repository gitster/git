#!/bin/sh

if test $# -ne 5
then
	echo "USAGE: $0 <GIT_VERSION> <PERL_HEADER> <PERL_PATH> <PERL_SCRIPT> <OUT>" >&2
	exit 1
fi

GIT_VERSION="$1"
PERL_HEADER="$2"
PERL_PATH="$3"
PERL_SCRIPT="$4"
OUT="$5"

sed -e '1{' \
    -e "	s|#!.*perl|#!$PERL_PATH|" \
    -e "	r $PERL_HEADER" \
    -e '	G' \
    -e '}' \
    -e "s/@GIT_VERSION@/$GIT_VERSION/g" \
    "$PERL_SCRIPT" >"$OUT"
chmod a+x "$OUT"
