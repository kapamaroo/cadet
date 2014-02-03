#!/bin/bash

if [ ! $# -eq 1 ]
then
    echo
    echo takes one parameter:  the wire size
    echo example:
    echo "          $0 wire_size"
    echo
    exit
fi

TOOL=`pwd`/cadet

WIRE_SIZE="$1"
TEST_DIR="EDA-testcase"
cd $TEST_DIR
TEST_DIR=`pwd`
cd -

LIBCELL=NangateLibrarySizes.txt

FILES=`ls $TEST_DIR/dimensions`

for i in $FILES;
do
    echo "################  $i  ###################"
    command $TOOL $TEST_DIR/$LIBCELL $TEST_DIR/dimensions/$i $TEST_DIR/legalized_cells/$i $TEST_DIR/nets/$i $WIRE_SIZE > layer_$i.log
done
