#!/bin/ksh
#dir=/home/cs452/fall15/phase2/testResults
dir=/Users/patrick/Classes/452/project/phase2/testResults

if [ "$#" -eq 0 ] 
then
    echo "Usage: ksh testphase2.ksh <num>"
    echo "where <num> is 00, 01, 02, ... or 26"
    exit 1
fi

num=$1
if [ -f test${num} ]
then
    /bin/rm test${num}
fi

if  make test${num} 
then
    if (( num == 14 )); then
        cp testcases/term1.in .
    fi
    test${num} > test${num}.txt 2> test${num}stderr.txt;
    if [ -s test${num}stderr.txt ]
    then
        cat test${num}stderr.txt >> test${num}.txt
    fi

    /bin/rm test${num}stderr.txt

    if diff --brief test${num}.txt ${dir}
    then
        echo
        echo test${num} passed!
    else
        echo
        diff -C 1 test${num}.txt ${dir}
    fi
fi
echo
