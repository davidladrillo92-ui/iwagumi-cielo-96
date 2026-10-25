# RECOVERY3.1 — Bloque 2: recuperación selectiva Chihiros

## Problema confirmado
En RECOVERY3, un estado SDK de conexión pendiente de Chihiros puede contaminar el snapshot de recuperación aunque la tarea BLE real siga viva. El supervisor reinició todo el ATOM al alcanzar 120 s, apagando temporalmente Pez Verde. No sabemos aún qué callback falló.

## Implementación candidata
La política `hold_ch_only` permite aislar temporalmente SOLO Chihiros si se cumplen TODAS estas condiciones:
- SDK de Chihiros pendiente entre 15 y menos de 300 segundos; Pez Verde no tiene conexión SDK pendiente.
- Tarea BLE viva: heartbeat y fase menores de 15 segundos.
- No existe obligación de apagado de Pez Verde.
- No hay OTA ni modo rescate en curso y BLE está preparado.

Mientras dure el aislamiento:
- Se suspenden nuevas órdenes, retornos automáticos y reconexiones Chihiros.
- Pez Verde sigue siendo atendida por la misma tarea BLE.
- No se destruyen objetos GATT ni se fuerza un cierre de cliente con callback pendiente.
- El supervisor recibe las edades REALES del worker, no la edad sintética de la solicitud Chihiros.
- Cuando desaparece el pendiente, se desbloquea la programación y se fuerza la reevaluación de la consigna Chihiros.
- Si el worker deja de responder, hay pendiente de PV, aparece obligación OFF, se entra en OTA/rescate, o la espera llega a 300 s, vuelve a aplicar el supervisor global de 120 s.

## Diagnóstico
Eventos `CH_AISLAMIENTO_INICIO` / `CH_AISLAMIENTO_FIN`, estado `bluetooth.blockage_diagnostic.selective_ch_hold`, episodios y plazo de respaldo.

## Seguridad y limitaciones
Esto NO repara un callback perdido: evita reiniciar todo el ATOM mientras la solicitud pendiente esté aislada y el worker continúe sano. Si el SDK no se limpia por sí mismo, se mantiene el reinicio global como último recurso. El aislamiento no garantiza que Chihiros haya recuperado luz ni modo My Chihiros. No se cambian las curvas V5, el protocolo de tramas, el interlock OFF de PV ni las particiones OTA.

Se necesitan pruebas de compilación, escenarios de callback tardío, funcionamiento nocturno, desconexión física y comprobación óptica antes de instalar. Es una propuesta experimental y no está instalada.
