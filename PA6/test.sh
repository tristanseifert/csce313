#!/bin/sh

N=10000
buffer=(1000)
workers=(5 10 15 25 35 45 55 65 75 85 95 105 115 125 135 145 155 165 175 185 200 250 300 400 500 600 700 800 900 1000 1500 2000 2500)

echo "TYPE\tBUF\tW\tTIME"

# Test FIFOs
# echo "Testing FIFO:"

for buffersz in "${buffer[@]}"
do
  for worker in "${workers[@]}"
  do
    seconds=$(./client -n $N -w $worker -b $buffersz -i f | grep Took | awk '{print $2}')
    echo "fifo\t$buffersz\t$worker\t$seconds"

    # clean up FIFOs if needed
    rm -rf fifo_*
  done
done

# Test message queues
# echo "\n\nTesting message queues:"
echo "\n\n"

for buffersz in "${buffer[@]}"
do
  for worker in "${workers[@]}"
  do
    seconds=$(./client -n $N -w $worker -b $buffersz -i q | grep Took | awk '{print $2}')
    echo "mq\t$buffersz\t$worker\t$seconds"

    # clean up temp files if needed
    ./delete_mq.sh
  done
done

# Test shared memory
# echo "\n\nTesting shared memory:"
echo "\n\n"

for buffersz in "${buffer[@]}"
do
  for worker in "${workers[@]}"
  do
    seconds=$(./client -n $N -w $worker -b $buffersz -i s | grep Took | awk '{print $2}')
    echo "shm\t$buffersz\t$worker\t$seconds"

    # clean up temp files if needed
    ./delete_shm.sh
  done
done
