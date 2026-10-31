#!/bin/bash
set -euo pipefail
KIND="${1:?iphone o ipad}"
DEVICE="${2:?UDID del simulador}"
case "$KIND" in
  iphone) STEM=CIELO_UI; DERIVED=/tmp/cielo-ios-simulator; CAPTURES=CIELO_UI_CAPTURAS ;;
  ipad) STEM=CIELO_IPAD_UI; DERIVED=/tmp/cielo-ios-ipad; CAPTURES=CIELO_IPAD_CAPTURAS ;;
  *) echo 'Familia de dispositivo no válida'; exit 2 ;;
esac
RESULT="/tmp/${STEM}.xcresult"
ATTACHMENTS="/tmp/${STEM}_attachments"
mkdir -p dist
TEST_STATUS=0
xcodebuild -project ios/Cielo.xcodeproj -scheme CIELO -configuration Debug -sdk iphonesimulator \
  -destination "id=$DEVICE" -parallel-testing-enabled NO -derivedDataPath "$DERIVED" \
  -resultBundlePath "$RESULT" CODE_SIGNING_ALLOWED=NO test || TEST_STATUS=$?
if [ -d "$RESULT" ]; then
  xcrun xcresulttool export attachments --path "$RESULT" --output-path "$ATTACHMENTS"
  python3 ios/export_ui_evidence.py "$ATTACHMENTS" "dist/$CAPTURES"
  xcrun xcresulttool get test-results summary --path "$RESULT" > "dist/${STEM}_RESUMEN.json"
  ditto -c -k --keepParent "$RESULT" "dist/${STEM}_RESULTADOS.zip"
  ditto -c -k --keepParent "dist/$CAPTURES" "dist/${CAPTURES}.zip"
fi
exit "$TEST_STATUS"
