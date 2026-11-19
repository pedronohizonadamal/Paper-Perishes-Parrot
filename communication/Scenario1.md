# Prueba CAN A/B — base para el proyecto Parrot

Esta entrega implementa la fase de comunicación y diagnóstico entre dos Arduino
UNO R3 con CAN-BUS Shield Seeed/MCP2515. No implementa todavía la ráfaga defensiva
de Parrot ni el nodo atacante C. Los roles siguen el documento del proyecto:

| Placa | Función | Identificador propio |
|---|---|---|
| A | Motor simulado; consume la distancia y publica su estado | 0x220 |
| B | Sensor de distancia simulado; propietario del ID del sensor | 0x120 |
| C, futura fase | Nodo suplantador | Sin firmware en esta entrega |

No hace falta un motor ni un sensor físico. A muestra RUN/STOP por el monitor
serie y no activa pines de potencia. La distancia de B recorre una tabla con un
contador de secuencia: 250, 200, 100, 50, 25, 50, 100, 200 cm, y vuelve a empezar.
Se publica una muestra cada 250 ms. A indica STOP cuando la distancia es <=50 cm.

## 1. Carga de los programas

1. Descomprime el paquete entero. No abras los `.ino` desde dentro del ZIP.
2. Instala **Arduino AVR Boards** en el gestor de placas si todavía no está.
3. Utiliza la biblioteca Seeed **CAN_BUS_Shield 2.3.4**. Se incluye su ZIP original
   en `dependencies/Seeed_Arduino_CAN-2.3.4.zip`, con su licencia MIT.
   En el IDE: **Sketch/Programa > Incluir biblioteca > Añadir biblioteca .ZIP**.
   Si ya tienes instalada esa misma versión, no la instales por duplicado.
4. Abre `firmware/A_Motor/A_Motor.ino`, selecciona **Arduino Uno** y el puerto de A,
   y pulsa **Subir**. Conserva los dos `.h` junto al `.ino`.
5. Abre `firmware/B_Sensor/B_Sensor.ino`, selecciona el puerto de B y pulsa **Subir**.
6. Abre los monitores serie correspondientes a **115200 baudios**. Para observar
   ambos a la vez puedes usar dos ventanas del IDE con un puerto diferente en cada una.
   Abrir un monitor puede reiniciar el UNO; el protocolo admite ese reinicio.

Los sketches son autocontenidos: no debes instalar `shared/` como biblioteca.
Los `.hex` de `compiled/` son resultados de la compilación verificada, sin
bootloader. La vía recomendada es subir los `.ino`; no pulses «Grabar bootloader».

### Configuración del hardware

En la pestaña `CanLabNode.h` de **cada sketch** están los ajustes:

```cpp
const uint8_t CAN_CS = 9;
const uint8_t CAN_CLOCK = MCP_16MHz;
const uint32_t CAN_SPEED = CAN_500KBPS;
```

Estos son los valores de la prueba base. Si el ejemplo Seeed que ya os funciona
usa otro CS, oscilador o bitrate, copiad esos valores en los dos sketches antes de
subirlos. El oscilador es el del shield MCP2515, no el reloj del procesador UNO.
Si cambiáis el bitrate, el mensaje de arranque que dice «500k» es solo una etiqueta:
actualizadlo también para no confundir los registros del experimento.

El programa mantiene D10 como salida para que el UNO continúe siendo maestro SPI,
y deselecciona la microSD poniendo D4 en HIGH. No usa D13 como LED: es SCK de SPI.
La SD no se utiliza ni debe acceder a SPI desde otra tarea durante esta prueba.

### Cableado

CANH(A) con CANH(B), CANL(A) con CANL(B), GND(A) con GND(B). Mantén la configuración
física que ya funciona con el ejemplo Seeed: dos terminaciones de 120 ohmios,
una en cada extremo, sin añadir otras. Con ambas placas completamente sin
alimentación, el multímetro entre CANH y CANL debería indicar aproximadamente
60 ohmios. Interruptor PWR_OBD en OFF y DB9 libre si usáis la bornera.
Alimenta cada placa por USB o por la fuente adecuada para el UNO. No unas los
5V/VIN de fuentes independientes.

## 2. Qué protocolo se ha implementado

