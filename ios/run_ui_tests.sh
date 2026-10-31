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
# The normal audit remains strict. A separate OS-level XXXL run diagnoses scaling.
xcrun simctl ui "$DEVICE" content_size large
trap 'xcrun simctl ui "$DEVICE" content_size large >/dev/null 2>&1 || true' EXIT
LARGE_RESULT="/tmp/${STEM}_LARGE.xcresult"
LARGE_STATUS=0
xcrun simctl ui "$DEVICE" content_size accessibility-extra-extra-extra-large
xcodebuild -project ios/Cielo.xcodeproj -scheme CIELO -configuration Debug -sdk iphonesimulator \
  -test-timeouts-enabled YES -default-test-execution-time-allowance 180 -maximum-test-execution-time-allowance 240 \
  -destination "id=$DEVICE" -parallel-testing-enabled NO -derivedDataPath "$DERIVED" \
  -resultBundlePath "$LARGE_RESULT" -only-testing:CieloUITests/CieloUITests/testLargestSystemTextLayout \
  CODE_SIGNING_ALLOWED=NO test || LARGE_STATUS=$?
if [ -d "$LARGE_RESULT" ]; then
  xcrun xcresulttool export attachments --path "$LARGE_RESULT" --output-path "${ATTACHMENTS}_LARGE"
  python3 ios/export_ui_evidence.py "${ATTACHMENTS}_LARGE" "dist/${CAPTURES}_LARGE"
  xcrun xcresulttool get test-results summary --path "$LARGE_RESULT" > "dist/${STEM}_LARGE_RESUMEN.json"
  ditto -c -k --keepParent "$LARGE_RESULT" "dist/${STEM}_LARGE_RESULTADOS.zip"
  ditto -c -k --keepParent "dist/${CAPTURES}_LARGE" "dist/${CAPTURES}_LARGE.zip"
fi
xcrun simctl ui "$DEVICE" content_size large
TEST_STATUS=0
xcodebuild -project ios/Cielo.xcodeproj -scheme CIELO -configuration Debug -sdk iphonesimulator \
  -test-timeouts-enabled YES -default-test-execution-time-allowance 180 -maximum-test-execution-time-allowance 240 \
  -destination "id=$DEVICE" -parallel-testing-enabled NO -derivedDataPath "$DERIVED" \
  -resultBundlePath "$RESULT" -skip-testing:CieloUITests/CieloUITests/testLargestSystemTextLayout CODE_SIGNING_ALLOWED=NO test-without-building || TEST_STATUS=$?
if [ -d "$RESULT" ]; then
  xcrun xcresulttool export attachments --path "$RESULT" --output-path "$ATTACHMENTS"
  python3 ios/export_ui_evidence.py "$ATTACHMENTS" "dist/$CAPTURES"
  xcrun xcresulttool get test-results summary --path "$RESULT" > "dist/${STEM}_RESUMEN.json"
  ditto -c -k --keepParent "$RESULT" "dist/${STEM}_RESULTADOS.zip"
  ditto -c -k --keepParent "dist/$CAPTURES" "dist/${CAPTURES}.zip"
fi
if [ "$TEST_STATUS" -ne 0 ]; then exit "$TEST_STATUS"; fi
exit "$LARGE_STATUS"
