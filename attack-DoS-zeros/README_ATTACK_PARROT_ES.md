# Ataque de evasión contra Parrot

Esta variante conserva los firmwares A y B de `deffense` y modifica únicamente
el firmware de C. El ataque seleccionado es la evasión mediante payload de
ceros descrita en la sección 4.5.5 de la tesis de Mert Dieter Pesé, *Bringing
Practical Security to Vehicles* (2022).

## Por qué se ha escogido

Parrot responde a una suplantación transmitiendo un Dmessage con el mismo ID y
DLC que el atacante, pero con todo el campo de datos a cero. Con un payload
normal, los dos mensajes divergen en algún bit de datos y ambos controladores
entran en la cadena de errores y retransmisiones que finalmente lleva al
atacante a bus-off.

Si C transmite exactamente el mismo mensaje que el Dmessage defensivo:

```text
ID   = 0x120
DLC  = 8
DATA = 00 00 00 00 00 00 00 00
```

ambos controladores emiten los mismos campos, CRC y bits de stuffing. No existe
una diferencia de bits que pueda iniciar el snowball. El resultado esperado es
que el TEC de C permanezca en cero y que Parrot termine el pulso con
`collision=NO`.

En este laboratorio el ataque también tiene efecto sobre A, porque A trata una
trama `0x120` de ocho ceros como `ZERO_DATA` y adopta STOP. Es una evasión/DoS,
no una muestra SENSOR válida: no contiene versión, secuencia, distancia ni
sesión válidas.

## Uso

1. Cargar `attack-parrot/A/A.ino` en A.
2. Cargar `attack-parrot/B/B.ino` en B.
3. Cargar `attack-parrot/C/C.ino` en C.
4. Comprobar primero el funcionamiento normal de A y B.
5. En C, enviar `1` para una sola trama o `a` para activar/desactivar el ataque
   periódico cada 120 ms.

## Resultado esperado

En C:

```text
TX_ZERO_EVASION id=0x120 data=00x8
peak_TEC=0
EFLG_seen sin TXEP/TXBO
```

En B:

```text
PARROT_ENGAGE own_id=0x120
PARROT_PULSE_START
PARROT_COMPLETE collision=NO
```

En A:

```text
ZERO_DATA
MOTOR STOP ...
```

Los criterios de éxito son: C continúa transmitiendo, no entra en error-passive
ni bus-off, y B no registra una colisión Parrot. El aumento de REC u overflow en
C no equivale a bus-off; la métrica decisiva es TEC/EFLG.

## Alternativas descartadas por ahora

- **Parrot ofensivo:** puede elevar también el TEC de C y no ofrece una ventaja
  determinista frente al Dmessage dominante de B.
- **Congelar el contador de error:** TEC pertenece al MCP2515 y no puede fijarse
  desde el firmware normal; requeriría modificar o sustituir el controlador.
- **Reinicio o blinking antes de bus-off:** puede implementarse reinicializando
  el MCP2515 al alcanzar un umbral, pero introduce ventanas de desconexión y
  estudia recuperación del controlador, no la limitación fundamental de
  payload señalada para Parrot.

