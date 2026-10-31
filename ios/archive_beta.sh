#!/bin/bash
# Local signed archive only. Upload/review/release remain separate owner actions.
set -euo pipefail
if [[ "${1:-}" == "--help" ]]; then
  echo 'Uso: CIELO_TEAM_ID=ABCDEFGHIJ CIELO_BUNDLE_ID=es.tuempresa.cielo CIELO_BUILD_NUMBER=3 bash ios/archive_beta.sh'
  echo 'Requiere un Mac con Xcode 26+, cuenta y firma ya configuradas en Xcode. No sube ni publica la app.'
  exit 0
fi
TEAM="${CIELO_TEAM_ID:-}"
BUNDLE="${CIELO_BUNDLE_ID:-}"
BUILD="${CIELO_BUILD_NUMBER:-}"
[[ "$TEAM" =~ ^[A-Z0-9]{10}$ ]] || { echo 'Falta CIELO_TEAM_ID válido (10 letras mayúsculas/dígitos).'; exit 2; }
[[ "$BUNDLE" =~ ^[A-Za-z][A-Za-z0-9-]*(\.[A-Za-z0-9-]+)+$ ]] || { echo 'Falta CIELO_BUNDLE_ID definitivo, sin comodines.'; exit 2; }
[[ "$BUILD" =~ ^[1-9][0-9]*$ ]] || { echo 'Falta CIELO_BUILD_NUMBER positivo y nuevo para App Store Connect.'; exit 2; }
ROOT=$(cd "$(dirname "$0")" && pwd)
command -v xcodebuild >/dev/null || { echo 'Este paso requiere Xcode moderno; no se ejecuta en el iMac Sierra.'; exit 2; }
MAJOR=$(xcodebuild -version | awk '/^Xcode / { split($2,part,"."); print part[1] }')
[[ "$MAJOR" -ge 26 ]] || { echo 'Se requiere Xcode 26 o posterior. Verifica los requisitos de Apple antes del envío.'; exit 2; }
OUTPUT="$ROOT/../dist/IOS_FIRMA_${BUILD}"
[[ ! -e "$OUTPUT" ]] || { echo 'La carpeta de esta compilación ya existe. Usa un número de compilación nuevo.'; exit 2; }
python3 "$ROOT/generate_project.py"
mkdir -p "$OUTPUT"
xcodebuild -project "$ROOT/Cielo.xcodeproj" -scheme CIELO -configuration Release \
  -destination 'generic/platform=iOS' -archivePath "$OUTPUT/CIELO.xcarchive" \
  DEVELOPMENT_TEAM="$TEAM" PRODUCT_BUNDLE_IDENTIFIER="$BUNDLE" \
  CURRENT_PROJECT_VERSION="$BUILD" CODE_SIGN_STYLE=Automatic archive
# No -allowProvisioningUpdates: use signing assets configured by the owner in Xcode.
codesign --verify --deep --strict "$OUTPUT/CIELO.xcarchive/Products/Applications/CIELO.app"
echo "Archivo firmado verificado: $OUTPUT/CIELO.xcarchive"
echo 'Ábrelo en Xcode Organizer, valida y distribuye por TestFlight cuando las pruebas físicas estén cerradas.'
