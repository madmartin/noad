#!/bin/bash --debug

RUNNING="`ps -A | grep -w noad | wc -l`"
PIDFILE="$1/noad.pid"

echo Aufzeichnung: 
echo
echo $1
echo

test -e $PIDFILE

if [  $? -ne 0 ]
#if [ $RUNNING -gt 0 ]  
	then
    	echo Werbefilter ist nicht aktiv!
	exit 0
fi

kill `cat $PIDFILE`
echo Werbefilter ist angehalten.



