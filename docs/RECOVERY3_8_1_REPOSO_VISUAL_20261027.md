# RECOVERY3.8.1 — enlace conectado y luz en reposo

La captura IMG_7982 y el Informe90 muestran que el punto ámbar junto a En reposo parece una desconexión. El reporte90 registra enlace BLE conectado,15/15 GATT,0 fallos y can_start=true. La discrepancia es visual.

Se cambia el texto a «Conectado · en reposo» y el punto ámbar a una luna sobre fondo verde suave. La marca de GATT reciente sigue siendo un check distinto. El tooltip y aria-label distinguen explícitamente enlace BLE conectado, último OFF aceptado y siguiente comprobación programada, sin confirmar luz óptica ni inventar un GATT reciente. Misma representación en Centro.

Solo se muestra cuando el gate3.8 de reposo confirmado está activo y la UI tiene datos actuales, enlace conectado y ningún error. Desconexión, fallos, datos HTTP antiguos y simulación no muestran reposo conectado. No se modifica ninguna lógica de control, seguridad, verificación de mantenimiento, temporización, motor o almacenamiento3.8; main.cpp cambia solo VERSION. Se conserva la gen10/38PV/32CH instalada.

Prueba DOM: gate de reposo y su apariencia, errores, enlace perdido, datos antiguos y vista previa. Suite heredada y compilación real del ATOMS3 Lite en Actions. Pendiente comprobar visualmente en el iPhone tras instalar. El reporte90 valida3.8, no esta revisión visual3.8.1.
