#!/bin/bash
# Fabrique l'installeur Mac avec l'AAX signé PACE (Chènevières Comp), sur le MacBook, clé iLok branchée.
# Dans le Terminal :  bash ~/Chenevieres-Comp-macOS-kit/tools/package-mac.sh  (kit dans le dossier personnel, PAS sur le Bureau iCloud)
# Étapes : 1. signature PACE de l'AAX (wraptool, avec le certificat Developer ID Application)
#          2. installeur .pkg signé (Developer ID Installer)   3. notarisation Apple et agrafage
# Il faut une fois : Xcode Command Line Tools, outils PACE pour Mac, et le profil de notarisation (proposé ci-dessous).
set -euo pipefail

KIT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="$(tr -d '[:space:]' < "$KIT/VERSION.txt")"
APP_ID="Developer ID Application: Mathieu CHOCAT (WF2H8TGRPY)"
INST_ID="Developer ID Installer: Mathieu CHOCAT (WF2H8TGRPY)"
ACCOUNT="infomatchoc@gmail.com"
WCGUID="DCD91EF0-CE23-11F1-898A-00505692C25A"
PROFILE="chenevieres-notary"
AAX="$KIT/build/ChenevieresComp_artefacts/Release/AAX/Chenevieres Comp.aaxplugin"

red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
stop()  { red "$*"; exit 1; }

echo "Chènevières Comp $VERSION : signature de l'AAX, installeur, notarisation"
echo

# 0. Vérifications
case "$KIT" in
  "$HOME/Desktop"*|"$HOME/Documents"*|*"Mobile Documents"*)
    stop "Le kit est dans un dossier synchronisé par iCloud (Bureau/Documents) : la signature Apple échouerait.
Glissez le dossier Chenevieres-Comp-macOS-kit dans votre dossier personnel, puis :  bash ~/Chenevieres-Comp-macOS-kit/tools/package-mac.sh" ;;
esac
xattr -cr "$KIT" 2>/dev/null || true   # fichiers téléchargés : on retire la quarantaine
command -v xcrun >/dev/null || stop "Outils Xcode absents : tapez  xcode-select --install  puis relancez."
security find-identity -v | grep -q "$APP_ID"  || stop "Certificat introuvable dans le trousseau : $APP_ID"
security find-identity -v | grep -q "$INST_ID" || stop "Certificat introuvable dans le trousseau : $INST_ID"
WRAPTOOL="$(ls -d /Applications/PACEAntiPiracy/Eden/Fusion/Versions/*/bin/wraptool 2>/dev/null | sort | tail -1 || true)"
[ -x "$WRAPTOOL" ] || stop "wraptool introuvable : installez d'abord PACECodeSigningForAAXSDK pour Mac."
[ -d "$AAX" ] || stop "AAX introuvable dans le kit : $AAX"
echo "wraptool : $WRAPTOOL"

if ! xcrun notarytool history --keychain-profile "$PROFILE" >/dev/null 2>&1; then
    echo
    echo "Première fois : enregistrement de la clé de notarisation dans le trousseau (une seule fois)."
    read -r -p "Glissez ici le fichier AuthKey_39QFK78P7Z.p8, puis Entrée : " P8
    P8="${P8%\"}"; P8="${P8#\"}"; P8="$(echo "$P8" | sed "s/^'//; s/'$//; s/\\\\ / /g; s/ *$//")"
    xcrun notarytool store-credentials "$PROFILE" --key "$P8" --key-id 39QFK78P7Z \
        --issuer 64cc255a-19a0-4f8e-af88-d86b65ca9394 || stop "Enregistrement de la clé impossible."
fi

# 1. Signature PACE
echo
read -r -s -p "Mot de passe iLok (laisser vide s'il a déjà été mémorisé par wraptool) : " PW; echo
ARGS=(sign --verbose --account "$ACCOUNT" --wcguid "$WCGUID" --signid "$APP_ID" --in "$AAX" --out "$AAX" --autoinstall on)
[ -n "$PW" ] && ARGS+=(--password "$PW")
# --dsigharden : runtime renforcé, exigé par la notarisation d'Apple
if ! "$WRAPTOOL" "${ARGS[@]}" --dsigharden; then
    echo "(nouvel essai sans --dsigharden)"
    "$WRAPTOOL" "${ARGS[@]}" || { PW=""; stop "Échec de la signature PACE. Envoyez une capture (sans mot de passe)."; }
fi
PW=""
"$WRAPTOOL" verify --verbose --in "$AAX" || stop "La vérification de la signature PACE a échoué."
codesign --verify --deep --strict "$AAX" || stop "La signature Apple de l'AAX n'est pas valide."
green "AAX signé (PACE + Developer ID)."

# 2. Installeur
echo
AAX_PACE_SIGNED=1 SIGN_APP="$APP_ID" SIGN_INSTALLER="$INST_ID" bash "$KIT/installer/macos/build_pkg.sh" "$VERSION"
PKG="$KIT/dist/Chenevieres-Comp-$VERSION-macOS.pkg"
[ -f "$PKG" ] || stop "Installeur non fabriqué."

# 3. Notarisation
echo
echo "Envoi à Apple pour la notarisation (quelques minutes)…"
LOG="$(mktemp)"
xcrun notarytool submit "$PKG" --keychain-profile "$PROFILE" --wait --timeout 30m | tee "$LOG"
if ! grep -q "status: Accepted" "$LOG"; then
    ID="$(grep -m1 '  id:' "$LOG" | awk '{print $2}')"
    [ -n "$ID" ] && xcrun notarytool log "$ID" --keychain-profile "$PROFILE" || true
    stop "Notarisation refusée : envoyez une capture de ce texte."
fi
xcrun stapler staple "$PKG"
spctl --assess --type install -v "$PKG"
echo
green "PRÊT : $PKG"
echo "C'est ce fichier qu'il faut joindre à la Release (à la place de celui fabriqué par GitHub)."
open -R "$PKG"
