#!/bin/bash

# Pieces and their offsets,

# Extend it as what you want

# 

# Format: file1@addr file2@addr file3@addr ...

# 
fs=$1
# 

# 





# dist

dist=$2

da=0



panic()

{

    echo "Error:" $1

    exit 1

}



# 

# Input parameters:

# 

# 1 : dist file, 2 : offset, 3 : Zero bytes count

# 

fill_zero()

{

    dd if=/dev/zero of=$1 bs=1 count=$3 seek=$2 >/dev/null 2>&1

}



# 

# Input parameters:

# 

# 1 : dist file, 2 : source file, 3 : source offset

# 

append_file()

{

    dd if=$2 of=$1 bs=1 seek=$3 >/dev/null 2>&1

}



# main

if [ -f $dist ]; then

    rm -f $dist

    sync

fi



for file in $fs

do

    name=`echo -n $file | awk 'BEGIN {FS="@"} END{print $1}'`

    addr=`echo -n $file | awk 'BEGIN {FS="@"} END{print $2}'`



    # Check source file

    if [ ! -f $name ]; then

	panic $name\ "absense"

    fi



    # Check offset ok

    if [ $da -gt $addr ]; then

	panic "Addr"\ $addr\ "Overlap"

    fi



    # Fill 0 bytes

    if [ $da -lt $addr ]; then

	fill_zero $dist $da `expr $addr - $da`

    fi

    da=$addr



    # Append file

    append_file $dist $name $addr

    nr=`ls -l $name | awk '{print $5}'`

    da=`expr $da + $nr`

    

    echo $name "done."



    # next

done



sync



echo "File" $dist "is ok, size" `ls -l $dist | awk '{print $5}'` "bytes"
