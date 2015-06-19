#!/bin/bash
# Lets try for /bin/sh but bashisms will sneak in.

# partial bash strict mode
set -uo pipefail

V=0;

if [ -f ~/.skiboot_boot_tests ]; then
	source ~/.skiboot_boot_tests
fi

# Utility functions
function error {
	unset SSHPASS;
	echo "$target: $1" >&2;
	exit 1;
}

function msg {
	if [ $V -ne 0 ] ; then
		echo "$target: $@";
	fi
}

# Generic conf
BOOT_SLEEP_PERIOD=10
FUNCTIONS_NEEDED="sshpass ssh ipmitool md5sum rsync expect";

function linux_boot {
	if [ $STRIP_CONTROL -eq 1 ]; then
	    STRIPCOMMAND="col -b -l 1"
	else
	    STRIPCOMMAND="cat"
	fi

	#Everyone is going to forget to disconnect - force them off
	ipmiresult=$($IPMI_COMMAND sol deactivate 2>&1);
	retval=$?
	if [ $retval -ne 0 -a "$ipmiresult" != "Info: SOL payload already de-activated" ]; then
	    msg "IPMI sol deactivate failed; IPMI may have stalled, may just be IPMI. Good luck."
	fi

	LINUXBOOT_LOG=$(mktemp --tmpdir builder-2.XXXXXX);
	cat <<EOF | expect > $LINUXBOOT_LOG
set timeout 300
spawn $IPMI_COMMAND sol activate
expect {
timeout { send_user "\nTimeout waiting for petitboot\n"; exit 1 }
eof { send_user "\nUnexpected EOF\n;" exit 1 }
"Welcome to Petitboot"
}

close
exit 0
EOF
	retval=$?
	$IPMI_COMMAND sol deactivate > /dev/null;
	if [ $retval -ne 0 ]; then
	        msg "Waiting for linux has timed out"
		msg "Boot log follows:"
		cat $LINUXBOOT_LOG
		rm -f $LINUXBOOT_LOG
		return 1
	else
	        rm -f $LINUXBOOT_LOG
	        return 0
	fi
}

function boot_test {
        # The functions called (e.g. flash, boot) are from the *_support files
        if [ $bootonly -ne 1 ]; then
	    msg "Flashing ${target}..."
	    flash $@;
	fi

	msg "Booting $target..."
	boot_firmware;
	msg "firmware looks good, waiting for linux";

	linux_boot;
	if [ $? -ne 0 ] ; then
		error "Couldn't reach petitboot on $target";
	fi
	msg "$target has booted";
	unset SSHPASS;
}

function sanity_test {
    $SSHCMD true;
    if [ $? -ne 0 ]; then
	echo "$target: Failed to SSH to $target..."
        echo "$target: Command was: $SSHCMD true"
	error "Try connecting manually to diagnose the issue."
    fi

    $IPMI_COMMAND chassis power status > /dev/null;
    if [ $? -ne 0 ]; then
	echo "$target: Failed to connect to $target with IPMI..."
        echo "$target: Command was: $IPMI_COMMAND chassis power status"
	error "Try connecting manually to diagnose the issue."
    fi

    # do further machine-type specific tests
    machine_sanity_test
}

function usage {
    cat <<EOF
boot_test.sh tests the bootability of a given target, optionally after
  flashing new firmware onto the target.

There are three usage modes.

1) boot_test.sh -h
     Print this help

2) boot_test.sh [-vdp] -t target -B -b (fsp|bmc)
     Boot test the target without flashing. Specify the type of machine
     (FSP or BMC) with the -b option.

3) boot_test.sh [-vdp] -b bmc -t target -P pnor
   boot_test.sh [-vdp] -b fsp -t target [-1 lid1] [-2 lid2] [-3 lid3]

     Flash the given firmware before boot testing.

     If -P is given, the file is assumed to be a PNOR for BMC based
     flashing.

     If -1/-2/-3 are given, the files are assumed to be lids for FSP based
     flashing. Any combination of lids is acceptable.

Common Options:

  -p powers off the machine if it is running. Without -p, a running machine
     will cause the script to error out.

  -v makes the script print some progress messages. Recommended.

  -d makes the script print lots of things (set -vx).
     Only use this for debugging the script: it's highly likely that
     successful booting into Petitboot will not be detected with this option.

  -b BMC type (bmc or fsp).
EOF
    exit 1;
}

## 'Main' script begins

# Check prereqs
for func in $FUNCTIONS_NEEDED ; do
	if ! command -v "$func" &> /dev/null ; then
		error "I require command $func but it is not in \$PATH ($PATH)";
	fi
done

# Parse options
V=0;
bootonly=0;
powerdown=0;
firmware_supplied=0;
target=""
method=""
PNOR=""
LID[0]=""
LID[1]=""
LID[2]=""
while getopts "hvdpB1:2:3:P:t:b:" OPT; do
    case "$OPT" in
	v)
	    V=1;
	    ;;
	h)
	    usage;
	    ;;
	d)
	    set -vx;
	    ;;
	B)
	    bootonly=1;
	    if [ $firmware_supplied -eq 1 ]; then
		usage
	    fi
	    ;;
	p)
	    powerdown=1;
	    ;;
	b)
	    method=$OPTARG;
	    ;;
	1|2|3)
	    firmware_supplied=1;
	    if [ ! -e "$OPTARG" ] ; then
		error "Couldn't stat $OPTARG";
	    fi
	    LID[$(expr ${OPT} - 1)]="$OPTARG"
	    ;;
	P)
	    firmware_supplied=1;
	    if [ ! -e "$OPTARG" ] ; then
		error "Couldn't stat $OPTARG";
	    fi
	    PNOR="$OPTARG"
	    ;;
	t)
	    target=$OPTARG;
	    ;;
	\?)
	    usage;
	    ;;
    esac
done

shift $(expr $OPTIND - 1);

# Pull out the target and test
if [ "$target" = "" ]; then
    usage;
fi

if ! ping -c 1 "$target" &> /dev/null ; then
	error "Couldn't ping $target";
fi

if [ "$#" -ne 0 ]; then
    usage
fi


# pull in the relevant config file and set things up
source $(dirname $0)/${method}_support.sh
IPMI_COMMAND="ipmitool -I lanplus -H $target $IPMI_AUTH"

msg "Running sanity test"
sanity_test
msg "Passed."

# check the target is down
# (pulls in is_off from ${method}_support.sh)
if ! is_off; then
    if [ $powerdown -eq 1 ]; then
	poweroff
    else
	error "$target is not turned off";
    fi
fi

# run the boot test
echo "$target: Boot testing $target";
begin_t=$(date +%s);
boot_test

echo "$target: Done in $(expr $(date +%s) - $begin_t ) seconds";
