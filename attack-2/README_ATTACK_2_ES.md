# Attack 2: One-Shot + blinking reset contra Parrot

Esta variante conserva A y B exactamente como en `deffense` y modifica solo C.
C vuelve a suplantar a B con un payload SENSOR válido de 10 cm, pero cambia la
configuración y recuperación de su MCP2515 para romper el mecanismo snowball de
Parrot.

## Hipótesis atacada

Parrot presupone que el atacante no puede modificar el comportamiento de bajo
nivel del controlador CAN. Su confinamiento depende de que, después de la
primera colisión, los controladores retransmitan automáticamente la misma trama
y permanezcan sincronizados hasta alcanzar error-passive y bus-off.

El MCP2515 expone por SPI el bit `CANCTRL.OSM`. En modo One-Shot una trama se
intenta transmitir una sola vez, incluso si pierde arbitraje o es destruida por
un error. C utiliza este modo para convertir el snowball en una sola colisión
por intento.

Las colisiones independientes todavía aumentan el TEC de C. Para evitar que se
acumulen hasta bus-off, C aplica un blinking reset cuando `TEC >= 32`:

1. conserva el máximo TEC observado y el valor anterior al reset;
2. ejecuta `can.begin()`, que envía `MCP_RESET` y reinicializa TEC/REC;
3. restaura 500 kbit/s, modo normal, filtros y One-Shot;
4. reanuda el ataque periódico después de 120 ms.

El umbral 32 mantiene a C muy por debajo de error-passive (`TEC > 127`).

## Uso

1. Mantener A y B con sus firmwares de `deffense`.
2. Cargar `attack-2/C/C.ino` únicamente en C.
3. Abrir el monitor de C a 115200 baud.
4. Enviar `a` para activar/desactivar el ataque periódico.

Comandos adicionales:

- `1`: una única suplantación válida de 10 cm.
- `b`: activa/desactiva la evasión. Está activada por defecto.
- `s`: estadísticas y registros.

## Evidencia esperada

En C:

```text
CAN_READY 11bit 500k DLC8 OSM=ON
TX_FRAME id=0x120 cm=10
ANTI_PARROT_BLINK_RESET pre_TEC=32 count=1
ANTI_PARROT_BLINK_RECOVERED
EFLG=0x0 TEC=0 ... OSM=1
```

En B, cada intento de C puede provocar un error aislado, pero no debería
alcanzarse `parrot_collision=YES` ni `TEC >= 128` en un único snowball. Los
pulsos deberían terminar normalmente con `collision=NO`.

El ataque tiene éxito si C sigue transmitiendo durante varios ciclos, registra
resets preventivos, nunca muestra `CAN_BUS_OFF` y A sigue recibiendo muestras
falsas válidas de 10 cm cuando C encuentra ventanas fuera del pulso defensivo.

## Resultado verificado en hardware

Se cargó esta versión en el Arduino C (`/dev/cu.usbmodem11301`) y se observó
simultáneamente el Arduino B con Parrot. A y B no se reprogramaron.

Con la evasión activada se obtuvo, tras una ejecución sostenida:

```text
STATS C ... queued=400 tx_ok=114 tx_fail=229 tx_custom=400
             peak_TEC=39 anti_parrot=1 blink_resets=57
EFLG=0x0 TEC=0 REC=0 ... CANCTRL=0xF OSM=1
```

C completó 400 intentos, sufrió 229 transmisiones fallidas por la defensa y
ejecutó 57 resets preventivos. El máximo TEC observado fue 39; no alcanzó
error-passive ni BUS-OFF. B sí detectó repetidamente su propio ID y ejecutó la
defensa, pero todos los ciclos observados terminaron sin snowball:

```text
STATS B ... parrot_suspect=79 parrot_cycles=78
             parrot_collision=0 parrot_no_collision=78
             parrot_peak_tec=8
PARROT_COMPLETE collision=NO
```

Como control causal se envió `b`, desactivando únicamente One-Shot y el reset
preventivo, sin modificar el payload ni el intervalo. El mismo C pasó entonces
por error-passive y BUS-OFF:

```text
EFLG=0x15 TEC=223 ... CANCTRL=0x7 OSM=0
CAN_ERROR_PASSIVE
CAN_BUS_OFF
BUS_OFF
```

Después de la prueba de control se volvió a cargar el firmware para dejar C
con contadores limpios, evasión activada por defecto y ataque periódico parado.
El comando `a` lo vuelve a iniciar.

Los `RX_OVERFLOW` de C no invalidan el resultado: C acepta todas las tramas y
el tren de Dmessages de B puede desbordar sus dos buffers RX mientras el UNO
atiende SPI y Serial. El criterio de confinamiento está en TEC/EFLG/BUS-OFF,
no en el overflow de recepción.

## Base técnica y bibliografía

- El datasheet oficial del MCP2515 define `CANCTRL.OSM`: con el bit a uno el
  mensaje solo intenta transmitirse una vez; a cero se reintenta cuando sea
  necesario. También especifica que una trama interrumpida por error en
  One-Shot se aborta en vez de quedar en retransmisión automática:
  <https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Family-Data-Sheet-DS20001801K.pdf>.
- Cho y Shin muestran que el confinamiento BUS-OFF depende de la evolución de
  los contadores de error CAN y estudian ataques y recuperación basados en ese
  mecanismo: <https://rtcl.eecs.umich.edu/rtclweb/assets/publications/2016/ktcho_busoff_CCS_16.pdf>.
- CANOX analiza vulnerabilidades adicionales del manejo de errores CAN y el
  efecto de configuraciones y recuperaciones del controlador:
  <https://www.usenix.org/conference/usenixsecurity21/presentation/serag>.
- Dagan y Wool basan el efecto snowball de Parrot en las retransmisiones
  sincronizadas después de la colisión. Este experimento sale deliberadamente
  de su modelo de adversario al permitir que el firmware comprometido configure
  y reinicie por SPI un controlador CAN externo.

## Alcance

Este ataque no es compatible con el modelo de adversario original de Parrot:
deliberadamente demuestra que, en un diseño donde el firmware comprometido
puede configurar o reiniciar un controlador CAN externo por SPI, la suposición
de retransmisión inalterable deja de cumplirse.

