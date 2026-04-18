#/usr/bin/env bash

set -e

sudo dpkg -i $1
systemctl --user daemon-reload
systemctl --user enable  sentrits.service
systemctl --user start sentrits.service

echo "Sentrits installed and started successfully."
echo $(systemctl --user status sentrits.service)
