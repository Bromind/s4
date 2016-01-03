#!/bin/sh

if [ $# != 8 ];
then
	echo $#
	echo "usage : ./s4 add1 port1 add2 port2 add3 port3 index file"
	exit 1;
fi;

OUTPUT="`echo $8 | cut -d '.' -f 1`.output"

if [ ! -e "s4.bin" ] 
then 
echo "The binary file is not created, running make"
make clean
make
fi

echo "#!/bin/sh\n./s4.bin $1 $2 $3 $4 $5 $6 $7 $8 > $OUTPUT" > ./launch
chmod +x launch

trap './launch' 2

while [ true ] ; do 
sleep 100;
done
