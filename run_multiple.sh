#!/usr/bin/env bash

# Number of processes to launch
N=20

echo "Launching $N instances of ./build/debug/bin/eROIL_Tests.exe..."

for ((i=0; i<N; i++)); do
    echo "Starting instance $i"
    gnome-terminal -- bash -c "./build/linux-debug/bin/eROIL_Tests $i; exec bash"
done

echo "Done."
exit 0
