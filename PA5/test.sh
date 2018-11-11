#!/bin/sh

N=10000
buffer=(20 100 200)
workers=(1 5 10 15 25 35 45 55 65 75 85 95 105 115 125 135 145 155 165 175 185 200 225 250 275 300 325 350 375 400 425 450 475 500)

for buffersz in "${buffer[@]}"
do
  for worker in "${workers[@]}"
  do
    # echo "./client -n $N -w $worker -b $buffersz"
    seconds=$(./client -n $N -w $worker -b $buffersz | grep Took | awk '{print $2}')
    echo "b=$buffersz, w=$worker: $seconds"

    # clean up FIFOs if needed
    rm -rf fifo*
  done
done
