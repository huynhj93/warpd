#!/bin/sh

export KEYCHAIN_NAME=warpd-temp-keychain
export KEYCHAIN_PASSWORD=

cd "$(dirname "$0")"

# codesign needs the temp keychain in the user's search list. Add it in front
# of the existing entries instead of replacing them, and put the original list
# back on exit: replacing it and then deleting the temp keychain used to leave
# the search list empty after every build.
orig_keychains=$(security list-keychains -d user)

cleanup() {
	eval "security list-keychains -d user -s $orig_keychains"
	security delete-keychain "${KEYCHAIN_NAME}" 2>/dev/null
}
trap cleanup EXIT

security delete-keychain "${KEYCHAIN_NAME}" 2>/dev/null
security create-keychain -p "${KEYCHAIN_PASSWORD}" "${KEYCHAIN_NAME}"

eval "security list-keychains -d user -s \"${KEYCHAIN_NAME}\" $orig_keychains"

security import warpd.p12 -k "${KEYCHAIN_NAME}" -P "" -T /usr/bin/codesign
security import warpd.cer -k "${KEYCHAIN_NAME}" -T /usr/bin/codesign
security unlock-keychain -p ${KEYCHAIN_PASSWORD} ${KEYCHAIN_NAME}

# Suppress codesign modal password prompt
security set-key-partition-list -S apple-tool:,apple: -s -k "$KEYCHAIN_PASSWORD" -D "${identity}" -t private ${KEYCHAIN_NAME} > /dev/null 2>&1

codesign --force --deep --keychain "${KEYCHAIN_NAME}" -s warpd ../bin/warpd
