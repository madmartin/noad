#!/bin/bash --debug

RUNNING="`ps -A | grep -w noad | wc -l`"
MARKSFILE="$1/marks.vdr"
PIDFILE="$1/noad.pid"

echo Aufzeichnung: 
echo
echo $1
echo


test -e $PIDFILE

#if [ $RUNNING -gt 0 ]  
if [  $? = 0 ]
	then
    	echo Werbefilter ist noch aktiv!
	echo
fi

test -e $MARKSFILE

if [  $? = 0 ]
	then
	cat $MARKSFILE
else
	echo Es sind keine Schnittmarkierungen vorhanden.
fi

