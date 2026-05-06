#!/bin/bash
#
# build_mac.sh — Build, sign, package, notarize MoonVerb for macOS.
#
# Prerequisites (one-time, already done on Quentin's Mac 2026-05-06):
#   - Xcode Command Line Tools installed
#   - Developer ID Application + Developer ID Installer certificates in Keychain
#   - Apple intermediate G2 Sub-CA installed
#   - notarytool credentials stored under profile DEADLOOP_NOTARY
#
# Usage:
#   chmod +x build_mac.sh
#   ./build_mac.sh
#
# Output:
#   artifacts/MoonVerb_v<VERSION>_macOS.pkg  (signed + notarized + stapled)

set -euo pipefail

# ============ CONFIG ============
PLUGIN_NAME="MoonVerb"
BUNDLE_ID="fr.deadloop.moonverb"
TEAM_ID="K6RAL2475G"
APP_CERT="Developer ID Application: LEROY QUENTIN (${TEAM_ID})"
INST_CERT="Developer ID Installer: LEROY QUENTIN (${TEAM_ID})"
NOTARY_PROFILE="DEADLOOP_NOTARY"

VERSION=$(grep -E 'VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -1 | sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')
echo "==> Version: ${VERSION}"

ROOT="$(pwd)"
BUILD_DIR="${ROOT}/build"
ARTIFACTS_DIR="${ROOT}/artifacts"
STAGE_DIR="${ROOT}/build/_pkg_stage"
mkdir -p "${ARTIFACTS_DIR}"

# ============ 1. BUILD ============
echo "==> [1/6] Configuring CMake (universal arm64 + x86_64)..."
rm -rf "${BUILD_DIR}"
cmake -B "${BUILD_DIR}" -G "Xcode" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13

echo "==> [2/6] Building Release..."
cmake --build "${BUILD_DIR}" --config Release --target ${PLUGIN_NAME}_All

PLUGIN_OUT="${BUILD_DIR}/${PLUGIN_NAME}_artefacts/Release"
AU_BUNDLE="${PLUGIN_OUT}/AU/${PLUGIN_NAME}.component"
VST3_BUNDLE="${PLUGIN_OUT}/VST3/${PLUGIN_NAME}.vst3"

[ -d "${AU_BUNDLE}" ] || { echo "ERROR: AU bundle not found at ${AU_BUNDLE}"; exit 1; }
[ -d "${VST3_BUNDLE}" ] || { echo "ERROR: VST3 bundle not found at ${VST3_BUNDLE}"; exit 1; }

# Sanity: confirm universal
echo "==> AU archs: $(lipo -archs "${AU_BUNDLE}/Contents/MacOS/${PLUGIN_NAME}")"
echo "==> VST3 archs: $(lipo -archs "${VST3_BUNDLE}/Contents/MacOS/${PLUGIN_NAME}")"

# ============ 3. CODESIGN ============
echo "==> [3/6] Code-signing bundles (hardened runtime + timestamp)..."
for bundle in "${AU_BUNDLE}" "${VST3_BUNDLE}"; do
    codesign --force --deep --options runtime --timestamp \
             --sign "${APP_CERT}" "${bundle}"
    codesign --verify --deep --strict --verbose=2 "${bundle}"
done

# ============ 4. PKG ============
echo "==> [4/6] Building installer package..."
rm -rf "${STAGE_DIR}"

# Component pkg for AU
mkdir -p "${STAGE_DIR}/au_root/Library/Audio/Plug-Ins/Components"
cp -R "${AU_BUNDLE}" "${STAGE_DIR}/au_root/Library/Audio/Plug-Ins/Components/"
pkgbuild --root "${STAGE_DIR}/au_root" \
         --identifier "${BUNDLE_ID}.au" \
         --version "${VERSION}" \
         --install-location "/" \
         "${STAGE_DIR}/${PLUGIN_NAME}_AU.pkg"

# Component pkg for VST3
mkdir -p "${STAGE_DIR}/vst3_root/Library/Audio/Plug-Ins/VST3"
cp -R "${VST3_BUNDLE}" "${STAGE_DIR}/vst3_root/Library/Audio/Plug-Ins/VST3/"
pkgbuild --root "${STAGE_DIR}/vst3_root" \
         --identifier "${BUNDLE_ID}.vst3" \
         --version "${VERSION}" \
         --install-location "/" \
         "${STAGE_DIR}/${PLUGIN_NAME}_VST3.pkg"

# Distribution xml
cat > "${STAGE_DIR}/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>${PLUGIN_NAME}</title>
    <organization>fr.deadloop</organization>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="default">
            <line choice="${BUNDLE_ID}.au"/>
            <line choice="${BUNDLE_ID}.vst3"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="${BUNDLE_ID}.au" visible="false">
        <pkg-ref id="${BUNDLE_ID}.au"/>
    </choice>
    <choice id="${BUNDLE_ID}.vst3" visible="false">
        <pkg-ref id="${BUNDLE_ID}.vst3"/>
    </choice>
    <pkg-ref id="${BUNDLE_ID}.au" version="${VERSION}" onConclusion="none">${PLUGIN_NAME}_AU.pkg</pkg-ref>
    <pkg-ref id="${BUNDLE_ID}.vst3" version="${VERSION}" onConclusion="none">${PLUGIN_NAME}_VST3.pkg</pkg-ref>
</installer-gui-script>
EOF

PKG_OUT="${ARTIFACTS_DIR}/${PLUGIN_NAME}_v${VERSION}_macOS.pkg"
productbuild --distribution "${STAGE_DIR}/distribution.xml" \
             --package-path "${STAGE_DIR}" \
             --sign "${INST_CERT}" \
             "${PKG_OUT}"

echo "==> Signed pkg: ${PKG_OUT}"

# ============ 5. NOTARIZE ============
echo "==> [5/6] Submitting to Apple notary service (this can take 1-10 min)..."
xcrun notarytool submit "${PKG_OUT}" \
    --keychain-profile "${NOTARY_PROFILE}" \
    --wait

# ============ 6. STAPLE ============
echo "==> [6/6] Stapling notarization ticket..."
xcrun stapler staple "${PKG_OUT}"
xcrun stapler validate "${PKG_OUT}"

# ============ FINAL CHECKS ============
echo ""
echo "============ FINAL VERIFICATION ============"
echo "--- spctl (Gatekeeper) ---"
spctl -a -vvv -t install "${PKG_OUT}" || true
echo ""
echo "--- pkg signature ---"
pkgutil --check-signature "${PKG_OUT}" || true
echo ""
echo "==> DONE: ${PKG_OUT}"
echo "==> Size: $(du -h "${PKG_OUT}" | cut -f1)"
