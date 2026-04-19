#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: uninstall-macos.sh must be run on macOS" >&2
  exit 1
fi

launch_agent="${HOME}/Library/LaunchAgents/io.sentrits.agent.plist"
package_root="${HOME}/Applications/Sentrits"
symlink_path="${HOME}/bin/sentrits"

launchctl unload "${launch_agent}" 2>/dev/null || true
rm -f "${launch_agent}"

if [[ -L "${symlink_path}" ]]; then
  symlink_target="$(readlink "${symlink_path}")"
  if [[ "${symlink_target}" == "${package_root}/bin/sentrits" ]]; then
    rm -f "${symlink_path}"
  fi
fi

rm -rf "${package_root}"

echo "Sentrits uninstalled from ${package_root}"
echo "Persistent state under ${HOME}/.sentrits was left untouched."
