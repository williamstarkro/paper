#!/bin/bash
set -euo pipefail
IFS=$'\n\t'

mkdir -p ~/Paper
if [ ! -f ~/Paper/config.json ]; then
  echo "Config File not found, adding default."
  cp /usr/share/paper/config.json ~/Paper/
fi
/usr/bin/paper_node --daemon
