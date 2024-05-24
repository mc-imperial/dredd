#!/bin/bash

SCRIPTPATH=$(dirname "$BASH_SOURCE")
echo $SCRIPTPATH

# make copy of orig file
cp ${SCRIPTPATH}/bit_overflow.cc ${SCRIPTPATH}/bit_overflow.orig.cc

# apply mutation
${DREDD_EXECUTABLE} ${SCRIPTPATH}/bit_overflow.cc --mutation-info-file ${SCRIPTPATH}/temp.json

# compile mutated binary
${DREDD_CLANG_BIN_DIR}/clang++ ${SCRIPTPATH}/bit_overflow.cc -o ${SCRIPTPATH}/bit_overflow.o

DREDD_ENABLED_MUTATION=32 ${SCRIPTPATH}/bit_overflow.o > ${SCRIPTPATH}/bit_overflow.cc.actual

# Compare actual output with expected output
if diff ${SCRIPTPATH}/bit_overflow.cc.expected ${SCRIPTPATH}/bit_overflow.cc.actual; then
    echo "Test Passed"
else
    echo "Test Failed"
fi

# cleanup
rm ${SCRIPTPATH}/temp.json
rm ${SCRIPTPATH}/bit_overflow.cc.actual
rm ${SCRIPTPATH}/bit_overflow.o
mv ${SCRIPTPATH}/bit_overflow.orig.cc ${SCRIPTPATH}/bit_overflow.cc 

exit 0