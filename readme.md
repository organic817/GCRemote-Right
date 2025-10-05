# ビルドガイド
これは[ニンテンドーストア等](https://x.com/N_Officialstore/status/1815552413207846971)で販売しているカプセルトイ「コントローラーボタンコレクション」第2弾の「ニンテンドー ゲームキューブ：A・B・X・Yボタン」を簡便なBluetoothリモコンにするキットです。  
  
「ニンテンドー ゲームキューブ：A・B・X・Yボタン」はキーホルダー（以下キーホルダーと呼びます）になっていて、中には導電ゴムのスイッチも入っています。それをそのまま利用して（無加工で）狭いキーホルダー内に基板や電源を詰め込みました。  
組み立てて電源を入れるとスマホなどで使えるBluetoothリモコンになります。
  
ボタンが少ないから機能は少なめ、でもキーホルダーが実際に使えてちょっと嬉しい。  
そんな気持ちになるキットです。


## 必要な素材・道具
* リモコン化キット
  * 基板
    * Xiao ESP32C3が載っています。プログラムを書き換えられます。
  * リポバッテリー
  * スペーサー × 3
* プラスドライバー

## 組み立て方  
1. キーホルダーの裏のネジ3つ(写真の赤丸)を外す  
<img src="./img/img1.jpeg" width="300">
<img src="./img/img2.jpeg" width="300">
1. 背面の中にあるパーツを外す（キットを使わない時は必要なので捨てないこと）  
<img src="./img/img3.jpeg" width="300">
1. マイコン(Xiao ESP32C3）が見える側を上にしてゴムスイッチに載せる  
<img src="./img/img4.jpeg" width="300">
<img src="./img/img5.jpeg" width="300">
1. 隙間にバッテリーを入れて基板に繋ぐ  
<img src="./img/img6.jpeg" width="300">
1. 写真の赤丸の位置にスペーサーを入れる  
<img src="./img/img7.jpeg" width="300">
1. ケーブルを挟まないように背面を戻す  
<img src="./img/img8.jpeg" width="300">
1. ネジを締めて完成  

## 取り出し方  
組み立て方の手順で基板を取り出してください。

## 電源のON/OFFとペアリング
この基板には電源LED等はありません。そのため電源のON/OFFはスマホなどからBluetoothのペアリングの状態から確認してください。  
電源を入れるとBluetooth設定に「GC-Right」が出るので選択してペアリングします。

## 充電
電源をONにしたままでESP32C3にType-Cケーブルを挿せば充電が始まります。
### 注意
ESP32C3のバッテリー充電回路は簡易なものです。充電したまま長時間放置することのないようお気をつけください。  
空から満充電までの時間の目安は1時間です。

## プログラミング
このキットはArduino IDEを使ってプログラムの書き換えが可能です。基板上のXiao ESP32C3をパソコンと接続してください。
### Arduino IDEの設定
Arduino IDEの設定の準備は搭載マイコンの[Xiao ESP32C3のスタートガイド](https://wiki.seeedstudio.com/ja/XIAO_ESP32C3_Getting_Started/)を参照してください。ただしサンプルプログラムは外部ライブラリーに対応するためにesp32のバージョンを「2.0.17」にしてください。その後外部ライブラリー[T-vK/ESP32-BLE-Keyboard](https://github.com/T-vK/ESP32-BLE-Keyboard)を使用するのでインポートしてください。
### プログラム
ボタンはA→GPIO2, B→GPIO3, X→GPIO4に接続されています。  
サンプルプログラムは以下のような操作が出来ます。
|ボタン|キー操作|
|--|--|
|Aボタン|Aキー|
|Bボタン|Bキー|
|Xボタン|Xキー|
|A長押し|Enter1回|
|B長押し|BackSpace連打|
  
#### サンプルプログラム
```
#include <BleKeyboard.h>
#include <BLESecurity.h>
#include <BLEDevice.h>

BleKeyboard bleKeyboard("GC-Right", "GC", 100);

const int numPins = 3;
const int pins[numPins] = {2, 3, 4};   // GPIOを指定。B=GPIO2, X=GPIO3, A=GPIO4

// ===== デバウンス & 長押し設定 =====
const unsigned long debouncePressMs   = 50;   // 押下確定まで
const unsigned long debounceReleaseMs = 100;   // 解放確定まで
const unsigned long longPressMs       = 300;  // 長押し閾値
const unsigned long repeatRateMs      = 60;   // B長押しの連打周期

// ===== 状態管理 =====
int  activeIndex = -1;
bool stableLow[numPins] = {false,false,false};
int  lastRaw[numPins]   = {HIGH, HIGH, HIGH};
unsigned long lastEdgeAt[numPins] = {0,0,0};

bool longMode = false;
bool sentEnterForA = false;
unsigned long pressedAt = 0;
unsigned long lastRepeatAt = 0;

// 接続状態の変化を検出して広告モードを切り替える
bool wasConnected = false;
unsigned long advModeSince = 0;
enum AdvMode { ADV_FAST, ADV_SLOW };
AdvMode advMode = ADV_FAST;

// 短押しのキー割り当て（B=GPIO2, X=GPIO3, A=GPIO4）
char keyOfPin(int pin) {
  switch (pin) {
    case 2: return 'b';   // B
    case 3: return 'x';   // X
    case 4: return 'a';   // A
  }
  return 0;
}

// 広告パラメータ切り替え（0.625ms単位）
void setFastAdvertising() {
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setScanResponse(true);
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812)); // HIDサービスを広告に載せる
  adv->setMinInterval(0x20);  // ≈20ms
  adv->setMaxInterval(0x30);  // ≈30ms
  // 接続後の希望（端末が必ず従うわけではないが提示）
  adv->setMinPreferred(0x06); // 7.5ms
  adv->setMaxPreferred(0x0C); // 15ms
  BLEDevice::startAdvertising();
  advMode = ADV_FAST;
  advModeSince = millis();
}

void setSlowAdvertising() {
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->setScanResponse(true);
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setMinInterval(0x80);  // ≈80ms
  adv->setMaxInterval(0xA0);  // ≈100ms
  adv->setMinPreferred(0x06);
  adv->setMaxPreferred(0x0C);
  BLEDevice::startAdvertising();
  advMode = ADV_SLOW;
  advModeSince = millis();
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], INPUT_PULLUP);    // ピンのプルアップ設定、ボタンはGND接地でON
  }

  bleKeyboard.begin(); // BLE初期化

  // 起動直後は“高速アドバタイズ”で即アピール
  setFastAdvertising();

  // ペアリングチューニング
  BLESecurity* pSec = new BLESecurity();
  pSec->setCapability(ESP_IO_CAP_NONE);
  pSec->setAuthenticationMode(ESP_LE_AUTH_BOND);
  pSec->setKeySize(16);
  pSec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  Serial.println("Ready with remapped pins (B=2, X=3, A=4).");
}

void loop() {
  // 接続状態の変化で広告モードを調整
  bool connected = bleKeyboard.isConnected();
  if (connected && !wasConnected) {
    // 接続後省電力のスロー広告
    setSlowAdvertising();
  }
  if (!connected && wasConnected) {
    // 切断の際に高速広告で再アピール
    setFastAdvertising();
  }
  // 未接続が長い時にスローへ戻す省電力運用
  if (!connected && advMode == ADV_FAST && millis() - advModeSince > 30000UL) {
    setSlowAdvertising();
  }
  wasConnected = connected;

  if (!connected) { delay(1); return; }

  const unsigned long now = millis();

  // デバウンス処理
  for (int i = 0; i < numPins; i++) {
    const int raw = digitalRead(pins[i]);
    if (raw != lastRaw[i]) {
      lastRaw[i] = raw;
      lastEdgeAt[i] = now;
    } else {
      const unsigned long need = (raw == LOW) ? debouncePressMs : debounceReleaseMs;
      if ((now - lastEdgeAt[i]) >= need) {
        stableLow[i] = (raw == LOW);
      }
    }
  }

  if (activeIndex == -1) {
    // 新規押下の確定
    for (int i = 0; i < numPins; i++) {
      if (stableLow[i]) {
        activeIndex = i;
        longMode = false;
        sentEnterForA = false;
        pressedAt = now;
        lastRepeatAt = now;
        break;
      }
    }
  } else {
    const int i = activeIndex;
    const int pin = pins[i];

    // 長押し移行
    if (!longMode && stableLow[i] && (now - pressedAt >= longPressMs)) {
      longMode = true;
      if (pin == 4 && !sentEnterForA) {      // A長押し=Enter 1回
        bleKeyboard.write('\n');
        sentEnterForA = true;
      } else if (pin == 2) {                 // B長押し=Backspace連打
      }
    }

    // B長押しの連打処理
    if (longMode && stableLow[i] && pin == 2) {
      if (now - lastRepeatAt >= repeatRateMs) {
        bleKeyboard.write('\b'); // バックスペース
        lastRepeatAt = now;
      }
    }

    // 離されたら処理
    if (!stableLow[i]) {
      if (!longMode) {
        // 短押し（a/b/x）
        char k = keyOfPin(pin);
        if (k) bleKeyboard.write(k);
      }
      activeIndex = -1;
    }
  }

  delay(1);
}
```



