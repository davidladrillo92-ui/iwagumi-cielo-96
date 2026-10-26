#!/bin/bash
# macOS Sierra / Bash3.2. Never opens a USB serial port.
task_root="$(cd "$(dirname "$0")" && pwd)"
if . "$task_root/cielo_python.sh"; then
    "$CIELO_PYTHON" "$task_root/cielo_backup_sierra.py" --directory "$task_root/RESULTADOS_PRIVADOS"
    task_rc=$?
else
    task_rc=1
fi
echo
read -r -p "Pulsa Intro para cerrar. " task_reply
exit "$task_rc"
