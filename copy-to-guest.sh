#!/usr/bin/env bash

# Bail on errors.
set -e

# Establish the root directory.
self="${0#./}"
base="${self%/*}"
current=`pwd`
if [ "$base" = "$self" ] ; then
    script_dir=$current
elif [[ $base =~ ^/ ]]; then
    script_dir="$base"
else
    script_dir="$current/$base"
fi
cd $script_dir


rm -rf /var/lib/vz/private/$1/agent/*
mkdir -p /var/lib/vz/private/$1/agent
sudo cp -rf * /var/lib/vz/private/$1/agent/
sudo cp -rf deploy/* /var/lib/vz/private/$1/usr/bin/
