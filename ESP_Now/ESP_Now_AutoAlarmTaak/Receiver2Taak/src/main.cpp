#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>

#include <Wire.h>
#include <U8g2lib.h>

#define ID_LEN  32
#define MSG_LEN 64

// OLED: SH1106 via I2C (SDA 21, SCL 22 op ESP32 standaard)
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// Outputs
#define LED_PIN     2
#define BUZZER_PIN  15   // actieve buzzer = HIGH/LOW

// MAC van de zender (moet matchen met sender code)
uint8_t senderAddress[] = {0xD4, 0x8A, 0xFC, 0xA2, 0xD2, 0x04};

// === VUL HIER DE MAC VAN DEZE RECEIVER IN ===
// Receiver1:
uint8_t myAddress[] = {0xD8, 0x8A, 0xFC, 0xA4, 0x57, 0x40};
// Receiver2 zou zijn:
// uint8_t myAddress[] = {0x1B, 0x69, 0x20, 0xCC, 0xE8, 0x8C};

// Struct moet identiek zijn aan sender
typedef struct __attribute__((packed)) {
  char  sender_id[ID_LEN];
  char  receiver_id[ID_LEN];
  float distance_cm;
  uint8_t alarm;
} struct_message;

struct_message Mydata;

bool data_received = false;
unsigned long lastRx = 0;   // tijdstip laatste correcte ontvangst

// Kleine OLED defaults
void u8g2_prepare() {
  u8g2.setFontRefHeightExtendedText();
  u8g2.setDrawColor(1);
  u8g2.setFontPosTop();
  u8g2.setFontDirection(0);
}

// Tekst + balk op OLED
void u8g2_publish_data(const char *line1, const char *line2, int barLen) {
  u8g2.clearBuffer();
  u8g2_prepare();

  u8g2.drawRFrame(0, 0, 127, 18, 3);
  u8g2.setFont(u8g2_font_6x13_tf);
  u8g2.drawStr(5, 3, line1);

  u8g2.drawStr(5, 22, line2);

  if (barLen < 0) barLen = 0;
  if (barLen > 127) barLen = 127;
  u8g2.drawBox(0, 44, barLen, 18);

  u8g2.sendBuffer();
}

// Wordt opgeroepen bij elk ontvangen ESP-NOW pakket
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *In_data, int data_len) {
  if (data_len != sizeof(struct_message)) return;

  memcpy(&Mydata, In_data, sizeof(Mydata));

  // We accepteren enkel data die echt voor deze receiver bedoeld is
  if (memcmp(Mydata.sender_id, senderAddress, 6) == 0 &&
      memcmp(Mydata.receiver_id, myAddress, 6) == 0) {
    data_received = true;
    lastRx = millis();
  } else {
    data_received = false;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  u8g2.begin();
  u8g2_prepare();
  u8g2_publish_data("ESP-NOW Receiver", "Waiting...", 0);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP NOW init error");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  bool linkOk;

  // Als we langer dan 1500ms niets ontvingen -> geen signaal
  if (millis() - lastRx < 1500) {
    linkOk = true;
  } else {
    linkOk = false;
  }

  // ===== NO SIGNAL: LED knippert, buzzer uit =====
  if (linkOk == false) {
    if ((millis() / 300) % 2 == 0) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
    digitalWrite(BUZZER_PIN, LOW);
    u8g2_publish_data("NO SIGNAL", "Geen data", 0);
    return;
  }

  // ===== Enkel iets doen als we een nieuw pakket kregen =====
  if (data_received == true) {
    data_received = false;

    // LED: aan bij alarm
    if (Mydata.alarm == 1) digitalWrite(LED_PIN, HIGH);
    else digitalWrite(LED_PIN, LOW);

    // BUZZER: piep-piep bij alarm (met if/else)
    static unsigned long t = 0;
    static bool buzzOn = false;

    if (Mydata.alarm == 1) {
      unsigned long now = millis();
      unsigned long interval;

      if (buzzOn == true) interval = 120;   // buzzer aan tijd
      else interval = 180;                  // pauze tijd

      if (now - t >= interval) {
        t = now;

        if (buzzOn == true) {
          buzzOn = false;
          digitalWrite(BUZZER_PIN, LOW);
        } else {
          buzzOn = true;
          digitalWrite(BUZZER_PIN, HIGH);
        }
      }
    } else {
      buzzOn = false;
      digitalWrite(BUZZER_PIN, LOW);
    }

    // OLED tekst
    char line1[MSG_LEN];
    char line2[MSG_LEN];

    snprintf(line1, MSG_LEN, "Afstand: %.1f cm", Mydata.distance_cm);

    if (Mydata.alarm == 1) snprintf(line2, MSG_LEN, "Alarm: JA");
    else snprintf(line2, MSG_LEN, "Alarm: NEE");

    // Balk: dichterbij -> meer balk
    int bar = 0;
    if (Mydata.distance_cm > 0) {
      float d = Mydata.distance_cm;
      if (d > 100) d = 100;
      bar = (int)(127.0f * (1.0f - d / 100.0f));
    }

    u8g2_publish_data(line1, line2, bar);

    // Serial debug
    Serial.print("Dist: ");
    Serial.print(Mydata.distance_cm);
    Serial.print("  Alarm: ");
    Serial.println(Mydata.alarm);
  }
}