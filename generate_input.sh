#!/bin/sh

NB_LINES=$1
FILE=$2

generateType(){
	TYPE=$(cat /dev/urandom | tr -dc '01' | head -c 1)
}

generateKey(){
	KEY=$(cat /dev/urandom | tr -dc 'A-Z' | head -c 2)
}

generateValue(){
	VALUE=$(cat /dev/urandom | tr -dc '0-9' | head -c 8)
}

for i in `seq $NB_LINES` ; do 
	generateType;
	generateKey;
	if test "$TYPE" -eq 1 ; then 
		TYPE="put";
		generateValue;
		CMD="$TYPE,$KEY,$VALUE";
	else 
		TYPE="get";
		CMD="$TYPE,$KEY";
	fi
	echo "$CMD"  >> $FILE
done;
