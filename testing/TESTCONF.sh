#! /bin/sh -f

#
# Variables:  (* = exported)
#  *SNMP_TMPDIR:  	  place to put files used in testing.
#   SNMP_TESTDIR: 	  where the test scripts are kept.
#  *SNMP_PERSISTENT_FILE: where to store the agent's persistent information
#                         (XXX: this should be specific to just the agent)

#
# Only allow ourselves to be eval'ed once
#
if [ "x$TESTCONF_SH_EVALED" != "xyes" ]; then
    TESTCONF_SH_EVALED=yes

#
# set cpu and memory limits to prevent major damage
#
# defaults: 1h CPU, 500MB VMEM
#
[ "x$SNMP_LIMIT_VMEM" = "x" ] && SNMP_LIMIT_VMEM=512000
[ "x$SNMP_LIMIT_CPU" = "x" ] && SNMP_LIMIT_CPU=3600
# ulimit will fail if existing limit is lower -- ignore because it's fine
ulimit -S -t $SNMP_LIMIT_CPU 2>/dev/null
ulimit -S -v $SNMP_LIMIT_VMEM 2>/dev/null

#
# Set up an NL suppressing echo command
#
case "`echo 'x\c'`" in
  'x\c')
    ECHO() { echo -n $*; }
    ;;
  x)
    ECHO() { echo $*\\c; }
    ;;
  *)
    echo "I don't understand your echo command ..."
    exit 1
    ;;
esac
#
# how verbose should we be (0 or 1)
#
if [ "x$SNMP_VERBOSE" = "x" ]; then
    SNMP_VERBOSE=0
    export SNMP_VERBOSE
fi

if [ "x$MIBDIRS" = "x" ]; then
  if [ "x$SNMP_PREFER_NEAR_MIBS" = "x1" ]; then
    MIBDIRS=${SNMP_BASEDIR}/../mibs
    export MIBDIRS
  fi
fi

# Set up the path to the programs we want to use.
if [ "x$SNMP_PATH" = "x" ]; then
    PATH=../agent:../apps:../../agent:../../apps:$PATH
    export PATH
    SNMP_PATH=yes
    export SNMP_PATH
fi
    

# Set up temporary directory
if [ "x$SNMP_TMPDIR" = "x" -a "x$SNMP_HEADERONLY" != "xyes" ]; then
    if [ "x$testnum" = "x" ] ; then
        testnum=1
    fi
    SNMP_TMPDIR="/tmp/snmp-test-$testnum-$$"
    export SNMP_TMPDIR
    if [ -d $SNMP_TMPDIR ]; then
	echo "$0: ERROR: $SNMP_TMPDIR already existed."
	exit 1;
    fi
    SNMP_TMP_PERSISTENTDIR=$SNMP_TMPDIR/persist
    export SNMP_TMP_PERSISTENTDIR
    mkdir $SNMP_TMPDIR
    mkdir $SNMP_TMP_PERSISTENTDIR
fi

if [ "x$SNMP_SAVE_TMPDIR" = "x" ]; then
    SNMP_SAVE_TMPDIR="no"
    export SNMP_SAVE_TMPDIR
fi

SNMP_TESTDIR="$SNMP_BASEDIR/tests"
SNMP_CONFIG_FILE="$SNMP_TMPDIR/snmpd.conf"
SNMPTRAPD_CONFIG_FILE="$SNMP_TMPDIR/snmptrapd.conf"
SNMP_SNMPTRAPD_LOG_FILE="$SNMP_TMPDIR/snmptrapd.log"
SNMP_SNMPTRAPD_PID_FILE="$SNMP_TMPDIR/snmptrapd.pid"
SNMP_SNMPD_PID_FILE="$SNMP_TMPDIR/snmpd.pid"
SNMP_SNMPD_LOG_FILE="$SNMP_TMPDIR/snmpd.log"
#SNMP_PERSISTENT_FILE="$SNMP_TMP_PERSISTENTDIR/persistent-store.conf"
#export SNMP_PERSISTENT_FILE

## Setup default flags and ports iff not done
if [ "x$SNMP_FLAGS" = "x" ]; then
    SNMP_FLAGS="-d"
fi
if test -x /bin/netstat ; then
    NETSTAT=/bin/netstat
elif test -x /usr/bin/netstat ; then
    NETSTAT=/usr/bin/netstat
elif test -x /usr/sbin/netstat ; then
    # e.g. Tru64 Unix
    NETSTAT=/usr/sbin/netstat
else
    NETSTAT=""
fi
if [ "x$SNMP_SNMPD_PORT" = "x" ]; then
    SNMP_SNMPD_PORT="8765"
    MAX_RETRIES=3
    if test -x "$NETSTAT" ; then
        if test -z "$RANDOM"; then
            RANDOM=2
        fi
        while :
        do
            IN_USE=`$NETSTAT -a -n 2>/dev/null | grep "[\.:]$SNMP_SNMPD_PORT "`
            if [ $? -eq 0 ]; then
                #ECHO "Port $SNMP_SNMPD_PORT in use:"
                #echo "->$IN_USE"
                SNMP_SNMPD_PORT=`expr $SNMP_SNMPD_PORT + \( $RANDOM % 100 \)`
            else
                #echo "Using port $SNMP_SNMPD_PORT"
                break
            fi
            MAX_RETRIES=`expr $MAX_RETRIES - 1`
            if [ $MAX_RETRIES -eq 0 ]; then
                echo "ERROR: Could not find available port."
                exit 255
            fi
        done
    fi
fi
if [ "x$SNMP_SNMPTRAPD_PORT" = "x" ]; then
    SNMP_SNMPTRAPD_PORT=`expr $SNMP_SNMPD_PORT - 1`
fi
if [ "x$SNMP_TRANSPORT_SPEC" = "x" ];then
	SNMP_TRANSPORT_SPEC="udp"
fi
if [ "x$SNMP_TEST_DEST" = "x" -a $SNMP_TRANSPORT_SPEC != "unix" ];then
	SNMP_TEST_DEST="localhost:"
fi
export SNMP_FLAGS SNMP_SNMPD_PORT SNMP_SNMPTRAPD_PORT

# Make sure the agent doesn't parse any config file but what we give it.  
# this is mainly to protect against a broken agent that doesn't
# properly handle combinations of -c and -C.  (since I've broke it before).
SNMPCONFPATH="$SNMP_TMPDIR/does-not-exist"
export SNMPCONFPATH

fi # Only allow ourselves to be eval'ed once
