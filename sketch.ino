#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <RTClib.h>

#define DHT_PIN     2
#define DHT_TYPE    DHT22
#define LDR_PIN     A0
#define TRIG_PIN    7
#define ECHO_PIN    8
#define SERVO_PIN   9

// Pulsadores de llamada (uno por planta)
#define BTN_P1      3
#define BTN_P2      12
#define BTN_P3      13
#define BTN_P4      A2
#define BTN_P5      A3

// LEDs indicadores
#define LED_P1      4
#define LED_P2      5
#define LED_P3      6
#define LED_P4      10
#define LED_P5      11
#define LED_LUZ     A1

const float GAMMA = 0.7;
const float RL10  = 50;

// Angulos del servo para cada planta
const int ANGULO_PLANTA[5] = {0, 45, 90, 135, 180};
const int NUM_PLANTAS = 5;

// Umbral para presencia en cabina (cm)
const float UMBRAL_PRESENCIA = 30.0;

// Setpoints de control
const float TEMP_SETPOINT  = 25.0;
const float ZONA_MUERTA    = 2.0;
const float TEMP_MAX = TEMP_SETPOINT + ZONA_MUERTA;
const float TEMP_MIN = TEMP_SETPOINT - ZONA_MUERTA;
const int   LUZ_SETPOINT   = 80;
const int   LUZ_HISTERESIS = 5;

// Objetos de hardware
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(0x27, 20, 4);
RTC_DS1307 rtc;
Servo servoAscensor;

// Arrays utilidades
const int LED_PLANTA[5] = {LED_P1, LED_P2, LED_P3, LED_P4, LED_P5};
const int BTN_PLANTA[5] = {BTN_P1, BTN_P2, BTN_P3, BTN_P4, BTN_P5};

// Variables globales
int   plantaActual    = 1;
int   plantaDestino   = 1;
bool  enMovimiento    = false;
bool  hayUsuario      = false;
float tempActual      = 0.0;
float tempAnterior    = 0.0;
float humActual       = 0.0;
float luxActual       = 0.0;
int   pctLuz          = 0;
int   accionTemp      = 0;
bool  luzArtificialOn = false;
bool  lecturaValida   = false;

