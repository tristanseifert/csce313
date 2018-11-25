#!/bin/sh

N=10000
buffer=(100 1000)
workers=(5 10 15 25 35 45 55 65 75 85 95 105 115 125 135 145 155 165 175 185 200 250 300 400 500 600 700 800 900 1000 1500 2000 2500)

for buffersz in "${buffer[@]}"
do
  for worker in "${workers[@]}"
  do
    seconds=$(./client -n $N -w $worker -b $buffersz | grep Took | awk '{print $2}')
    echo "b=$buffersz, w=$worker: $seconds"

    # clean up FIFOs if needed
    rm -rf fifo*
  done
done
