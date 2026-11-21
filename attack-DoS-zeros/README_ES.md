# Firmware con defensa Parrot en B

Esta carpeta conserva los firmwares A y C de `attack` sin cambios. La defensa
de Dagan y Wool se implementa exclusivamente en `B/CanLabNode.h` y está activa
por defecto.

## Funcionamiento

En modo normal B publica `0x120` cada 250 ms y valida los `APP_ACK` de A igual
que en `attack`. En modo normal del MCP2515, B no recibe sus propias tramas. Por
ello, recibir una trama de datos estándar con su ID `0x120` identifica una
suplantación realizada por otro controlador.

Después de detectar el primer spoof, B:

1. suspende su publicación normal;
2. aborta cualquier transmisión normal pendiente;
3. transmite hasta 700 Dmessages con ID `0x120`, el DLC observado y datos a
   cero;
4. utiliza simultáneamente TXB0, TXB1 y TXB2 para mantener el siguiente mensaje
   preparado y reducir al mínimo el espacio entre tramas;
5. detecta la colisión snowball mediante `TXEP` o `TEC >= 128`;
6. conserva el mensaje interceptado y transmite 15 Dmessages adicionales;
7. vuelve al funcionamiento normal.

El pulso de 700 mensajes cubre de forma conservadora unos 189 ms a 500 kbit/s,
por encima de los 120 ms entre suplantaciones periódicas de C.

## Comandos serie de B

- `h`: ayuda.
- `s`: estadísticas y registros MCP2515.
- `g`: activa o desactiva Parrot cuando no hay un pulso en curso.
- `r`: reinicializa CAN.
- `p`, `i`, `z`, `d`, `o`, `m`: conservan el comportamiento de `attack`.

Parrot arranca con `parrot_guard=1`. Durante el pulso B ignora comandos serie y
no imprime estadísticas, porque la latencia de Serial produciría huecos entre
Dmessages.

## Prueba física prevista

1. Cargar `deffense/A/A.ino`, `deffense/B/B.ino` y `deffense/C/C.ino`.
2. Encender A y B y comprobar el intercambio normal y los `APP_ACK`.
3. En C, enviar `a` para activar el ataque periódico.
4. En B deben aparecer `PARROT_ENGAGE`, `PARROT_PULSE_START` y finalmente
   `PARROT_COMPLETE collision=YES post=15`.
5. En las estadísticas de B se espera `parrot_collision>0` y
   `parrot_failed=0`.
6. En C se espera el crecimiento de TEC, la transición a error-passive y
   finalmente bus-off. Este último resultado depende del comportamiento físico
   y del tiempo real del MCP2515 y queda pendiente de validación en hardware.

Si B muestra `collision=NO`, el pulso terminó sin interceptar otra trama de C.
Si muestra `PARROT_FAILED defender_bus_off`, la defensa también llevó a B a
bus-off y habrá que revisar la continuidad temporal de la tubería en hardware.

