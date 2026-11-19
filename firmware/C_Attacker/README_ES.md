# Nodo C - ataque de suplantación preparado para Parrot

Este firmware convierte el tercer Arduino en **C/Eve**, un nodo atacante para el
bus aislado del laboratorio. B es Alice, la propietaria legítima del ID `0x120`,
y A es Bob, la víctima que interpreta la distancia.

## Ataque implementado

1. C escucha una trama SENSOR legítima de B.
2. Aprende automáticamente la sesión y la secuencia observada.
3. Espera 400 ms y transmite una trama CAN estándar legal con ID `0x120`, DLC 8,
   la misma sesión, la secuencia siguiente y una distancia falsa.
4. Continúa falsificando una trama cada 1.000 ms. Por defecto anuncia 250 cm para
   forzar `RUN`; la orden serie `t` cambia a 25 cm para forzar `STOP`, y `u` vuelve
   a 250 cm.

El payload falso respeta el protocolo de aplicación y no es cero. Sin defensa,
A no puede distinguirlo de B y debería aceptarlo. Como la trama es válida, ACK,
CRC y bit stuffing los gestiona el MCP2515 normalmente y TEC/REC deberían seguir
cerca de cero.

## Por qué es protegible por Parrot

El ataque conserva las condiciones del modelo de Dagan y Wool:

- C suplanta un ID que pertenece a B.
- B recibe una trama con su propio ID que no transmitió y puede detectarla.
- C utiliza un controlador CAN normal, sin alterar sus reglas de bajo nivel.
- El ID y el DLC coinciden con la futura Dmessage defensiva de B.
- Los datos falsos son no nulos, por lo que una Dmessage de ocho ceros difiere en
  el campo de datos y puede provocar el bit error descrito por el paper.
- C deja activada la retransmisión automática y no aborta una TX pendiente. Esto
  permite que una colisión inicial produzca las retransmisiones necesarias para
  el efecto snowball de Parrot.

La primera falsificación puede llegar a A antes de que B active la defensa; es la
naturaleza de segundo golpe reconocida por el paper.

## Montaje y ejecución

- C usa CS D9, cristal MCP2515 de 16 MHz, CAN clásico a 500 kbit/s y serie a
  115200 baudios.
- Conecta C a CANH, CANL y GND del mismo bus. Mantén **sólo dos terminaciones de
  120 ohmios en los extremos físicos**; no añadas una tercera terminación por ser
  el tercer nodo.
- Sube `C_Attacker.ino` a C y arranca A y B. C no ataca hasta observar una trama
  válida de B y aprender su sesión.

Con Parrot todavía desactivado, la evidencia esperada es:

- C: `TARGET_SYNC` seguido de `SPOOF QUEUED` y `SPOOF TX_OK`.
- B: `OWN_ID_RX: possible spoof; monitor only, Parrot OFF`.
- A: acepta las secuencias falsas y muestra `MOTOR RUN cm=250` por defecto.
- C: `EFLG=0`, `TEC=0`, `REC=0`, porque el ataque usa tramas CAN válidas.

## Órdenes serie de C

| Orden | Acción |
|---|---|
| `h` | Ayuda |
| `s` | Estadísticas y registros MCP2515 |
| `a` | Activar/desactivar el ataque |
| `u` | Falsificar 250 cm, solicitando RUN |
| `t` | Falsificar 25 cm, solicitando STOP |
| `r` | Reinicializar MCP2515 y desactivar el ataque; borra TEC/REC |

## Evidencia futura de Parrot

C sondea `EFLG` cada 1 ms. Si observa `TXBO`, registra
`ATTACK_CONTAINED_AT_CONTROLLER`, conserva un contador y deja de encolar mientras
el controlador siga en bus-off. No aplica un latch artificial permanente: si el
MCP2515 se recupera según las reglas CAN, lo registra y el ataque puede reanudarse.
Así se distingue aislamiento real del controlador de una simple decisión de la
aplicación atacante.

No se debe declarar éxito de Parrot sólo porque B detecte `OWN_ID_RX`, porque A
reciba ceros o porque C deje de imprimir. La evidencia mínima será: colisiones y
crecimiento de TEC, transición de C a error-passive y `TXBO`, supervivencia de B,
STOP seguro en A y comprobación de cualquier recuperación posterior de C.

## Validación local

El sketch se compiló y enlazó para Arduino UNO/ATmega328P con el toolchain AVR,
`-Wall -Wextra -Werror` y la biblioteca Seeed fijada por el proyecto. Ocupa
11.718 bytes de flash y 435 bytes de RAM estática. Una prueba con MCP2515 simulado
verificó que C aprende `sesión=0x31, secuencia=41`, espera hasta 400 ms después,
genera la falsificación `secuencia=42, distancia=250`, conserva ID `0x120` y DLC 8,
y registra tanto la entrada en `TXBO` como una recuperación posterior. Esta prueba
no demuestra todavía el ataque ni Parrot sobre el bus físico.

Además se ejecutó el modelo digital Parrot existente con ID `0x120`, DLC 8 y el
payload no nulo `01 00 2A 00 FA 00 00 31`: sin defensa entregó 29 falsificaciones
y Eve permaneció `ERROR_ACTIVE`; con defensa sólo se entregó la primera y Eve
alcanzó `BUS_OFF`. Ese modelo ideal funciona a 1 Mbit/s y no representa los tiempos
del UNO/MCP2515 a 500 kbit/s, por lo que valida la compatibilidad conceptual del
ataque con Parrot, no el resultado futuro en hardware.
