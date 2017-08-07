#!/bin/bash --debug

RUNNING="`ps -fA | grep -w noad | wc -l`"


echo Zaehlung der Werbefilter:
echo

if [ $RUNNING = 1 ]
    then
    echo Zur Zeit ist kein Werbefilter aktiv.
fi

if [ $RUNNING = 2 ]
    then
    ANZAHL=$[$RUNNING-1]
    echo "Zur Zeit ist $ANZAHL Werbefilter aktiv!"
fi

if [ $RUNNING -gt 2 ]
    then
    ANZAHL=$[$RUNNING-1]
    echo "Zur Zeit sind $ANZAHL Werbefilter aktiv!"
fi
