#!/bin/csh -f

set dir = `pwd`
set path = ($dir $dir/ds3100.md $path)
set s = '.a'
set f = "-l 1 -prefix $dir/../data/prefixes"

if (0) then

echo "Doing results1a"$s
rm -rf results1a$s
mkdir results1a$s
cd results1a$s
time proc4 $f \
  -f /scratch4/traces1/files.a \
  -s 23.23.31 -e 24.23.31 \
  | extract >! results
cd ..

echo "Doing results1b"$s
mkdir results1b$s
cd results1b$s
time proc4 $f \
  -f /scratch4/traces1/files.b \
  -s 24.23.31 -e 25.23.31 \
  | extract >! results
cd ..

endif

echo "Doing results2a"$s
mkdir results2a$s
cd results2a$s
time proc4 $f \
  -f /scratch4/traces2/files.a \
  -s 10.13.00 -e 11.13.00 \
  | extract >! results
cd ..

echo "Doing results2b"$s
mkdir results2b$s
cd results2b$s
time proc4 $f \
  -f /scratch4/traces2/files.b \
  -s 11.13.00 -e 12.13.00  \
  | extract >! results
cd ..

echo "Doing results3a"$s
mkdir results3a$s
cd results3a$s
time proc4 $f \
  -f /scratch4/traces3/files.a \
  -s 14.18.07 -e 15.18.07 \
  | extract >! results
cd ..

exit

echo "Doing results3b"$s
mkdir results3b$s
cd results3b$s
time proc4 $f \
  -f /scratch4/traces3/files.b \
  -s 15.18.07 -e 16.18.07 \
  | extract >! results
cd ..

echo "Doing results4a"$s
mkdir results4a$s
cd results4a$s
time proc4 $f \
  -f /scratch4/traces4/files \
  -s 26.13.07 -e 27.13.07 \
  | extract >! results
cd ..

echo "Doing results5a"$s
mkdir results5a$s
cd results5a$s
time proc4 $f \
  -f /scratch4/traces5/files \
  -s 27.14.22 -e 28.14.22 \
  | extract >! results
cd ..
