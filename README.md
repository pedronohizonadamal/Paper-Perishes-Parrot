# Paper-Perishes-Parrot

Firmware base: `firmware/A_Motor` y `firmware/B_Sensor`.

Firmware alternativo de depuración: `firmware/firmware_A_debug` y
`firmware/firmware_B_debug`. Consulta `firmware/DEBUG_CAN_ES.md` para el formato
del volcado CAN y sus límites frente a una captura física del bus.

Nodo atacante C para el experimento previo a Parrot: `firmware/C_Attacker`.
Su guía explica el modelo de amenaza, los comandos serie y la evidencia que debe
recogerse antes y después de activar la defensa.
=======
CAN frame:

field 0: SoF (start of frame), 1 bit, 0 dominant (bit stuffing starts)

field 1: Abitration Field (ID), 11 bits

field 2: RTR (remote transmission request), 1 bit, 0->transmit, 1->request

field 3: Control Field, 6 bits: IEB (1 bit extends ID to 29 bits on recessive), r (reserved bit, future use, keep dominant), DLC (data length code) 4 bits for data byte length (up to 8 bytes)

field 4: Data field, 0-64 bits

field 5: CRC field (cyclic redundancy check), 15 bits->CRC, 1 bit->DEL (bit stuffing ends)

field 6: ACK (acknowledgement), 1 bit ACK (Recessive when sent, Dominant after read), 1 bit DEL

field 7: EOF (end of frame), 7 consecutive recessive bits

field "8": Intermission field, 3 recessive bits between frames

