#!/bin/bash
# Bash 3.2 and macOS Sierra: no Homebrew, venv, pip or system Python changes.
CIELO_PYTHON=""
for candidate in "$(command -v python3.9 2>/dev/null)" /Library/Frameworks/Python.framework/Versions/3.9/bin/python3.9 /usr/local/bin/python3.9 "$(command -v python3 2>/dev/null)"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
        if "$candidate" -c 'import sys,ssl,sqlite3; assert sys.version_info[:2] == (3,9); assert ssl.OPENSSL_VERSION_INFO >= (1,1,1)' >/dev/null 2>&1; then
            CIELO_PYTHON="$candidate"
            break
        fi
    fi
done
if [ -z "$CIELO_PYTHON" ]; then
    echo "No se localiza Python 3.9 con SSL compatible en este iMac."
    echo "No se modifica el vigilante actual. No uses el Python 2.7 de macOS."
    return 1
fi
export PYTHONIOENCODING=utf-8
return 0
