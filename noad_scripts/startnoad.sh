#!/bin/bash --debug

RUNNING="`ps -A | grep -w noad | wc -l`"
PIDFILE="$1/noad.pid"

echo Aufzeichnung: 
echo
echo $1
echo

test -e $PIDFILE

if [  $? = 0 ]
#if [ $RUNNING -gt 0 ]  
	then
    	echo Werbefilter ist bereits aktiv!
	exit 0
fi


echo "/usr/local/bin/noad -B -S -O -j -a -o -c --statisticfile=$1/noad.sta nice $1" > /usr/local/bin/noad_tmp.sh
chmod +x /usr/local/bin/noad_tmp.sh
at now -f /usr/local/bin/noad_tmp.sh
echo "Werbefilter wird gestartet..."
