#!/bin/sh
cd dnet
make clean
cd ../client
make clean
cd ../server
make clean
cd ../test
make clean
cd ../lib
rm -f *.o 
cd ..
rm -f *.o 

