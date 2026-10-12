#include <BleKeyboard.h>
#include <BLESecurity.h>
#include <BLEDevice.h>
#include "esp_bt.h"
#include "esp_gap_ble_api.h"

// ===== BLEキーボード =====
BleKeyboard bleKeyboard("GC-Right", "GC", 100);

// ===== GPIO設定 =====
const int numPins = 3;
const int pins[numPins] = {2, 3, 4};  // B=GPIO2, X=GPIO3, A=GPIO4

// ===== キー設定 =====
const char SHORTPRESS_A_KEY = 'a';
const char SHORTPRESS_B_KEY = 'b';
const char SHORTPRESS_X_KEY = 'x';

const char LONGPRESS_A_KEY = '\n';  // Enter
const char LONGPRESS_B_KEY = '\b';  // Backspace
const char LONGPRESS_X_KEY = 'X';

const bool longPressEnabled[3] = {true, true, false};
const bool repeatEnabled[3] = {false, true, false};

// ===== デバウンス・長押し設定 =====
const unsigned long debouncePressMs = 10;
const unsigned long debounceReleaseMs = 50;
const unsigned long longPressMs = 300;
const unsigned long repeatRateMs = 60;

// ===== 動作設定 =====
const int cpuFreqMhz = 80;
const esp_power_level_t bleTxPower = ESP_PWR_LVL_N0;  // 0dBm

// ===== 状態管理 =====
int activeIndex = -1;
bool stableLow[numPins] = {false, false, false};
int lastRaw[numPins] = {HIGH, HIGH, HIGH};
unsigned long lastEdgeAt[numPins] = {0, 0, 0};

bool longMode = false;
unsigned long pressedAt = 0;
unsigned long lastRepeatAt = 0;

char shortKeyOfPin(int pin) {
  switch (pin) {
    case 2: return SHORTPRESS_B_KEY;
    case 3: return SHORTPRESS_X_KEY;
    case 4: return SHORTPRESS_A_KEY;
    default: return 0;
  }
}

char longKeyOfPin(int pin) {
  switch (pin) {
    case 2: return LONGPRESS_B_KEY;
    case 3: return LONGPRESS_X_KEY;
    case 4: return LONGPRESS_A_KEY;
    default: return 0;
  }
}

int buttonIndexOfPin(int pin) {
  switch (pin) {
    case 4: return 0;  // A
    case 2: return 1;  // B
    case 3: return 2;  // X
    default: return -1;
  }
}

void setup() {
  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], INPUT_PULLUP);
  }

  setCpuFrequencyMhz(cpuFreqMhz);

  // BLEスタックを初期化してから送信出力とPHYを設定する。
  bleKeyboard.begin();
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, bleTxPower);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, bleTxPower);
  esp_ble_gap_set_prefered_default_phy(
    ESP_BLE_GAP_PHY_2M,
    ESP_BLE_GAP_PHY_2M
  );

  // パスキー入力を使わず、ボンディングを有効にする。
  static BLESecurity security;
  security.setCapability(ESP_IO_CAP_NONE);
  security.setAuthenticationMode(ESP_LE_AUTH_BOND);
  security.setKeySize(16);
  security.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}

void loop() {
  if (!bleKeyboard.isConnected()) {
    delay(10);
    return;
  }

  const unsigned long now = millis();

  // デバウンス処理
  for (int i = 0; i < numPins; i++) {
    const int raw = digitalRead(pins[i]);

    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      lastEdgeAt[i] = now;
    } else {
      const unsigned long requiredMs =
        (raw == LOW) ? debouncePressMs : debounceReleaseMs;

      if (now - lastEdgeAt[i] >= requiredMs) {
        stableLow[i] = (raw == LOW);
      }
    }
  }

  if (activeIndex == -1) {
    for (int i = 0; i < numPins; i++) {
      if (stableLow[i]) {
        activeIndex = i;
        longMode = false;
        pressedAt = now;
        lastRepeatAt = now;
        break;
      }
    }
  } else {
    const int pin = pins[activeIndex];
    const int buttonIndex = buttonIndexOfPin(pin);

    if (!longMode &&
        stableLow[activeIndex] &&
        buttonIndex >= 0 &&
        longPressEnabled[buttonIndex] &&
        now - pressedAt >= longPressMs) {
      longMode = true;

      if (!repeatEnabled[buttonIndex]) {
        const char key = longKeyOfPin(pin);
        if (key) {
          bleKeyboard.write(key);
        }
      }
    }

    if (longMode &&
        stableLow[activeIndex] &&
        buttonIndex >= 0 &&
        repeatEnabled[buttonIndex] &&
        now - lastRepeatAt >= repeatRateMs) {
      const char key = longKeyOfPin(pin);
      if (key) {
        bleKeyboard.write(key);
        lastRepeatAt = now;
      }
    }

    if (!stableLow[activeIndex]) {
      if (!longMode) {
        const char key = shortKeyOfPin(pin);
        if (key) {
          bleKeyboard.write(key);
        }
      }
      activeIndex = -1;
    }
  }

  delay(10);
}
