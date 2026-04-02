#!/usr/bin/env bash
set -euo pipefail

cd Sentrits-Core
exec ./build/sentrits serve