// Anti-rebote: ultimo estado leido y timestamp
bool btnEstadoPrev[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
unsigned long btnTUltimo[5] = {0, 0, 0, 0, 0};
const unsigned long DEBOUNCE_MS = 50;

unsigned long tUltimaLectura = 0;
unsigned long tUltimoLCD     = 0;
int pantallaLCD = 0;

// ===== SETUP =====
void setup() {
  Serial.begin(9600);

  pinMode(LDR_PIN, INPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Pulsadores con resistencia interna pull-up
  for (int i = 0; i < NUM_PLANTAS; i++) {
    pinMode(BTN_PLANTA[i], INPUT_PULLUP);
  }

  // LEDs
  for (int i = 0; i < NUM_PLANTAS; i++) {
    pinMode(LED_PLANTA[i], OUTPUT);
    digitalWrite(LED_PLANTA[i], LOW);
  }
  pinMode(LED_LUZ, OUTPUT);
  digitalWrite(LED_LUZ, LOW);

  // Pantalla LCD
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print("     ACME S.A.      ");
  lcd.setCursor(0, 1); lcd.print("  Ascensor v2.0     ");
  lcd.setCursor(0, 2); lcd.print("  Inicializando...  ");

  dht.begin();

  if (!rtc.begin()) {
    Serial.println("ERROR: RTC no encontrado!");
    Serial.flush();
  }

  servoAscensor.attach(SERVO_PIN);
  servoAscensor.write(ANGULO_PLANTA[0]);

  digitalWrite(LED_PLANTA[0], HIGH);     // Indicador planta 1 ON

  delay(2000);
  lcd.clear();

  printClockData();
  Serial.println("Sistema ascensor ACME iniciado. Planta 1.");
  Serial.println("Pulsadores listos: P1=D3, P2=D12, P3=D13, P4=A2, P5=A3");
}

// ===== LOOP =====
void loop() {
  leerPulsadores();
  moverAscensor();

  if (millis() - tUltimaLectura >= 2000) {
    tUltimaLectura = millis();
    leerSensores();
    detectarPresencia();
    if (lecturaValida) {
      controlTemperatura();
      controlIluminacion();
    }
  }

  if (millis() - tUltimoLCD >= 4000) {
    tUltimoLCD = millis();
    actualizarLCD();
    pantallaLCD = (pantallaLCD + 1) % 3;
  }
}

// ===== Lectura de pulsadores con anti-rebote =====
// INPUT_PULLUP: pulsador no presionado = HIGH, presionado = LOW.
// Detecta flanco de bajada (HIGH -> LOW) con debounce de 50 ms.
void leerPulsadores() {
  for (int i = 0; i < NUM_PLANTAS; i++) {
    bool estado = digitalRead(BTN_PLANTA[i]);

    if (estado != btnEstadoPrev[i] &&
        (millis() - btnTUltimo[i]) > DEBOUNCE_MS) {

      btnTUltimo[i]   = millis();
      btnEstadoPrev[i] = estado;

      // Flanco de bajada = pulsacion valida
      if (estado == LOW) {
        plantaDestino = i + 1;
        printClockData();
        Serial.print("Pulsador planta ");
        Serial.print(plantaDestino);
        Serial.println(" presionado");
      }
    }
  }
}

// ===== Movimiento del servo =====
void moverAscensor() {
  if (plantaDestino == plantaActual) return;

  enMovimiento = true;
  int idxActual  = plantaActual  - 1;
  int idxDestino = plantaDestino - 1;

  digitalWrite(LED_PLANTA[idxActual], LOW);

  printClockData();
  Serial.print("Moviendo de planta ");
  Serial.print(plantaActual);
  Serial.print(" a planta ");
  Serial.println(plantaDestino);

  int angOrigen  = ANGULO_PLANTA[idxActual];
  int angDestino = ANGULO_PLANTA[idxDestino];

  if (angOrigen < angDestino) {
    for (int a = angOrigen; a <= angDestino; a++) {
      servoAscensor.write(a);
      delay(15);
    }
  } else {
    for (int a = angOrigen; a >= angDestino; a--) {
      servoAscensor.write(a);
      delay(15);
    }
  }

  plantaActual = plantaDestino;
  enMovimiento = false;
  digitalWrite(LED_PLANTA[idxDestino], HIGH);

  printClockData();
  Serial.print("Llegado a planta ");
  Serial.println(plantaActual);
}

// ===== Deteccion de presencia con HC-SR04 =====
void detectarPresencia() {
  float dist = ultrasoundData();
  hayUsuario = (dist > 0 && dist < UMBRAL_PRESENCIA);
  if (hayUsuario) {
    Serial.print("Presencia detectada en cabina. Distancia: ");
    Serial.print(dist);
    Serial.println(" cm");
  }
}

// ===== Lectura de sensores =====
void leerSensores() {
  tempAnterior = tempActual;

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    tempActual    = t;
    humActual     = h;
    lecturaValida = true;
  }

  luxActual = luxData();
  pctLuz = (int)constrain(map((long)luxActual, 0, 1000, 0, 100), 0, 100);

  printClockData();
  Serial.print("Planta: ");    Serial.print(plantaActual);
  Serial.print(" | T: ");      Serial.print(tempActual, 1); Serial.print(" C");
  Serial.print(" | H: ");      Serial.print(humActual, 1);  Serial.print(" %");
  Serial.print(" | Lux: ");    Serial.print(luxActual, 1);
  Serial.print(" | Luz%: ");   Serial.print(pctLuz);
  Serial.print(" | Usuario: "); Serial.println(hayUsuario ? "SI" : "NO");
}

float luxData() {
  int   analogLightValue = analogRead(LDR_PIN);
  float voltage    = analogLightValue / 1024.0 * 5;
  float resistance = 2000 * voltage / (1 - voltage / 5);
  float lux        = pow(RL10 * 1e3 * pow(10, GAMMA) / resistance, (1 / GAMMA));
  return lux;
}

float ultrasoundData() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long timeHigh = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (timeHigh == 0) return 999.0;
  float distance = timeHigh * 0.0340 / 2;
  return distance;
}

