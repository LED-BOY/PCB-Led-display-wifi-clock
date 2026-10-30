# GemaLight · Reloj LED Wi-Fi sobre PCB

Un reloj de cuatro dígitos que utiliza la translucidez del FR4 como pantalla. Un ESP32-C3 controla los LED mediante charlieplexing y muestra la hora y la temperatura exterior. Se configura desde el navegador e incluye actualizaciones de firmware por Wi-Fi.

[English](README.md) · **Español**

[Descargar firmware](https://github.com/LED-BOY/PCB-Led-display-wifi-clock/releases/tag/v3.6) · [Montaje](docs/HARDWARE.md) · [Instalación](docs/FIRMWARE.md) · [Configuración](docs/SETUP.md)

<p align="center">
  <img src="https://github.com/user-attachments/assets/0c8ef493-8c9e-4351-bac5-b09b48d5e743" alt="Reloj GemaLight con pantalla de PCB translúcido" width="900">
</p>

## Funciones

- Hora por NTP al iniciar y cada hora después de una sincronización correcta.
- Temperatura exterior de Open-Meteo según la ubicación aproximada de la IP pública. No utiliza un sensor de temperatura integrado.
- Brillo automático según amanecer y atardecer; horario nocturno de respaldo de 20:00 a 08:00.
- Brillo manual Bajo/Alto y opción Boost.
- Modo sin internet con ajuste manual de la hora.
- Portal de configuración, prueba de segmentos y actualización inalámbrica.
- Sincronización en segundo plano: la versión 3.6 mantiene el refresco de la pantalla durante las consultas de red.

## Primer uso

1. Monta la PCB y un ESP32-C3 Super Mini con la distribución de pines correcta. Consulta la [guía de hardware](docs/HARDWARE.md).
2. [Instala el firmware](docs/FIRMWARE.md). El archivo **merged.bin** es para la instalación inicial por USB; **ino.bin** es para actualizar desde el portal.
3. Enciende o reinicia el reloj y conéctate a **ClockSetup** con la contraseña **12345678**.
4. Si el portal no se abre solo, visita [192.168.4.1](http://192.168.4.1/).
5. Introduce una red Wi-Fi de **2,4 GHz**, elige las opciones y pulsa **Save**. El reloj se reinicia.

El portal está disponible durante 60 segundos al arrancar. Si se conecta un dispositivo, la ventana se amplía a 10 minutos. Para abrirlo de nuevo, reinicia el reloj.

## Botón e indicadores

| Situación | Comportamiento |
| --- | --- |
| BOOT durante el portal | Sale del portal y continúa el arranque |
| BOOT en modo automático, sin una sincronización en curso | Solicita una sincronización inmediata |
| BOOT durante una sincronización | La pulsación se consume; no se acumulan solicitudes |
| BOOT en modo manual | Alterna entre brillo Bajo y Alto |
| Dos puntos encendidos | La última conexión Wi-Fi terminó correctamente; no garantiza éxito de NTP o del clima |
| Dos puntos apagados | Modo manual o última conexión Wi-Fi fallida |

La hora y la temperatura alternan cada 10 segundos cuando la opción de temperatura está activa y hay datos recientes. La temperatura caduca después de dos horas sin actualizarse. El reloj no incluye una garantía de conservación de hora tras perder alimentación; en modo manual debes comprobarla después de reiniciarlo.

## Archivos y documentación

| Recurso | Contenido |
| --- | --- |
| [Código Arduino](Source/GemaLight_ESP_LED_clock_3.6/) | Sketch 3.6 con todos sus archivos auxiliares |
| [Montaje de PCB](PCB%20assembly/) | Gerbers, listas de materiales y orientación de LED |
| [Modelos STL](STL%20files/) | Carcasa, difusor y soporte |
| [Esquema v1.7](schematic%20v1.7.png) | Referencia del circuito |
| [Firmware](docs/FIRMWARE.md) | Parámetros de Arduino y métodos de carga, en inglés |
| [Configuración](docs/SETUP.md) | Opciones y solución de problemas, en inglés |
| [Historial](CHANGELOG.md) | Versiones y cambios del repositorio |

## Versión 3.6

Una tarea FreeRTOS gestiona las conexiones de red. NTP y clima conservan resultados separados y reintentan tras 1, 5 y 15 minutos cuando fallan. Un fallo de sincronización no reinicia un reloj que ya está funcionando. Consulta las [notas técnicas](Source/README-3.6.md) y la [lista de comprobaciones en hardware](docs/VALIDATION.md).

## Autor y licencia

Proyecto de [LED-BOY](https://github.com/LED-BOY), basado en Arduino-ESP32 y datos meteorológicos de Open-Meteo. El repositorio todavía no declara una licencia. Consulta al autor sobre permisos de reutilización o distribución del código, PCB, modelos y fotografías.
