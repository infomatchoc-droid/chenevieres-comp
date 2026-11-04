#!/bin/bash
# Construit l'installateur macOS (.pkg) : VST3 + AU + AAX (Pro Tools)
set -euo pipefail

VERSION="${1:-1.0.0}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
ART="$ROOT/build/ChenevieresComp_artefacts/Release"
WORK="$ROOT/build/pkg"
OUT="$ROOT/dist"
NAME="Chenevieres Comp"

rm -rf "$WORK" && mkdir -p "$WORK/vst3" "$WORK/au" "$OUT"
cp -R "$ART/VST3/$NAME.vst3" "$WORK/vst3/"
cp -R "$ART/AU/$NAME.component" "$WORK/au/"

# Signature Apple (variables posées par le build GitHub quand les certificats sont là) :
#   SIGN_APP       = « Developer ID Application: … »  (plugins)
#   SIGN_INSTALLER = « Developer ID Installer: … »    (.pkg)
# Sans elles : signature ad hoc, comme avant.
SIGN_APP="${SIGN_APP:-}"
SIGN_INSTALLER="${SIGN_INSTALLER:-}"

# AAX : signature Apple ici ; la signature PACE (wraptool) pour Pro Tools « normal » viendra plus tard
HAS_AAX=0
if [ -d "$ART/AAX/$NAME.aaxplugin" ]; then
    HAS_AAX=1
    mkdir -p "$WORK/aax"
    cp -R "$ART/AAX/$NAME.aaxplugin" "$WORK/aax/"
    if [ "${AAX_PACE_SIGNED:-0}" = 1 ]; then
        :   # déjà signé par wraptool (PACE + Developer ID, kit Mac) : surtout ne pas re-signer
    elif [ -n "$SIGN_APP" ]; then
        codesign --force --deep --options runtime --timestamp -s "$SIGN_APP" "$WORK/aax/$NAME.aaxplugin"
    else
        codesign --force --deep -s - "$WORK/aax/$NAME.aaxplugin"
    fi
fi

make_component () { # $1 = dossier racine, $2 = identifiant, $3 = destination, $4 = paquet
    pkgbuild --analyze --root "$1" "$WORK/$2.plist"
    # Toujours installer au bon endroit, même si une ancienne copie existe ailleurs
    plutil -replace 0.BundleIsRelocatable -bool NO "$WORK/$2.plist"
    pkgbuild --root "$1" --component-plist "$WORK/$2.plist" \
             --identifier "fr.chenevieres.comp.$2" --version "$VERSION" \
             --install-location "$3" "$WORK/$4"
}

make_component "$WORK/vst3" vst3 "/Library/Audio/Plug-Ins/VST3"       vst3.pkg
make_component "$WORK/au"   au   "/Library/Audio/Plug-Ins/Components" au.pkg

if [ "$HAS_AAX" = 1 ]; then
    make_component "$WORK/aax" aax "/Library/Application Support/Avid/Audio/Plug-Ins" aax.pkg
    AAX_LINE='<line choice="aax"/>'
    AAX_CHOICE='<choice id="aax" title="AAX (Pro Tools)" description="/Library/Application Support/Avid/Audio/Plug-Ins"><pkg-ref id="fr.chenevieres.comp.aax"/></choice>'
    AAX_REF='<pkg-ref id="fr.chenevieres.comp.aax">aax.pkg</pkg-ref>'
else
    AAX_LINE=""; AAX_CHOICE=""; AAX_REF=""
fi

cat > "$WORK/distribution.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Chènevières Comp $VERSION</title>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <domains enable_localSystem="true"/>
    <background file="background.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
    <background-darkAqua file="background-dark.png" alignment="bottomleft" scaling="none" mime-type="image/png"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        $AAX_LINE
    </choices-outline>
    <choice id="vst3" title="VST3" description="/Library/Audio/Plug-Ins/VST3">
        <pkg-ref id="fr.chenevieres.comp.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit (Logic, GarageBand…)" description="/Library/Audio/Plug-Ins/Components">
        <pkg-ref id="fr.chenevieres.comp.au"/>
    </choice>
    $AAX_CHOICE
    <pkg-ref id="fr.chenevieres.comp.vst3">vst3.pkg</pkg-ref>
    <pkg-ref id="fr.chenevieres.comp.au">au.pkg</pkg-ref>
    $AAX_REF
</installer-gui-script>
EOF

SIGN_ARGS=()
if [ -n "$SIGN_INSTALLER" ]; then SIGN_ARGS=(--sign "$SIGN_INSTALLER" --timestamp); fi
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" --resources "$ROOT/installer/macos/resources" "${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"}" \
             "$OUT/Chenevieres-Comp-$VERSION-macOS.pkg"

echo "OK : $OUT/Chenevieres-Comp-$VERSION-macOS.pkg"
