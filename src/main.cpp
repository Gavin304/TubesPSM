#include <WiFi.h>
#include <esp_now.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <HX711.h>
#include <math.h>

// --- Servo ---
Servo myServo;
const int servoPin = 15;
int currentAngle = 0;

// --- MPU6050 ---
Adafruit_MPU6050 mpu;
const int MPU_SDA = 21;
const int MPU_SCL = 22;
float pitchOffset = 0;
float rollOffset = 0;

// --- HX711 ---
HX711 scale;
const int HX_DT = 4;
const int HX_SCK = 5;
float scaleFactor = -213.4769;
bool isCalibratingWeight = false;

// --- Serial Input ---
String inputString = "";
bool inputReady = false;

// --- ESP-NOW ---
uint8_t receiverMAC[] = {0xF4, 0x65, 0x0B, 0xE7, 0x45, 0x74};

typedef struct struct_message {
  int pitch;
  int roll;
  int weight;
  int angle;
} struct_message;

struct_message dataToSend;

void handleSerialInput();
void kalibrasiMPU();
void kalibrasiBerat();

void setup() {
  Serial.begin(115200);
  delay(1000);

  // --- Servo ---
  myServo.attach(servoPin);

  // --- MPU6050 ---
  Wire.begin(MPU_SDA, MPU_SCL);
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("MPU6050 tidak ditemukan!");
    while (1) delay(10);
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // --- HX711 ---
  scale.begin(HX_DT, HX_SCK);
  Serial.println("Menunggu HX711...");
  while (!scale.is_ready()) {
    Serial.println("HX711 belum siap.");
    delay(500);
  }
  scale.set_scale(scaleFactor);
  scale.tare();
  Serial.println("Tare selesai.");

  // --- ESP-NOW ---
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peerInfo;
  memcpy(peerInfo.peer_addr, receiverMAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  peerInfo.ifidx = WIFI_IF_STA;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  esp_now_register_send_cb([](const uint8_t *mac, esp_now_send_status_t status) {
    Serial.print("Send Status: ");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
  });

  Serial.println("Ketik sudut (0–180), 'tare', 'kalibrasiMPU', atau 'kalibrasiBerat'");
  Serial.println("Pitch\tRoll\tWeight\tAngle");
}

void loop() {
  handleSerialInput();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  float pitchRaw = atan2(a.acceleration.y, a.acceleration.z) * -180 / PI;
  float rollRaw  = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180 / PI;
  float weightRaw = scale.get_units(15);

  int pitch = ceil(pitchRaw - pitchOffset);
  int roll  = ceil(rollRaw - rollOffset);
  int weight = -1 * ceil(weightRaw);
  int angle = currentAngle;

  // Print to Serial
  Serial.print(pitch); Serial.print("\t");
  Serial.print(roll); Serial.print("\t");
  Serial.print(weight); Serial.print("\t");
  Serial.println(angle);

  // Send via ESP-NOW
  dataToSend.pitch = pitch;
  dataToSend.roll = roll;
  dataToSend.weight = weight;
  dataToSend.angle = angle;
  esp_now_send(receiverMAC, (uint8_t *)&dataToSend, sizeof(dataToSend));

  delay(200);
}

void handleSerialInput() {
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n' || inChar == '\r') {
      inputReady = true;
    } else {
      inputString += inChar;
    }
  }

  if (inputReady && inputString.length() > 0) {
    inputString.trim();

    if (inputString.equalsIgnoreCase("tare")) {
      scale.tare();
      Serial.println("Tare selesai.");
    } else if (inputString.equalsIgnoreCase("kalibrasiMPU")) {
      kalibrasiMPU();
    } else if (inputString.equalsIgnoreCase("kalibrasiBerat")) {
      kalibrasiBerat();
    } else if (isCalibratingWeight && inputString.toFloat() > 0) {
      float refWeight = inputString.toFloat();
      long reading = scale.read_average(15);
      float newScale = (float)(reading) / refWeight;
      scale.set_scale(newScale);
      scaleFactor = newScale;
      Serial.print("Kalibrasi berat selesai. Skala baru: ");
      Serial.println(scaleFactor, 4);
      isCalibratingWeight = false;
    } else if (isDigit(inputString[0])) {
      int angle = inputString.toInt();
      if (angle >= 0 && angle <= 180) {
        myServo.write((angle - 180) * -1);
        currentAngle = angle;
        Serial.print("Servo diputar ke ");
        Serial.print(angle);
        Serial.println("°");
      } else {
        Serial.println("Sudut tidak valid (0–180).");
      }
    } else {
      Serial.println("Perintah tidak dikenali. Ketik sudut, 'tare', 'kalibrasiMPU', atau 'kalibrasiBerat'.");
    }

    inputString = "";
    inputReady = false;
  }
}

void kalibrasiMPU() {
  Serial.println("Kalibrasi MPU6050 dimulai (diamkan alat dalam posisi netral)...");
  float sumPitch = 0, sumRoll = 0;
  const int samples = 100;

  for (int i = 0; i < samples; i++) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    float pitch = atan2(a.acceleration.y, a.acceleration.z) * -180 / PI;
    float roll = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180 / PI;

    sumPitch += pitch;
    sumRoll += roll;
    delay(10);
  }

  pitchOffset = sumPitch / samples;
  rollOffset = sumRoll / samples;

  Serial.println("Kalibrasi MPU6050 selesai.");
  Serial.print("Offset Pitch: "); Serial.println(pitchOffset);
  Serial.print("Offset Roll: "); Serial.println(rollOffset);
}

void kalibrasiBerat() {
  Serial.println("Letakkan berat referensi dan tekan enter...");
  isCalibratingWeight = true;
}