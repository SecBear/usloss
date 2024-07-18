#!/bin/bash

/bin/rm outfile
touch outfile

for i in 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 
do
  make test$i
  echo starting test $i ....  >> outfile
  echo >> outfile
  echo  running test $i
  ./test$i >> outfile 2>&1 3>&-
  echo >> outfile
  rm test$i.o test$i
done
