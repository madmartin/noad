#!/bin/sh
# this is an example-script for a noad-call with
# different parameters for a call before or after 
# a recording is done
# this script should be called from inside vdr via '-r ' 
# e.g. vdr '-r /usr/local/sbin/noadcall.sh'

# set the noad-binary here
NOAD="/usr/local/bin/noad"

# set the online-mode here 
# 1 means online for live-recording only
# 2 means online for every recording
ONLINEMODE="--online=1"

# set additional args for every call here here
ADDOPTS="--ac3 --overlap --jumplogo --comments"

# set special args for a call with 'before' here
# e.g. set a specail statistikfile
BEFOREOPTS="--statisticfile=/video0/noadonlinestat"

# set special args for a call with 'after' here
# e.g. backup the marks from the online-call before
#      so you can compare the marks and see
#      how the marks vary between online-mode 
#      and normal scan (backup-marks are in marks0.vdr)
AFTEROPTS="--backupmarks --statisticfile=/video0/noadstat"

if [ $1 == "before" ]; then
  $NOAD $* $ONLINEMODE $ADDOPTS $BEFOREOPTS
else
  $NOAD $* $ONLINEMODE $ADDOPTS $AFTEROPTS
fi
