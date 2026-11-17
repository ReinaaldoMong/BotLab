/* =====================================================================
   LabEnergy · Mesa de medición en Wokwi (ESP32)
   ---------------------------------------------------------------------
   - Muestra en la pantalla OLED un código de 4 dígitos para emparejar
     el circuito con la página web.
   - Recibe por MQTT qué dispositivo eligió el usuario en la página y
     activa ese componente en el circuito.
   - Publica cada 500 ms la tensión U y la potencia P del dispositivo.
   - El potenciómetro regula la tensión entre 50 % y 100 % de la
     tensión nominal. La carga se modela como resistiva:
       R = Un² / Pn,   I = U / R,   P = U · I
   ===================================================================== */
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Servo.h>
#include "DHTesp.h"

// ---------- Red y MQTT (deben coincidir con la página) ----------
const char* WIFI_SSID    = "Wokwi-GUEST";
const char* WIFI_PASS    = "";
const char* MQTT_HOST    = "broker.hivemq.com";
const int   MQTT_PORT    = 1883;
const char* TOPIC_PREFIX = "labenergy-um";

// ---------- Pines ----------
const int PIN_BULB   = 26;  // Bombilla LED (LED amarillo)
const int PIN_ROUTER = 27;  // Router Wi-Fi (LED azul de actividad)
const int PIN_BUZZER = 25;  // Altavoz (buzzer)
const int PIN_SERVO  = 13;  // Ventilador (servo oscilante)
const int PIN_RING   = 15;  // Tira de luces RGB (anillo NeoPixel)
const int PIN_DHT    = 14;  // Sensor ambiental (DHT22)
const int PIN_POT    = 34;  // Potenciómetro (regulación de tensión)
const int RING_PIXELS = 12;

// ---------- Dispositivos (mismos valores de referencia que la página) ----------
struct Device { const char* key; const char* name; float un; float pn; int eta; };
Device DEVICES[] = {
  { "led",     "Bombilla LED",  12.0, 9.0, 88 },
  { "fan",     "Ventilador",     5.0, 4.0, 72 },
  { "speaker", "Altavoz",        5.0, 6.0, 81 },
  { "router",  "Router Wi-Fi",  12.0, 8.0, 90 },
  { "rgb",     "Tira RGB",       5.0, 3.6, 80 },
  { "sensor",  "Sensor amb.",    3.3, 0.5, 94 },
};
const int N_DEVICES = sizeof(DEVICES) / sizeof(DEVICES[0]);

WiFiClient net;
PubSubClient mqtt(net);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Adafruit_NeoPixel ring(RING_PIXELS, PIN_RING, NEO_GRB + NEO_KHZ800);
Servo fan;
DHTesp dht;

char code[5];
char topicCmd[48], topicData[48];
int current = -1;               // índice del dispositivo activo (-1 = ninguno)
float U = 0, P = 0, I = 0, k = 1;
float tempC = NAN, hum = NAN;
unsigned long tPublish = 0, tDht = 0, tServo = 0, tRing = 0, tBeep = 0, tRouter = 0;
int servoAngle = 0, servoStep = 3;
uint16_t hue = 0;

// ---------------------------------------------------------------------
void allOff() {
  digitalWrite(PIN_BULB, LOW);
  digitalWrite(PIN_ROUTER, LOW);
  noTone(PIN_BUZZER);
  ring.clear(); ring.show();
}

void onMessage(char* topic, byte* payload, unsigned int len) {
  char key[16] = {0};
  for (unsigned int i = 0; i < len && i < sizeof(key) - 1; i++) key[i] = (char)payload[i];
  int found = -1;
  for (int i = 0; i < N_DEVICES; i++) if (strcmp(key, DEVICES[i].key) == 0) found = i;
  if (found != current) { allOff(); current = found; }
}

