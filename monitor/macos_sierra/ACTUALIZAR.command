#!/bin/bash
cd "$(dirname "$0")" || exit 1
. ./cielo_python.sh || { read -r -p "Pulsa INTRO para cerrar..." z; exit 1; }
"$CIELO_PYTHON" cielo_observer.py update
res=$?
echo
read -r -p "Pulsa INTRO para cerrar..." z
exit "$res"
