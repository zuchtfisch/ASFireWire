#!/usr/bin/env bash
# Read-only diagnosis of a local ASFW app's ability to access the installed
# DriverKit UserClient. Does NOT install, sign, activate or open the driver.
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP="${1:-${ROOT}/build/DerivedData/Build/Products/Debug/ASFW.app}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "[ERROR] This check is intended for macOS." >&2
  exit 2
fi
if [[ ! -d "${APP}" ]]; then
  echo "[ERROR] App not found: ${APP}" >&2
  echo "Build first with ./build.sh --config Debug --no-bump" >&2
  exit 2
fi

echo "=== ASFW read-only connection preflight ==="
echo "App: ${APP}"
echo
echo "=== Installed system extension ==="
systemextensionsctl list 2>&1 | grep -E 'enabled|active|net\.asfw\.driver|extension\(s\)' || true

echo
echo "=== IOKit driver and FF800 registration ==="
# Restrict to the relevant property lines; no IOKit client is opened.
ioreg -r -c ASFWDriver -l 2>&1 |
  grep -E '(ASFWDriver[[:space:]]+<class|"IOUserClass" = "ASFWDriver"|"ASFWDeviceName" = "RME Fireface 800"|"ASFWGUID" =)' |
  head -n 30 || true

echo
echo "=== App signature ==="
codesign -dv --verbose=2 "${APP}" 2>&1 |
  grep -E '^(Executable=|Identifier=|Signature=|TeamIdentifier=|Authority=|CodeDirectory)' || true

echo
echo "=== App's *embedded* entitlements (not the source plist) ==="
ENTITLEMENTS="$(codesign -d --entitlements - "${APP}" 2>/dev/null || true)"
if [[ -n "${ENTITLEMENTS}" ]]; then
  printf '%s\n' "${ENTITLEMENTS}" |
    grep -A 5 -B 1 -E 'com\.apple\.developer\.driverkit\.userclient-access|net\.asfw\.driver' || true
else
  echo "No readable embedded entitlements."
fi

echo
echo "=== Available signing identities ==="
security find-identity -v -p codesigning 2>&1

echo
echo "=== Interpretation ==="
echo "DriverKit userclient-access is checked by macOS at IOServiceOpen."
echo "An installed/active extension does not imply the app can open its UserClient."
echo "An unsigned or ad-hoc app with no team identifier does not establish"
echo "the authorized entitlement required for this driver's UserClient."
echo "If the entitlement check is denied, use an authorized, properly signed"
echo "application; do not disable SIP, forge entitlements, or reinstall the driver."
echo "This tool reads metadata only and does not test selector 84."