Es un **protocolo de aplicación propio sobre CAN clásico**, no una implementación
de CANopen, J1939 ni ISO-TP. Usa tramas de datos estándar, ID de 11 bits, DLC=8 y
500 kbit/s. El MCP2515 genera/verifica CRC, inserta y elimina stuff bits, arbitra,
emite ACK/error frames y aplica los contadores de error y retransmisiones de CAN.
El programa no fabrica CRC ni error frames por software ni altera esas reglas.

El ID identifica el tipo de mensaje y su propietario acordado; no es una dirección
de destino ni una identidad autenticada. B tiene prioridad sobre A (0x120 < 0x220).
La periodicidad y el tratamiento de muestras se definen aquí, por encima de CAN.

### Trama de B: SENSOR, ID=0x120

| Byte(s) | Campo | Codificación |
|---|---|---|
| 0 | Versión | 0x01 |
| 1–2 | Contador de secuencia | uint16, byte alto primero |
| 3–4 | Distancia simulada | uint16, cm, byte alto primero; 0..5000 |
| 5 | Estado del sensor | 0=correcto, 1=fallo de sensor |
| 6–7 | Sesión de B | uint16, byte alto primero, distinta de cero |

Ejemplo de la muestra secuencia 0, distancia 250 cm, sesión 1:

```text
ID=0x120 DLC=8 DATA=01 00 00 00 FA 00 00 01
```

### Trama de A: MOTOR_STATUS, ID=0x220

| Byte(s) | Campo | Codificación |
|---|---|---|
| 0 | Versión | 0x01 |
| 1–2 | Secuencia procesada | Eco de la secuencia de B |
| 3–4 | Distancia procesada | Eco de la distancia de B |
| 5 | Estado del motor simulado | 0=STOP, 1=RUN, 2=STOP por fallo de sensor |
| 6–7 | Sesión procesada | Eco de la sesión de B |

A responde a cada muestra nueva aceptada, incluso a una muestra que declara fallo
del sensor (en ese caso el motor se detiene). B comprueba sesión, secuencia,
distancia y estado antes de contabilizar la confirmación. En esta prueba de baja
carga solo admite confirmaciones de su última muestra publicada.

**Dos confirmaciones diferentes:** `tx_ok` observa que el controlador terminó una
transmisión sin aborto/error indicado; el ACK de CAN significa que algún receptor
recibió una trama correcta. `APP_ACK` significa que A respondió con el resultado
de procesamiento. Ninguno de los dos autentica a A. `queued` solo indica que la
trama fue puesta en el buffer, no que llegó al otro nodo.

### Secuencias, sesión y frescura

- El contador da la vuelta de 65535 a 0. Se usa comparación modular con ventana
  de media escala: un salto de 32768 o más se considera atrasado/ambiguo.
- A cuenta huecos de secuencia como muestras perdidas, descarta duplicados y
  muestras atrasadas. Estos mensajes no renuevan el temporizador de frescura.
- Cada arranque de B incrementa una sesión de diagnóstico guardada en los bytes
  EEPROM 0 y 1. A acepta una sesión diferente como resincronización. Se reservan
  esos bytes; la escritura no es atómica ante un corte de energía, ni incorpora
  desgaste distribuido. No reinicies miles de veces en pruebas automatizadas.
- La sesión tiene 16 bits, se recicla y **no es un nonce criptográfico**. Un cambio
  de sesión se acepta sin autenticación: un atacante puede abusar de ello.
- Si A no acepta muestras nuevas durante 1250 ms, muestra `SENSOR_TIMEOUT` y STOP.
  La comprobación es en el bucle principal; hay latencia adicional de ejecución.
- B indica `APPLICATION_ACK_TIMEOUT` si no recibe confirmaciones nuevas durante
  1250 ms, también si nunca llegó la primera. Una confirmación duplicada no refresca
  el watchdog. B continúa publicando para permitir recuperar el enlace.
- Una nueva muestra válida recupera el funcionamiento. Si contiene fallo del sensor,
  recupera el enlace pero mantiene la parada del motor.

