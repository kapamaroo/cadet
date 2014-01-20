#!/bin/bash

if [ ! $# -eq 1 ]
then
    echo
    echo takes one parameter:  the EDA-testcase directory
    echo example:
    echo "          $0 EDA-testcase"
    echo
    exit
fi

TOOL=`pwd`/cadet

TEST_DIR="$1"
cd $TEST_DIR
TEST_DIR=`pwd`
cd -

LIBCELL=NangateLibrarySizes.txt

FILES=`ls $TEST_DIR/dimensions`

for i in $FILES;
do
    echo "################  $i  ###################"
    command $TOOL $TEST_DIR/$LIBCELL $TEST_DIR/dimensions/$i $TEST_DIR/legalized_cells/$i $TEST_DIR/nets/$i
done
