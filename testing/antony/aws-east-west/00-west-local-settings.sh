#!/bin/bash

export WESTNET="192.0.0.254"

local_settings() {
	if [ "${WESTNET}" = "192.0.1.0/24" ] ; then
		# qemu testing
		sed -i -e 's/local_ts  =.*/local_ts  = 192.0.2.0\/24/g' east.swanctl.conf
		sed -i -e 's/remote_ts =.*/remote_ts = 192.0.1.0\/24/g' east.swanctl.conf

		sed -i -e 's/local_ts  =.*/local_ts  = 192.0.2.0\/24/g' west.swanctl.conf
		sed -i -e 's/remote_ts =.*/remote_ts = 192.0.1.0\/24/g' east.swanctl.conf

		export SUNSET="192.1.10.252"
		export SUNRISE="192.1.20.252"
		export WEST0="192.0.1.254"
		export EAST0="192.0.2.254"

	fi
}

ip addr show dev eth0 | grep -q "192.0.1.254" && export WESTNET="192.0.1.0/24"
local_settings