Se elige **última muestra disponible**, sin reenvíos de aplicación que acumulen
datos antiguos: cada 250 ms llega una muestra nueva. CAN reintenta automáticamente
la transmisión pendiente según sus reglas, hasta que el firmware solicita un
aborto controlado al alcanzar 100 ms. Una muestra que agota ese plazo no se considera
entregada. Esto no es una cola fiable para órdenes que deban ejecutarse exactamente
una vez; para comandos de ese tipo habría que diseñar otra semántica.

## 3. Tratamiento de errores y observación

| Situación | Comportamiento |
|---|---|
| Error al iniciar el shield | Reintento de inicialización cada 2 s; revisar CS, cristal y montaje |
| DLC distinto de 8, versión desconocida, rango/flags inválidos | Rechazo y contador `bad`; no renuevan frescura |
| Tramas extendidas o RTR | Rechazadas por software; no se interpretan como medidas |
| ID ajeno al protocolo | Ignorado; no renueva frescura |
| Duplicado / muestra antigua | Rechazo; contadores `duplicate` / `old` |
| Hueco de secuencia | A acepta la muestra nueva y acumula el hueco en `missing` |
| Sensor declara fallo | A muestra STOP_SENSOR_FAULT y responde estado 2 |
| Falta de muestras | A se detiene por timeout |
| Falta de confirmación de aplicación | B notifica timeout y continúa publicando |
| Buffer RX desbordado | Registra `RX_OVERFLOW`; cuenta observaciones, no número exacto de tramas perdidas |
| Envío pendiente >=100 ms | Solicita ABAT al controlador; no reutiliza el buffer hasta ver TXREQ libre |
| Warning / error-passive / bus-off observado | Registra EFLG, TEC y REC y el estado correspondiente |
| Bus-off observado | Bloquea nuevas transmisiones de aplicación y mantiene A en STOP; `r` reinicializa |

Se comprueban los registros aproximadamente cada 10 ms y se imprime un resumen
cada 2 s. EFLG: 0x01 warning, 0x02 RX warning, 0x04 TX warning, 0x08 RX passive,
0x10 TX passive, 0x20 bus-off, 0x40 RX0 overflow, 0x80 RX1 overflow.

Los umbrales son warning a partir de 96; error-passive a partir de 128 en TEC o
REC; bus-off cuando TEC supera 255. El contador TEC de 8 bits no puede mostrar
el número 256: se identifica bus-off mediante EFLG.TXBO.

**Límites de la telemetría:** son muestras de registros, no un analizador a nivel
de bit. No proporcionan un contador exacto de errores CRC, ACK, bit, stuff o form.
Pueden perder transiciones muy breves. No atribuyas una colisión ni un ataque solo
a una subida de TEC o a un error-passive. MLOA (pérdida de arbitraje) no significa
por sí misma error del protocolo.

El MCP2515 puede salir de bus-off automáticamente después de la secuencia de
recuperación del bus. `halted` es un bloqueo de la aplicación si se llega a observar
TXBO; no impide la recuperación interna ni convierte el aislamiento en permanente.
Una recuperación rápida puede ocurrir entre muestras. No se reinicia el controlador
automáticamente tras un error de ejecución: eso borraría evidencia de TEC/REC.
La orden explícita `r` sí borra contadores del controlador; los totales de software
se conservan hasta reiniciar el UNO.

## 4. Pruebas paso a paso

Introduce las letras en el monitor serie; pulsa Enviar. Los finales de línea se
ignoran. `h` muestra ayuda y `s` el resumen en cualquiera de las dos placas.

### T1 — Funcionamiento normal

Sube A y B y alimenta ambos. A debe alternar `MOTOR RUN` y `MOTOR STOP` siguiendo
las distancias; B debe recibir `APP_ACK seq=...`. Por ejemplo:

```text
A: MOTOR RUN cm=250 seq=0
B: APP_ACK seq=0 motor=1
A: MOTOR RUN cm=200 seq=1
A: MOTOR RUN cm=100 seq=2
A: MOTOR STOP cm=50 seq=3
A: MOTOR STOP cm=25 seq=4
```

Cada 2 s: los contadores queued/tx_ok/rx deben crecer. En condiciones normales,
bad/old/duplicate/missing/overflow/own_id permanecen en cero y TEC/REC cerca de 0.
El ejemplo de consola es ilustrativo; no es una captura de vuestras placas.

### T2 — Desaparición de B sin tocar los cables

