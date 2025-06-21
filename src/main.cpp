#include <WiFi.h>
#include <esp_now.h>

typedef struct struct_message {
  int pitch;
  int roll;
  int weight;
  int angle;
} struct_message;

struct_message incomingData;

// ✅ Old ESP-NOW callback signature
void onDataReceive(const uint8_t *mac, const uint8_t *data, int len) {
  memcpy(&incomingData, data, sizeof(incomingData));
  Serial.println("=== Data Diterima ===");
  Serial.print("Pitch: "); Serial.println(incomingData.pitch);
  Serial.print("Roll : "); Serial.println(incomingData.roll);
  Serial.print("Berat: "); Serial.println(incomingData.weight);
  Serial.print("Sudut: "); Serial.println(incomingData.angle);
  Serial.println("=====================\n");
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // ✅ Use legacy-style callback registration
  esp_now_register_recv_cb(onDataReceive);
  Serial.println("Receiver ESP-NOW siap...");
}

void loop() {
  // No need to do anything in the loop
}