void controlTemperatura() {
  if (tempActual > TEMP_MAX) {
    accionTemp = -1;
    Serial.println("CTRL TEMP: ENFRIANDO");
  } else if (tempActual < TEMP_MIN) {
    accionTemp = +1;
    Serial.println("CTRL TEMP: CALENTANDO");
  } else {
    accionTemp = 0;
    Serial.println("CTRL TEMP: ZONA MUERTA (OK)");
  }
}

void controlIluminacion() {
  if (!luzArtificialOn && pctLuz < (LUZ_SETPOINT - LUZ_HISTERESIS)) {
    luzArtificialOn = true;
    digitalWrite(LED_LUZ, HIGH);
    Serial.println("CTRL LUZ: ENCENDIDA (luz baja)");
  } else if (luzArtificialOn && pctLuz > LUZ_SETPOINT) {
    luzArtificialOn = false;
    digitalWrite(LED_LUZ, LOW);
    Serial.println("CTRL LUZ: APAGADA (luz suficiente)");
  } else {
    Serial.print("CTRL LUZ: ");
    Serial.println(luzArtificialOn ? "ON (manteniendo)" : "OFF (manteniendo)");
  }
}

void actualizarLCD() {
  lcd.clear();

  if (pantallaLCD == 0) {
    lcd.setCursor(0, 0); lcd.print("ASCENSOR   Pta: "); lcd.print(plantaActual);
    lcd.setCursor(0, 1); lcd.print("Destino:   Pta: "); lcd.print(plantaDestino);
    lcd.setCursor(0, 2); lcd.print("Estado: ");
    lcd.print(enMovimiento ? "MOVIENDO    " : "PARADO      ");
    lcd.setCursor(0, 3); lcd.print("Usuario: ");
    lcd.print(hayUsuario ? "SI  " : "NO  ");
  } else if (pantallaLCD == 1) {
    lcd.setCursor(0, 0); lcd.print("TEMPERATURA         ");
    lcd.setCursor(0, 1); lcd.print("Anterior: ");
    lcd.print(tempAnterior, 1); lcd.print((char)223); lcd.print("C  ");
    lcd.setCursor(0, 2); lcd.print("Actual:   ");
    lcd.print(tempActual, 1); lcd.print((char)223); lcd.print("C  ");
    lcd.setCursor(0, 3); lcd.print("Control: ");
    if      (accionTemp == -1) lcd.print("ENFRIANDO  ");
    else if (accionTemp == +1) lcd.print("CALENTANDO ");
    else                       lcd.print("ZONA MUERTA");
  } else {
    lcd.setCursor(0, 0); lcd.print("CLIMA DE OPERACION  ");
    lcd.setCursor(0, 1); lcd.print("Humedad:  ");
    lcd.print(humActual, 1); lcd.print(" %   ");
    lcd.setCursor(0, 2); lcd.print("Ilumina:  ");
    lcd.print(pctLuz); lcd.print(" %   ");
    lcd.setCursor(0, 3); lcd.print("Ctrl luz: ");
    lcd.print(luzArtificialOn ? "ENCENDIDA" : "APAGADA  ");
  }
}

void printClockData() {
  DateTime current = rtc.now();
  Serial.print("Fecha y hora: ");
  Serial.print(current.day(),    DEC); Serial.print('/');
  Serial.print(current.month(),  DEC); Serial.print('/');
  Serial.print(current.year(),   DEC); Serial.print(" [");
  Serial.print(current.hour(),   DEC); Serial.print(':');
  Serial.print(current.minute(), DEC); Serial.print(':');
  Serial.print(current.second(), DEC); Serial.print("]: ");
}