En B envía `p`. Deja los nodos alimentados: se detienen solo las muestras de B.
Después de aproximadamente 1,25 s A muestra `SENSOR_TIMEOUT -> MOTOR STOP`.
Envía `p` otra vez: A recupera el enlace con la siguiente muestra nueva.

### T3 — Sensor averiado

En B envía `i`: las muestras declaran fallo. A debe mostrar STOP_SENSOR_FAULT,
y B recibirá confirmaciones con `motor=2`. Envía `i` otra vez para volver a normal.

### T4 — Duplicado, muestra atrasada, longitud errónea

Después de al menos una muestra, envía `p` a B y espera 250 ms para que termine
el último envío. Después envía **una** de estas letras:

| Letra en B | Trama de prueba | Resultado esperado en A |
|---|---|---|
| `d` | Repite exactamente la última muestra | `duplicate` aumenta; no renueva frescura |
| `o` | Repite con secuencia anterior | `old` aumenta; no renueva frescura |
| `m` | DLC=3 en vez de 8 | `BAD_DLC` y `bad` aumenta |
| `z` | DLC=8, todos los bytes 0 | `ZERO_RESERVED`, contador `zero`, motor STOP |

Es normal que A ya haya entrado en timeout si tardas más de 1,25 s en enviar la
prueba. Para repetir desde normal, envía `p` para reanudar y luego vuelve a pausar.
Si B dice `TEST_FRAME_BUSY_OR_HALTED`, la trama no se puso en cola; vuelve a probar
cuando esté libre. Estos ensayos son errores de aplicación con tramas CAN válidas:
no son inyecciones de CRC incorrecto, error frames ni una prueba completa de Parrot.

### T5 — Reinicio de B

Con B publicando, pulsa RESET en B. A debe detectar cambio de sesión y aceptar
la nueva secuencia desde 0. Si pasa suficiente tiempo, puede parar antes por timeout.

### T6 — Falta de receptor/ACK

La prueba menos ambigua es dejar A sin programa receptor o apagar A dejando B
alimentado, manteniendo el cableado/terminaciones del banco. Observa B: falta de
APP_ACK, plazos TX agotados y posibles incrementos de TEC. Desconectar un nodo
puede alterar también las condiciones eléctricas del bus, por lo que no aísla
perfectamente un fallo de ACK.

**No se exige alcanzar bus-off**: errores de ACK sin otro nodo pueden dejar al
controlador en error-passive según las excepciones de CAN. Si se observa bus-off,
la aplicación queda detenida hasta `r`. Reanuda A y, si B quedó halted, envía `r`.

### T7 — Futuro nodo C

No está incluido aún. Un nodo C que emita una trama de sensor válida con ID 0x120,
sesión/secuencia plausibles y distancia falsa puede influir en A. B registra
`OWN_ID_RX` porque ha recibido un ID de su propiedad que no ha enviado él.
En modo normal, el MCP2515 no entrega sus propias transmisiones como recepción:
este detector presupone que no se han añadido loopback ni puentes que hagan eco.

## 5. Preparación para Parrot, según Dagan y Wool (2016)

Referencia leída: *Parrot, a software-only anti-spoofing defense system for the
CAN bus*, Tsvika Dagan y Avishai Wool, PDF proporcionado, especialmente pp. 4–9.

Mapeo del artículo: **B = Alice (propietaria/defensora)**, **A = Bob (receptor
víctima)**, **C = Eve (suplantadora)**. No confundir los nombres Alice/Bob del
paper con las letras A/B del proyecto. El propietario del ID suplantado es B.

La base mantiene estos requisitos de diseño:

1. B recibe tráfico de todos los IDs, incluido el suyo. No se aplica un filtro de
   hardware que oculte 0x120 a su propio propietario.
2. Tramas de medida siempre de 8 bytes. Una futura Dmessage utilizará el mismo
   ID **y DLC** que la trama suplantada, con el campo de datos completamente a cero.
3. El bloque de ocho ceros está reservado y A lo trata como advertencia/parada,
   sin interpretar esos bytes como una distancia legítima ni renovar frescura.
   Esto es una decisión conservadora para el simulador, no la defensa de Parrot.
