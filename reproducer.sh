#!/bin/bash

WORKING_DEVICE=/dev/nvme0n1

echo "Working device: $WORKING_DEVICE"

rm -f ./blkx1.output
rm -f ./blkx2.output
rm -f ./blkx-discard[1-3].output

./blkx $WORKING_DEVICE 2 >> blkx1.output 2>&1 &
BLKX1_PID=$!
echo "Running blkx #1 at offset 2G with PID $BLKX1_PID and writing output at ./blkx1.output"
./blkx $WORKING_DEVICE 5 >> blkx2.output 2>&1 &
BLKX2_PID=$!
echo "Running blkx #2 at offset 5G with PID $BLKX2_PID and writing output at ./blkx2.output"

export BLKX_DISCARD_PIDS=''
run_blkx_discard() {
        BLKX_DISCARD_ID=$1
        BLKX_DISCARD_OFFSET_IN_GB=$2
	./blkx-discard $WORKING_DEVICE $BLKX_DISCARD_OFFSET_IN_GB >> blkx-discard$BLKX_DISCARD_ID.output 2>&1
	BLKX_DISCARD_PID=$!
        echo "Running blkx-discard #$BLKX_DISCARD_ID at offset $BLKX_DISCARD_OFFSET_IN_GB with PID $BLKX_DISCARD_PID and writing output at ./blkx-discard$BLKX_DISCARD_ID.output"
}

sleep 1

( run_blkx_discard 1 20 ) &
BLKX_DISCARD_PIDS+=" $!"
( run_blkx_discard 2 50 ) &
BLKX_DISCARD_PIDS+=" $!"
( run_blkx_discard 3 100 ) &
BLKX_DISCARD_PIDS+=" $!"

if wait -n $BLKX1_PID $BLKX2_PID; then
        echo "BLKX succeeded"
else
        echo "BLKX failed; inspect blkx*.output files in `pwd`"
fi

# do other stuff
kill -9 $BLKX1_PID > /dev/null 2> /dev/null
kill -9 $BLKX2_PID > /dev/null 2> /dev/null
echo "BLKX_DISCARD_PIDS: $BLKX_DISCARD_PIDS"
kill -9 $BLKX_DISCARD_PIDS > /dev/null 2> /dev/null
