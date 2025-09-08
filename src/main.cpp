#include <M5Unified.h>

// ===== ユーザー設定 =====
static const int TOUCH_PIN   = 5;   // G5=GPIO5（他: G6=6, G7=7, G8=8）
static const int POWER_PIN   = 16;  // G39=GPIO39（電源検出ピン、未使用なら-1）
static const int LED_PIN     = 35;  // Atom S3 内蔵RGB LEDのデータピン（35が既定）
static const int LED_COUNT   = 1;   // 内蔵は1ピクセル
static const uint32_t CALIB_MS = 2000; // 起動時キャリブ時間(ms)
static const int SMA_N       = 16;   // 移動平均サンプル数
static const int MARGIN      = 150; // ベースラインからのON閾値差（環境で調整）
static const int HYST        = 60;  // ヒステリシス（OFF側の戻し幅）

int baseline = 0;
int smaBuf[SMA_N];
int smaIdx = 0;
int smaSum = 0;
bool touched = false;

// ---- 移動平均つきの現在値取得 ----
int readTouchAveraged() {
  int v = touchRead(TOUCH_PIN);     // ESP32-S3は「触れると数値が上がる」傾向
  smaSum -= smaBuf[smaIdx];
  smaBuf[smaIdx] = v;
  smaSum += v;
  smaIdx = (smaIdx + 1) % SMA_N;
  return smaSum / SMA_N;
}

void setup() {
  // M5の初期化
  auto cfg = M5.config();
  M5.begin(cfg);

  // 画面設定（数値のみ表示）
  M5.Display.setRotation(0);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::Font2);   // 見やすいフォント
  M5.Display.setTextSize(1);

  Serial.begin(115200);
  Serial.println("Touch Sensor with Calibration");
  Serial.printf("TOUCH_PIN=%d\n", TOUCH_PIN);
  delay(100);

  // バッファ初期化
  smaSum = 0;
  for (int i = 0; i < SMA_N; ++i) { smaBuf[i] = 0; }

  // キャリブレーション
  M5.Display.setCursor(0, 0);
  M5.Display.println("Calibrating... (2s)");
  uint32_t t0 = millis();
  long acc = 0; int n = 0;
  while (millis() - t0 < CALIB_MS) {
    int v = touchRead(TOUCH_PIN);
    acc += v; n++;
    delay(10);
  }
  baseline = (n > 0) ? (acc / n) : touchRead(TOUCH_PIN);

  // 移動平均バッファにベースラインを反映
  smaSum = 0; smaIdx = 0;
  for (int i = 0; i < SMA_N; ++i) { smaBuf[i] = baseline; smaSum += baseline; }

  // 初期表示
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.printf("Baseline : %d\n", baseline);
  M5.Display.printf("Threshold: base+%d\n\n", MARGIN);
}

void loop() {
  M5.update();

  int avg = readTouchAveraged();
  int delta = avg - baseline;

  // しきい値＆ヒステリシス
  int onThresh  = baseline + MARGIN;
  int offThresh = onThresh - HYST;

  // タッチ判定
  if (!touched && avg >= onThresh) {
    touched = true;
    M5.Display.setTextColor(TFT_RED, TFT_BLACK);  // 赤文字
    M5.Display.setCursor(0, 90);
    M5.Display.println("TOUCHED!");
    Serial.println("TOUCHED!");
  } 
  else if (touched && avg <= offThresh) {
    touched = false;
    // "TOUCHED!" 表示を消去
    M5.Display.fillRect(0, 90, 120, 20, TFT_BLACK);
    Serial.println("released");
  }

  // 数値表示を更新
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 50) {
    lastDraw = millis();
    int y = 48;
    M5.Display.fillRect(0, y, M5.Display.width(), 40, TFT_BLACK);
    M5.Display.setCursor(0, y);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.printf("avg  : %d\n", avg);
    M5.Display.printf("delta: %d\n", delta);
  }

  delay(5);
}
