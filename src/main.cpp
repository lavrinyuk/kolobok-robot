#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <MPU6050_light.h>
#include <ESP32Servo.h>

const char* ssid = "Kolobok_Bot";
const char* password = "12345678";

const int ENA_PIN = 14;
const int IN1_PIN = 27;
const int IN2_PIN = 26;
const int SERVO_PIN = 13;
const int LED_PIN = 16;

unsigned long lastCommandTime = 0;
const unsigned long SAFE_TIMEOUT = 2000;

bool firstCommandReceived = false;
bool manualSos = false;
bool safeModeActive = false;

float Kp = 0.21;
float Kd = 0.02;
float Ki = 0.46;

float currentAngle = 0;
float targetAngle = 0;
float error = 0;
float prevError = 0;

unsigned long lastPIDTime = 0;
const int dt = 10;

float derivative = 0;
float integral = 0;

int out = 0;
bool isBalansing = false;

WebServer server(80);
Servo myServo;
MPU6050 mpu(Wire);

// Прототипы
void balancePID();
void handleSOSPattern();
void connectionCheck();
void handleAction();
void stopRobot();

// ========================
// SETUP
// ========================

void setup() {
    Serial.begin(115200);

    pinMode(ENA_PIN, OUTPUT);
    pinMode(IN1_PIN, OUTPUT);
    pinMode(IN2_PIN, OUTPUT);
    pinMode(LED_PIN, OUTPUT);

    myServo.attach(SERVO_PIN);
    myServo.write(90);

    Wire.begin();

    byte status = mpu.begin();

    Serial.print(F("MPU6050 статус: "));
    Serial.println(status);

    delay(1000);

    // Гиро и не акселерометр
    mpu.calcOffsets(true, false);


    // ========================
    // LITTLEFS
    // ========================

    if (!LittleFS.begin(true)) {
        Serial.println("Ошибка монтирования LittleFS");
        return;
    }

    Serial.println("LittleFS OK");


    // ========================
    // WI-FI
    // ========================

    WiFi.softAP(ssid, password);

    Serial.print("Точка доступа запущена: ");
    Serial.println(ssid);

    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());


    // ========================
    // WEB SERVER
    // ========================

    

    // Получение телеметрии
    server.on("/getAllData", []() {
        lastCommandTime = millis();
        safeModeActive = false;

        String angle =
            String(mpu.getAngleX()) + "," +
            String(mpu.getAngleY()) + "," +
            String(mpu.getTemp());

        server.send(200, "text/plain", angle);
    });


    // Kp
    server.on("/setKp", []() {
        if (server.hasArg("val")) {
            Kp = server.arg("val").toFloat();

            Serial.print("New Kp: ");
            Serial.println(Kp);
        }

        server.send(200, "text/plain", "OK");
    });


    // Kd
    server.on("/setKd", []() {
        if (server.hasArg("val")) {
            Kd = server.arg("val").toFloat();

            Serial.print("New Kd: ");
            Serial.println(Kd);
        }

        server.send(200, "text/plain", "OK");
    });


    // Ki
    server.on("/setKi", []() {
        if (server.hasArg("val")) {
            Ki = server.arg("val").toFloat();

            Serial.print("New Ki: ");
            Serial.println(Ki);
        }

        server.send(200, "text/plain", "OK");
    });

    // Главная страница
    server.on("/", []() {
    File file = LittleFS.open("/web/index.html", "r");

    if (!file) {
        server.send(404, "text/plain", "index.html not found");
        return;
    }

    server.streamFile(file, "text/html");
    file.close();
    });

    server.serveStatic("/style.css", LittleFS, "/web/style.css");
    server.serveStatic("/script.js", LittleFS, "/web/script.js");

    // Управление
    server.on("/action", handleAction);

    server.begin();

    Serial.println("HTTP server started");
}


// ========================
// LOOP
// ========================

void loop() {
    mpu.update();

    server.handleClient();

    handleSOSPattern();

    connectionCheck();

    if (isBalansing && !manualSos && !safeModeActive) {
        balancePID();
    }
}


// ========================
// PID-СТАБИЛИЗАЦИЯ
// ========================

void balancePID() {
    if (millis() - lastPIDTime < dt) {
        return;
    }

    lastPIDTime = millis();

    currentAngle = mpu.getAngleX();

    error = targetAngle - currentAngle;

    float dt_sec = dt / 1000.0f;

    derivative = (error - prevError) / dt_sec;

    integral += error * dt_sec;

    if (abs(integral) > 50) {
        integral = 50 * (integral > 0 ? 1 : -1);
    }

    out =
        90
        - (error * Kp)
        - (derivative * Kd)
        - (integral * Ki);

    out = constrain(out, 45, 135);

    myServo.write(out);

    prevError = error;
}


// ========================
// SOS
// ========================

void handleSOSPattern() {
    static unsigned long lastUpdate = 0;
    static int step = 0;

    const int timings[] = {
        200, 200, 200, 200, 200, 200,
        600,
        200, 600, 200, 600, 200,
        200, 200, 200, 200, 200, 1000
    };

    if (manualSos || safeModeActive) {

        if (millis() - lastUpdate > timings[step]) {
            lastUpdate = millis();

            step++;

            if (step >= 18) {
                step = 0;
            }

            digitalWrite(LED_PIN, (step % 2 == 0));
        }

    } else {

        digitalWrite(LED_PIN, LOW);

        step = 0;
    }
}


// ========================
// ПРОВЕРКА СОЕДИНЕНИЯ
// ========================

void connectionCheck() {
    if (
        firstCommandReceived &&
        !manualSos &&
        (millis() - lastCommandTime > SAFE_TIMEOUT)
    ) {

        if (!safeModeActive) {
            safeModeActive = true;

            stopRobot();

            Serial.println("Соединение потеряно");
        }
    }
}


// ========================
// WEB ACTION
// ========================

void handleAction() {

    // SOS
    if (server.hasArg("sos")) {

        manualSos = (server.arg("sos") == "1");

        if (manualSos) {
            stopRobot();
        }
    }


    // Управление джойстиком
    if (server.hasArg("speed") && !manualSos) {

        firstCommandReceived = true;

        lastCommandTime = millis();

        safeModeActive = false;

        int s = server.arg("speed").toInt();
        int t = server.arg("turn").toInt();


        // Стоп при отпускании джойстика
        if (abs(s) < 15) {

            stopRobot();

        } else {

            isBalansing = false;

            digitalWrite(IN1_PIN, s > 0);
            digitalWrite(IN2_PIN, s < 0);

            analogWrite(ENA_PIN, abs(s));

            myServo.write(
                constrain(t, 45, 135)
            );
        }
    }

    server.send(200, "text/plain", "OK");
}


// ========================
// СТОП
// ========================

void stopRobot() {

    analogWrite(ENA_PIN, 0);

    digitalWrite(IN1_PIN, LOW);
    digitalWrite(IN2_PIN, LOW);

    isBalansing = true;
}