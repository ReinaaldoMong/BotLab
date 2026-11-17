# LabEnergy · Mesa de medición

Laboratorio digital interactivo para estudiar el **consumo eléctrico de dispositivos de baja tensión** en corriente continua.
El usuario elige un dispositivo, lo coloca en la mesa de medición y obtiene su tensión (U), potencia (P), eficiencia de referencia (η)
y la intensidad calculada con **I = P ÷ U**. Volti, una ampolleta animada, explica cada paso y cada valor.

Proyecto del ramo **Proyecto de Innovación Tecnológica** · Universidad Mayor · Ingeniería Civil en Computación e Informática.

## Enlaces

- **Página:** `https://reinaaldomong.github.io/BotLab/` (activar GitHub Pages: *Settings → Pages → Branch: main → Save*)
- **Simulación en Wokwi:** https://wokwi.com/projects/480966180230860801

## Cómo usarla

### Modo rápido (valores de referencia)
1. Abre la página.
2. Elige un dispositivo (clic o arrastrándolo a la mesa).
3. Presiona **Medir dispositivo** y revisa las lecturas.

### Modo en vivo (circuito en Wokwi)
1. En la página, haz clic en **Abrir simulación en Wokwi ↗** y presiona ▶ en Wokwi.
2. Espera el código de 4 dígitos en la pantalla OLED del circuito.
3. Escríbelo en la página y presiona **Conectar** (indicador verde = en vivo).
4. Deja Wokwi y la página **lado a lado**: si la pestaña de Wokwi queda oculta, el navegador pausa la simulación.
5. Elige un dispositivo: se activa en el circuito. Gira el potenciómetro para variar la tensión (50 %–100 % de la nominal).

Guía completa: [`docs/Manual_de_Usuario_LabEnergy.pdf`](docs/Manual_de_Usuario_LabEnergy.pdf)

## Estructura del repositorio

```
BotLab/
├── index.html              Página web (HTML + CSS + JavaScript, MQTT.js)
├── wokwi/
│   ├── sketch.ino          Código del ESP32 (C++ / Arduino)
│   ├── diagram.json        Circuito de Wokwi
│   └── libraries.txt       Librerías del proyecto en Wokwi
└── docs/
    ├── Manual_de_Usuario_LabEnergy.pdf
    ├── Informe_Tecnico_Avance_LabEnergy.pdf
    └── bitacoras/          Bitácoras N.º 1 a N.º 4
```

## Arquitectura

- **Captación simulada:** ESP32 en Wokwi con LED (bombilla), servomotor (ventilador), buzzer (altavoz), LED azul (router),
  anillo NeoPixel de 12 LED (tira RGB), DHT22 (sensor ambiental), potenciómetro y pantalla OLED.
  Cada dispositivo se modela como carga resistiva: `R = Un² / Pn`, `U = k · Un`, `P = U² / R`.
- **Comunicación:** MQTT sobre el broker público HiveMQ.
  - `labenergy-um/<código>/cmd` (página → ESP32): dispositivo elegido.
  - `labenergy-um/<código>/data` (ESP32 → página): JSON con `u`, `p`, `eta`, cada 500 ms.
- **Aplicación web:** catálogo de referencia, motor de cálculo `I = P ÷ U`, indicadores, estado de conexión y asistente Volti.
  Si no llegan datos en 6 s, conserva la última lectura y avisa que Wokwi está en pausa.

### Usar el circuito en Wokwi desde cero
Crear un proyecto ESP32 en wokwi.com, pegar `wokwi/sketch.ino` y `wokwi/diagram.json`, crear el archivo `libraries.txt` con el
contenido de `wokwi/libraries.txt`, guardar el proyecto y copiar su enlace en la constante `WOKWI_URL` de `index.html`.

## Equipo

| Integrante | Rol |
|---|---|
| Victor Pacheco | Product Owner |
| Isabella Simonetti | Responsable Científico |
| Reinaldo Monge | Hardware / Instrumentación |
| Matías Añasco | Hardware / Instrumentación |
| Sofía González | Software / Datos |
| Alejandro Queupán | UX/UI |
| Sofía Rodríguez | Pedagógico / Documentación |

> Simulación educativa. No conectes equipos reales a esta página.