4. Las sesiones, contadores y confirmaciones no se presentan como autenticación.
   No se añaden MACs ni una lista de IDs supuestamente segura que oculte la
   vulnerabilidad que el proyecto quiere estudiar.
5. TEC, REC y EFLG son observables y no se borran mediante reinicios periódicos.

La defensa **pendiente** es el algoritmo 1 de la p. 5: detectar un ID propio
recibido, lanzar una ráfaga de Dmessages a máxima velocidad, detectar la condición
de colisión/error-passive en el contexto de ese episodio y completar la fase con
15 Dmessages adicionales. La longitud ND de la ráfaga debe cubrir el intervalo
esperado entre tramas del atacante. El paper admite que la primera trama de
suplantación puede llegar a la víctima antes de que se active la defensa.

No basta con retransmitir ceros cada 1 ms. En las pp. 7–9 se muestra que incluso
un controlador con huecos de 31 microsegundos a 1 Mbit/s no consiguió completar
la defensa; el éxito exigió huecos mínimos o un vecino asistente. El artículo no
ensaya el UNO+MCP2515: no prueba que nuestro hardware lo logre ni que sea imposible.

Para esa fase habrá que sustituir el camino de transmisión de baja carga: el
firmware base usa un solo buffer, deadline de 100 ms y mensajes de consola. Eso
NO está diseñado para saturación. Una defensa experimental requerirá mantener
buffers de transmisión preparados, silenciar el logging durante la ráfaga,
conservar retransmisiones automáticas y medir los huecos reales. No declares éxito
por contar envíos ni por observar error-passive; verifica en **C** la transición
a bus-off y la posible recuperación automática del MCP2515. Un osciloscopio o
analizador CAN permite medir lo que no demuestra el monitor serie.

El paper usa 1 Mbit/s; la base usa 500 kbit/s para conservar el ejemplo inicial.
Antes de comparar resultados, fija el mismo bitrate en todos los nodos y expresa
los huecos en tiempos de bit. El objetivo inmediato es validar T1–T5 antes de
introducir C y después la variante B con defensa.

## 6. Validación y dependencias

Consulta `VALIDACION.md` para los resultados exactos. Compilación/enlace para
ATmega328P y pruebas de la lógica realizados; **no se ha flasheado ni ejecutado
este código en vuestras placas**. Las pruebas con I/O simulado no reproducen
arbitraje, colisiones, error frames, bus-off físico ni tiempos eléctricos.

Código de terceros de Seeed fijado al commit:
`cb607da7da84d3ebf375948b70e5a909b80f2f8f` (CAN_BUS_Shield 2.3.4).

Fuentes:
- https://github.com/Seeed-Studio/Seeed_Arduino_CAN
- https://wiki.seeedstudio.com/CAN-BUS_Shield_V2.0/#getting-started
- Microchip MCP2515, DS20001801J, sección 6 (error detection/error modes).
  https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf
- Documento de roles del proyecto:
  https://docs.google.com/document/d/1C4rOFEHWQoEUT2c0K90r2e1RQ8vZcaOj9Dis14VWY3M/edit

## 7. Repetir las pruebas de lógica

Desde la carpeta raíz del paquete, con Python 3 y un compilador C++:

```sh
python3 tools/sync_headers.py
c++ -std=c++11 -Wall -Wextra -Werror -DNODE_ROLE_A=1 -Itests/stubs tests/test_node.cpp -o /tmp/can-test-a
/tmp/can-test-a
c++ -std=c++11 -Wall -Wextra -Werror -DNODE_ROLE_A=0 -Itests/stubs tests/test_node.cpp -o /tmp/can-test-b
/tmp/can-test-b
```

`shared/` es la fuente común para desarrollo. Si la editas, ejecuta
`tools/sync_headers.py` para actualizar las copias dentro de ambos sketches.
Para recompilar normalmente con Arduino CLI ya configurado:

```sh
arduino-cli compile --fqbn arduino:avr:uno firmware/A_Motor
arduino-cli compile --fqbn arduino:avr:uno firmware/B_Sensor
```

Se incluye además `tools/build_avr.py` para compilar/enlazar con rutas explícitas
al toolchain oficial AVR, al core Arduino y a la biblioteca Seeed. No descarga ni
instala dependencias y no realiza ninguna operación de flasheo.