void connectWifi() {
  Serial.print("WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS, 6);
  while (WiFi.status() != WL_CONNECTED) { delay(100); Serial.print("."); }
  Serial.println(" conectado");
}

void connectMqtt() {
  while (!mqtt.connected()) {
    String id = String("labenergy-esp32-") + code;
    Serial.print("MQTT...");
    if (mqtt.connect(id.c_str())) {
      Serial.println(" conectado");
      mqtt.subscribe(topicCmd);
    } else {
      Serial.printf(" fallo, estado %d\n", mqtt.state());
      delay(1000);
    }
  }
}

void drawScreen() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(0, 0);  oled.print("Codigo:");
  oled.setTextSize(2);
  oled.setCursor(52, 0); oled.print(code);
  oled.setTextSize(1);
  oled.setCursor(0, 20);
  oled.print(current >= 0 ? DEVICES[current].name : "Elige en la pagina");
  oled.setCursor(0, 32); oled.printf("U = %.2f V", U);
  oled.setCursor(0, 42); oled.printf("P = %.2f W", P);
  oled.setCursor(0, 52); oled.printf("I = %.3f A", I);
  if (current >= 0 && strcmp(DEVICES[current].key, "sensor") == 0 && !isnan(tempC)) {
    oled.setCursor(80, 32); oled.printf("%.1fC", tempC);
    oled.setCursor(80, 42); oled.printf("%.0f%%", hum);
  }
  oled.display();
}

// ---------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  pinMode(PIN_BULB, OUTPUT);
  pinMode(PIN_ROUTER, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  ring.begin(); ring.clear(); ring.show();
  fan.attach(PIN_SERVO);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  Wire.begin(21, 22);
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);

  snprintf(code, sizeof(code), "%04lu", (unsigned long)(esp_random() % 10000));
  snprintf(topicCmd, sizeof(topicCmd), "%s/%s/cmd", TOPIC_PREFIX, code);
  snprintf(topicData, sizeof(topicData), "%s/%s/data", TOPIC_PREFIX, code);

  oled.clearDisplay(); oled.setTextColor(SSD1306_WHITE); oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print("Conectando WiFi..."); oled.display();
  connectWifi();
  oled.clearDisplay(); oled.setCursor(0, 0); oled.print("Conectando MQTT..."); oled.display();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMessage);
  connectMqtt();
  Serial.printf("Listo. Codigo de emparejamiento: %s\n", code);
}

void loop() {
  if (!mqtt.connected()) connectMqtt();
  mqtt.loop();
  unsigned long now = millis();

  // Potenciómetro: 50 % a 100 % de la tensión nominal
  k = 0.5 + 0.5 * (analogRead(PIN_POT) / 4095.0);
  if (current >= 0) {
    Device& d = DEVICES[current];
    float R = d.un * d.un / d.pn;
    U = d.un * k;
    I = U / R;
    P = U * I;
  } else {
    U = P = I = 0;
  }

  // Actuadores según el dispositivo activo
  const char* key = current >= 0 ? DEVICES[current].key : "";
  if (strcmp(key, "led") == 0) digitalWrite(PIN_BULB, HIGH);
  if (strcmp(key, "fan") == 0 && now - tServo > 20) {          // ventilador oscilante
    tServo = now; servoAngle += servoStep;
    if (servoAngle >= 180 || servoAngle <= 0) servoStep = -servoStep;
    fan.write(servoAngle);
  }
  if (strcmp(key, "speaker") == 0 && now - tBeep > 700) {      // pitido periódico
    tBeep = now; tone(PIN_BUZZER, 440 + (int)(k * 440), 200);
  }
  if (strcmp(key, "router") == 0) digitalWrite(PIN_ROUTER, (now / 150) % 2); // actividad
  if (strcmp(key, "rgb") == 0 && now - tRing > 30) {           // arcoíris, brillo según tensión
    tRing = now; hue += 600;
    ring.setBrightness((uint8_t)(90 + 165 * (k - 0.5) * 2));
    for (int i = 0; i < RING_PIXELS; i++)
      ring.setPixelColor(i, ring.gamma32(ring.ColorHSV(hue + i * 65536L / RING_PIXELS)));
    ring.show();
  }
  if (now - tDht > 2000) {                                     // lectura del DHT22
    tDht = now;
    TempAndHumidity th = dht.getTempAndHumidity();
    tempC = th.temperature; hum = th.humidity;
  }

  // Publicar lectura cada 500 ms
  if (now - tPublish > 500) {
    tPublish = now;
    char json[160];
    snprintf(json, sizeof(json),
      "{\"device\":\"%s\",\"u\":%.2f,\"p\":%.2f,\"eta\":%d,\"pot\":%d,\"t\":%.1f,\"h\":%.0f}",
      current >= 0 ? key : "none", U, P, current >= 0 ? DEVICES[current].eta : 0,
      (int)(k * 100), isnan(tempC) ? 0.0 : tempC, isnan(hum) ? 0.0 : hum);
    mqtt.publish(topicData, json);
    drawScreen();
  }
}
