#/usr/bin/env bash

set -e

systemctl --user stop sentrits.service
systemctl --user disable sentrits.service
systemctl --user daemon-reload
sudo dpkg --remove sentrits

echo "Sentrits uninstalled successfully."