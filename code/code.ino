// === LIBRAIRIES ===
#include <Servo.h>
#include <PID_v1.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === TEMPERATURE ===
#define ONE_WIRE_BUS 2       // DS18B20 temperature sensor on pin 2 --> D22
#define PIN_OUTPUT 3         // PWM output pin to control heating element --> D3

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// PID control for temperature
double temp_SP = -100.0, temp_measured = 0.0, temp_resist_control = 0.0; // default temperature is negative to avoid overwarming
const double MAX_DURATION = 100; // PWM period in milliseconds

const double Kp_temp = 2, Ki_temp = 5, Kd_temp = 1;
PID myPID_temp(&temp_measured, &temp_resist_control, &temp_SP, Kp_temp, Ki_temp, Kd_temp, DIRECT);

// Map a value with a given resolution
double mapWithResolution(double x, double fromLow, double fromHigh, double toLow, double toHigh, double resolution) {
  x = constrain(x, fromLow, fromHigh);
  double y = map(x, fromLow, fromHigh, toLow, toHigh);
  y = int(y / resolution) * resolution;
  return y;
}

// Output PWM signal for a given percentage of max duration
void thermalOutput(double percentage, double maxDuration, int pinOut, double frequency) {
  double time = mapWithResolution(percentage, 0, 100, 0, maxDuration, 1 / frequency);
  digitalWrite(pinOut, HIGH);
  delay(time);
  digitalWrite(pinOut, LOW);
  delay(maxDuration - time);
}

// === SERVO VALVES ===
Servo external_valve;
Servo backpressure_valve;

float extern_close_SP = 0.0;
float back_pressure_SP = 0.0;

const int pos_external_zero = -10;
const int pos_backpres_zero = 5.3;

// === INNER VALVE CONTROL ===
const int analogAngle = A0;
const int borneENA = 10;
const int borneIN1 = 9;
const int borneIN2 = 8;

const float voltage_open = 4.41;
const float voltage_close = 0.5;
const float voltage_generator = 8.0;

const int max_speed = 255;

double inner_measured = 0.0;
double inner_close_SP = 0.0;
double controllerOutput = 0.0;

double Kp = 200.0, Ki = 150.0, Kd = 10.0;
PID openingPID(&inner_measured, &controllerOutput, &inner_close_SP, Kp, Ki, Kd, DIRECT);

float closingRead = 0.0;

float voltage2closing(float voltage) {
  float m = (0.0 - 1.0) / (voltage_open - voltage_close);
  float p = -m * voltage_open;
  return m * voltage + p;
}

float closing2voltage(float closing) {
  float m = (voltage_open - voltage_close) / -1.0;
  float p = voltage_open;
  return m * closing + p;
}

float correctingAngle(float positionServo) {
  return constrain(90.0*positionServo, 0, 90);
}

void rotationDirection(bool forward) {
  digitalWrite(borneIN1, forward ? HIGH : LOW);
  digitalWrite(borneIN2, forward ? LOW : HIGH);
}

void rotationSpeedForward(int speed) {
  speed = constrain(speed, 0, max_speed);
  analogWrite(borneENA, speed);
}

void readSerialData() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n'); // lit jusqu'à retour à la ligne
    input.trim(); // supprime les espaces et retours chariot

    // FOR DEBUG: à remettre pour debug
    // Serial.print("Message reçu : ");
    // Serial.println(input);

    // Séparation des valeurs
    int index1 = input.indexOf(',');
    int index2 = input.indexOf(',', index1 + 1);
    int index3 = input.indexOf(',', index2 + 1);

    if (index1 > 0 && index2 > 0 && index3 > 0) {
      String temp_str         = input.substring(0, index1);
      String inner_close_str  = input.substring(index1 + 1, index2);
      String extern_close_str = input.substring(index2 + 1, index3);
      String back_pressure_str= input.substring(index3 + 1);

      // Conversion si nécessaire (ex: en float)
      temp_SP          = constrain(temp_str.toFloat(),-100,50);
      inner_close_SP   = constrain(inner_close_str.toFloat(),0,1);
      extern_close_SP  = constrain(extern_close_str.toFloat(),0,1);
      back_pressure_SP = constrain(back_pressure_str.toFloat(),0,1);
    } else {
      Serial.println("Erreur : Format invalide.");
    }
  }
}



// === SETUP ===
void setup() {
  Serial.begin(9600);

  pinMode(borneENA, OUTPUT);
  pinMode(borneIN1, OUTPUT);
  pinMode(borneIN2, OUTPUT);
  pinMode(PIN_OUTPUT, OUTPUT);

  rotationDirection(true);
  openingPID.SetMode(AUTOMATIC);
  myPID_temp.SetMode(AUTOMATIC);

  external_valve.attach(6);
  backpressure_valve.attach(7);

  sensors.begin();
  sensors.requestTemperatures();
  temp_measured = sensors.getTempCByIndex(0);
}

// === LOOP PRINCIPAL ===
void loop() {
  // Lecture de la position réelle de la vanne intérieure
  int sensorValue = analogRead(analogAngle);
  float angleVoltageIn = sensorValue * (5.0 / 1023.0);
  inner_measured = voltage2closing(angleVoltageIn);

  // Lecture température actuelle
  sensors.requestTemperatures();
  temp_measured = sensors.getTempCByIndex(0);

  // === PARSING SERIAL INPUT ===
  readSerialData();

  // === PID TEMPERATURE ===
  myPID_temp.Compute();
  if (temp_SP <= 0 || temp_measured > 50) temp_resist_control = 0;
  temp_resist_control = constrain(temp_resist_control, 0, 100);

  thermalOutput(temp_resist_control, MAX_DURATION, PIN_OUTPUT, 200);

  // === PID POSITION VANNE INTÉRIEURE ===
  openingPID.Compute();
  if (inner_close_SP == 0.0) controllerOutput = 0;

  rotationSpeedForward(controllerOutput);

  // Positionnement des servos
  external_valve.write(correctingAngle(extern_close_SP)+pos_external_zero);
  backpressure_valve.write(correctingAngle(back_pressure_SP)+pos_backpres_zero);

  // === DEBUG SERIAL à remettre pour debug===
  // Serial.print("temp_measured:"); Serial.print(temp_measured);
  // Serial.print(",temp_SP:"); Serial.print(temp_SP);
  // Serial.print(",inner_measured:"); Serial.print(inner_measured);
  // Serial.print(",inner_SP:"); Serial.print(inner_close_SP);
  // Serial.print(",back_press_SP:"); Serial.print(back_pressure_SP);
  // Serial.print(",outer_SP:"); Serial.println(extern_close_SP);
}
