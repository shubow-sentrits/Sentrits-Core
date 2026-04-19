#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "error: install-macos.sh must be run on macOS" >&2
  exit 1
fi

if [[ $# -ne 1 ]]; then
  echo "usage: $0 /path/to/sentrits-<version>-macos-<arch>.dmg|.tar.gz" >&2
  exit 1
fi

artifact_path="$1"
install_root="${HOME}/Applications"
package_root="${install_root}/Sentrits"
launch_agent="${HOME}/Library/LaunchAgents/io.sentrits.agent.plist"
binary_path="${package_root}/bin/sentrits"

if [[ ! -f "${artifact_path}" ]]; then
  echo "error: install artifact not found: ${artifact_path}" >&2
  exit 1
fi

mkdir -p "${install_root}"

staging_dir="$(mktemp -d "${TMPDIR:-/tmp}/sentrits-install-XXXXXX")"
mount_dir=""
cleanup() {
  if [[ -n "${mount_dir}" ]]; then
    hdiutil detach "${mount_dir}" >/dev/null 2>&1 || true
    rm -rf "${mount_dir}"
  fi
  rm -rf "${staging_dir}"
}
trap cleanup EXIT

case "${artifact_path}" in
  *.dmg)
    mount_dir="$(mktemp -d "${TMPDIR:-/tmp}/sentrits-dmg-XXXXXX")"
    hdiutil attach -nobrowse -readonly -mountpoint "${mount_dir}" "${artifact_path}" >/dev/null
    cp -R "${mount_dir}/Sentrits" "${staging_dir}/Sentrits"
    ;;
  *.tar.gz)
    tar -xzf "${artifact_path}" -C "${staging_dir}"
    ;;
  *)
    echo "error: expected a .dmg or .tar.gz install artifact" >&2
    exit 1
    ;;
esac

if [[ ! -x "${staging_dir}/Sentrits/bin/sentrits" ]]; then
  echo "error: install artifact does not contain Sentrits/bin/sentrits" >&2
  exit 1
fi

rm -rf "${package_root}"
mv "${staging_dir}/Sentrits" "${package_root}"

"${binary_path}" service install
launchctl unload "${launch_agent}" 2>/dev/null || true
launchctl load "${launch_agent}"

echo "Sentrits installed to ${package_root}"
echo "LaunchAgent loaded from ${launch_agent}"
echo
echo "Smoke checks:"
echo "  ${binary_path} host status"
echo "  curl http://127.0.0.1:18085/health"
