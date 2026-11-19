# Firmware A/B de depuración CAN

Los sketches alternativos son:

- `firmware_A_debug/firmware_A_debug.ino`
- `firmware_B_debug/firmware_B_debug.ino`

No sustituyen a `A_Motor` y `B_Sensor`. Reutilizan su máquina de estados con
dos opciones de compilación: B publica cada 10.000 ms y ambos nodos consideran
el enlace obsoleto tras 50.000 ms. El segundo valor mantiene la relación original
de cinco periodos antes del timeout; sin ese cambio, A se detendría 1,25 s después
de cada muestra y permanecería parada durante la mayor parte de los diez segundos.

El monitor serie continúa a 115200 baudios. Para cada transmisión y recepción
se muestra:

- instante y dirección (`TX_REQUEST`, `TX_RESULT` o `RX_DECODED`);
- formato estándar/extendido y tipo data/RTR;
- SOF, identificador hexadecimal y sus bits;
- RTR, IDE, r0, DLC y bits del DLC;
- datos en hexadecimal y binario;
- CRC-15 reconstruido para tramas estándar;
- secuencia reconstruida desde SOF hasta CRC con bit stuffing;
- delimitador CRC, ACK, delimitador ACK, EOF e intermission;
- tras TX: resultado, interpretación del ACK y registros TX0CTRL, EFLG, TEC y REC.

## Límite de la observación

`readMsgBuf()` entrega al ATmega328P una trama que el MCP2515 ya ha validado y
de la que ya ha retirado los bits de stuffing. El MCP2515 tampoco entrega al
programa una captura del ACK slot ni de la forma de onda CANH/CANL. Por eso los
campos CRC y stuffing impresos son una reconstrucción determinista de la trama
lógica, no una captura física. `ACK=dominant_observed` se infiere de que TX terminó
correctamente; para observar niveles, tiempos de bit, errores y ACK reales hace
falta un osciloscopio o analizador CAN.

La reconstrucción completa se implementa para las tramas estándar de 11 bits que
usa el proyecto. Las tramas extendidas se identifican y muestran, pero se marcan
como no soportadas para reconstrucción CRC/stuffing.

## Validación local

Ambas variantes se compilaron para Arduino UNO/ATmega328P con el toolchain AVR,
`-Wall -Wextra -Werror` y la biblioteca Seeed fijada por el proyecto. El resultado
fue 14.012 bytes de flash y 476 bytes de RAM estática para A_debug, y 14.550 bytes
de flash y 501 bytes de RAM estática para B_debug. También se verificó que la
trama de ejemplo `0x120: 01 00 00 00 FA 00 00 01` reconstruye CRC-15 `0x4622`,
que B no publica antes de 10.000 ms y que sí pone en cola la muestra al alcanzar
ese instante. Esta validación no reemplaza una prueba en las placas ni una captura
del bus físico.
