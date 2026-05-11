# Actividad 2 — Ascensor inteligente ACME S.A.

**Equipos e Instrumentación Electrónica** · Pablo Javier Aneas Torres

Continuación de la Actividad 1, esta vez centrada en el producto final: un ascensor de cinco plantas con panel de llamada, control de temperatura e iluminación, y supervisión de presencia de usuario.

Simulación WOKWI: https://wokwi.com/projects/463711656113033217

## Hardware

El sistema reutiliza la base sensorial de la Actividad 1 (DHT22, fotorresistor, ultrasonido HC-SR04, RTC DS1307, LCD 20x4 I²C) y añade:

- 5 pulsadores para la llamada de planta (uno por planta), con pull-up interno
- 1 servomotor para simular el desplazamiento de la cabina
- 5 LEDs indicadores de planta actual
- 1 LED amarillo para representar la luz artificial de cabina

<img width="1206" height="926" alt="image" src="https://github.com/user-attachments/assets/24963718-5e12-4ca7-a9ad-709c73949778" />

## Decisiones de diseño

**Pulsadores en lugar de mando IR.** Se probó inicialmente con un receptor IR y mando virtual, pero las librerías `IRremote` y `Servo` entran en conflicto en Arduino Uno porque ambas usan el mismo timer del ATmega328. Cambiar a pulsadores físicos resuelve el problema y además es más realista para un panel de ascensor.

**Servomotor frente a paso a paso.** Para un movimiento posicional discreto entre cinco puntos fijos el servo es la opción más simple: un único pin PWM, sin driver auxiliar, sin alimentación externa para una carga simulada. Cada planta tiene asignado un ángulo (0°, 45°, 90°, 135°, 180°) y se mueve grado a grado con 15 ms entre pasos para un efecto gradual.

**Control de temperatura: ON-OFF con zona muerta.** Setpoint 25 °C, margen de ±2 °C. Tres regiones: enfriar si T > 27, calentar si T < 23, zona muerta entre medias. El margen de ±2 °C es suficiente para evitar oscilaciones, por eso no se añade histéresis adicional sobre el algoritmo discontinuo.

**Control de iluminación: ON-OFF con histéresis.** Setpoint 80 %, histéresis de 5 puntos. La luz artificial se enciende por debajo del 75 % y se apaga por encima del 80 %. La histéresis evita el chattering típico cuando la lectura del LDR oscila en torno al umbral. Se ha preferido a la variante con 8 LEDs y registro 74HC595 propuesta opcionalmente porque consume menos pines (necesarios para los pulsadores) y representa mejor el comportamiento real de una luz de cabina.

## Detalles de implementación

- **Anti-rebote por software:** debounce de 50 ms, detección solo de flancos de bajada. Evita que una pulsación se registre varias veces.
- **Timeout en el ultrasonido:** `pulseIn` con 30 ms de timeout para que el loop no quede bloqueado si no hay eco.
- **Lectura válida del DHT22:** flag `lecturaValida` que bloquea los algoritmos de control hasta tener la primera lectura coherente, evitando que arranque en modo CALENTAR por la inicialización a 0.0.
- **Loop no bloqueante:** la lectura de pulsadores es continua, los sensores se leen cada 2 s y el LCD rota cada 4 s, usando `millis()` en lugar de `delay()`.

## LCD — tres pantallas rotando cada 4 segundos

```
Pantalla 0              Pantalla 1              Pantalla 2
─────────────────       ─────────────────       ─────────────────
ASCENSOR  Pta: 3        TEMPERATURA             CLIMA OPERACION
Destino:  Pta: 5        Anterior: 24.5°C        Humedad: 56.5 %
Estado:   MOVIENDO      Actual:   26.1°C        Ilumina: 72 %
Usuario:  SI            Control:  ENFRIANDO     Ctrl luz: ENCENDIDA
```

La pantalla 1 muestra la temperatura anterior junto a la actual, siguiendo la sugerencia simple del enunciado para que se vea el efecto del control.

## Ejemplos de logs

Fragmentos del monitor serie durante la ejecución de la simulación.

**Arranque del sistema y primera llamada de planta:**

```
Fecha y hora: 11/5/2026 [14:16:42]: Sistema ascensor ACME iniciado. Planta 1.
Pulsadores listos: P1=D3, P2=D12, P3=D13, P4=A2, P5=A3
Fecha y hora: 11/5/2026 [14:16:42]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: NO
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ENCENDIDA (luz baja)
Fecha y hora: 11/5/2026 [14:16:44]: Pulsador planta 5 presionado
Fecha y hora: 11/5/2026 [14:16:44]: Moviendo de planta 1 a planta 5
Fecha y hora: 11/5/2026 [14:16:46]: Llegado a planta 5
Fecha y hora: 11/5/2026 [14:16:46]: Planta: 5 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: SI
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
```

**Respuesta de los algoritmos de control al modificar temperatura y luminosidad:**

```
Fecha y hora: 11/5/2026 [14:17:9]: Sistema ascensor ACME iniciado. Planta 1.
Pulsadores listos: P1=D3, P2=D12, P3=D13, P4=A2, P5=A3
Fecha y hora: 11/5/2026 [14:17:9]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: NO
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ENCENDIDA (luz baja)
Fecha y hora: 11/5/2026 [14:17:11]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:13]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: SI
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:15]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: NO
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:17]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: NO
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:19]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:21]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 301.5 | Luz%: 30 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:23]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 100916.5 | Luz%: 100 | Usuario: SI
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: APAGADA (luz suficiente)
Fecha y hora: 11/5/2026 [14:17:25]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 100916.5 | Luz%: 100 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: OFF (manteniendo)
Fecha y hora: 11/5/2026 [14:17:27]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ENCENDIDA (luz baja)
Fecha y hora: 11/5/2026 [14:17:29]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:31]: Planta: 1 | T: 25.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ZONA MUERTA (OK)
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:33]: Planta: 1 | T: 80.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ENFRIANDO
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:35]: Planta: 1 | T: 80.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: ENFRIANDO
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:37]: Planta: 1 | T: -40.0 C | H: 60.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 1.90 cm
CTRL TEMP: CALENTANDO
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:39]: Planta: 1 | T: -40.0 C | H: 0.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: CALENTANDO
CTRL LUZ: ON (manteniendo)
Fecha y hora: 11/5/2026 [14:17:41]: Planta: 1 | T: -40.0 C | H: 100.0 % | Lux: 0.1 | Luz%: 0 | Usuario: SI
Presencia detectada en cabina. Distancia: 2.01 cm
CTRL TEMP: CALENTANDO
CTRL LUZ: ON (manteniendo)
```
