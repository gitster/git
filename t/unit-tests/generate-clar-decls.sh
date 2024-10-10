#!/bin/sh

if test $# -lt 2
then
	echo "USAGE: $0 <OUTPUT> <SUITE>..." 2>&1
	exit 1
fi

OUTPUT="$1"
shift

while test "$#" -ne 0
do
	suite="$1"
	shift
	sed -ne "s/^\(void test_$suite__[a-zA-Z_0-9][a-zA-Z_0-9]*(void)$\)/extern \1;/p" "$suite" ||
	exit 1
done >"$OUTPUT"
