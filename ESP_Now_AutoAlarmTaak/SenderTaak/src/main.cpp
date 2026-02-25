#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>

#define ID_LEN      32
#define ADDRES_LEN  6
#define NR_RCVS     2

// HC-SR04 pins
#define TRIG_PIN 5
#define ECHO_PIN 18
#define SOUND_SPEED 0.034f   // cm per microseconde

// MAC van deze zender (mag je laten, maar handig voor controle)
uint8_t SendFromAddress[] = {0xD4, 0x8A, 0xFC, 0xA2, 0xD2, 0x04};

// MAC's van de 2 receivers (pas aan naar jouw receivers)
uint8_t SendToAddress[NR_RCVS][ADDRES_LEN] = {
  {0x1B, 0x69, 0x20, 0xCC, 0xE8, 0x8C}, // Receiver 2
  {0xD8, 0x8A, 0xFC, 0xA4, 0x57, 0x40}  // Receiver 1
};

// Data die we sturen (struct moet identiek zijn op receivers)
typedef struct __attribute__((packed)) {
  char  sender_id[ID_LEN];     // we gebruiken enkel de eerste 6 bytes als MAC
  char  receiver_id[ID_LEN];   // idem
  float distance_cm;           // gemeten afstand
  uint8_t alarm;               // 0 of 1
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// Wordt opgeroepen na het versturen (handig om te zien of het lukt)
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  if (status == ESP_NOW_SEND_SUCCESS) Serial.println("OK");
  else Serial.println("FAIL");
}

// Leest afstand met HC-SR04
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn meet hoe lang ECHO HIGH blijft (in microseconden)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000); // timeout 25ms
  if (duration == 0) return -1.0f;               // geen meting

  // afstand = (tijd * snelheid) / 2 (heen en terug)
  float distance = (duration * SOUND_SPEED) / 2.0f;
  return distance;
}

void setup() {
  Serial.begin(9600);

  // Sensor pins instellen
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // ESP-NOW werkt enkel als WiFi in station mode staat
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // ESP-NOW starten
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // callback voor send-status
  esp_now_register_send_cb(OnDataSent);

  // 2 receivers toevoegen als peers
  for (int i = 0; i < NR_RCVS; i++) {
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, SendToAddress[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Failed to add peer");
      return;
    }
  }
}

void loop() {
  static int receiver = 0;  // naar welke receiver sturen we nu
  static int count = 1;

  if (receiver >= NR_RCVS) receiver = 0;

  float d = readDistanceCm();

  // Alarmregel: alarm aan als afstand geldig is én < 50 cm
  uint8_t alarm = 0;
  if (d > 0 && d < 50.0f) {
    alarm = 1;
  } else {
    alarm = 0;
  }

  // Data invullen
  myData.distance_cm = d;
  myData.alarm = alarm;

  // sender_id/receiver_id vullen: we zetten de eerste 6 bytes = MAC
  memset(myData.sender_id, 0, ID_LEN);
  memset(myData.receiver_id, 0, ID_LEN);
  memcpy(myData.sender_id, SendFromAddress, 6);
  memcpy(myData.receiver_id, SendToAddress[receiver], 6);

  // Verzenden naar de gekozen receiver
  esp_err_t result = esp_now_send(SendToAddress[receiver], (uint8_t*)&myData, sizeof(myData));
  if (result != ESP_OK) {
    Serial.println("Error sending data");
  }

  // Debug print
  Serial.print("Package ");
  Serial.print(count);
  Serial.print(" -> Receiver ");
  Serial.print(receiver + 1);
  Serial.print(" | Dist: ");
  Serial.print(myData.distance_cm);
  Serial.print(" cm | Alarm: ");
  Serial.println(myData.alarm);

  receiver++;
  count++;

  delay(200); // hoe sneller, hoe vaker updates (mag je aanpassen)
}