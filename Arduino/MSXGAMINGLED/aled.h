//---------------------------------
// CustomARGB用 アドレサブルRGB LEDライブラリ
// Indoor Corgi
// Version 2020/7/4

#include <Arduino.h>
#ifndef ALED_H
#define ALED_H

//---------------------------------
// マクロ

#define CH1 0x1
#define CH2 0x2
#define CH3 0x4
#define CH4 0x8
#define CH5 0x10

// #define DEBUG_PERIOD  // アニメーション描写時間をシリアルモニターに出力。別途Serial.begin()が必要

//---------------------------------
// クラス

// RGB方式で色を管理するクラス
class RGBColor {
 public:
  uint8_t red;
  uint8_t green;
  uint8_t blue;

  // コンストラクタ
  //   指定が無ければ0で初期化
  RGBColor() : red(0), green(0), blue(0) {}

  // コンストラクタ
  //   0 - 255の整数で色を指定
  RGBColor(uint8_t red, uint8_t green, uint8_t blue) : red(red), green(green), blue(blue) {}

  // 0.0 - 100.0の小数で色を指定
  void setFloat(float r, float g, float b) {
    red = (int)(r * 2.55);
    green = (int)(g * 2.55);
    blue = (int)(b * 2.55);
  }
};

// HSV方式で色を管理するクラス
class HSVColor {
 public:
  float hue = 0.0f;  // 色相     0.0 - 360.0
  float sat = 0.0f;  // 色の濃さ 0.0 - 100.0
  float val = 0.0f;  // 明るさ   0.0 - 100.0

  // コンストラクタ
  //   指定が無ければ0で初期化
  HSVColor() : hue(0.0f), sat(0.0f), val(0.0f) {}

  // コンストラクタ
  //   初期値の色を指定
  HSVColor(float hue, float sat, float val) : hue(hue), sat(sat), val(val) {}

  // RGB形式に変換
  RGBColor toRGB() {
    float r = val;
    float g = val;
    float b = val;

    while (hue >= 360.0f) hue -= 360.0f;

    if (sat > 0.0f) {
      float hd = hue / 60.0f;
      int i = (int)hd;
      float f = hd - (float)i;
      switch (i) {
        case 0:
          g *= 1.0f - sat * (1.0f - f) / 100.0f;
          b *= 1.0f - sat / 100.0f;
          break;
        case 1:
          r *= 1.0f - sat * f / 100.0f;
          b *= 1.0f - sat / 100.0f;
          break;
        case 2:
          r *= 1.0f - sat / 100.0f;
          b *= 1.0f - sat * (1.0f - f) / 100.0f;
          break;
        case 3:
          r *= 1.0f - sat / 100.0f;
          g *= 1.0f - sat * f / 100.0f;
          break;
        case 4:
          r *= 1.0f - sat * (1.0f - f) / 100.0f;
          g *= 1.0f - sat / 100.0f;
          break;
        case 5:
          g *= 1.0f - sat / 100.0f;
          b *= 1.0f - sat * f / 100.0f;
          break;
      }
    }

    RGBColor rgb;
    rgb.setFloat(r, g, b);
    return rgb;
  }
};

// アドレサブルRGB LEDを制御し、模様やアニメーションを表示するためのクラス
class ALED {
 public:
  static const uint8_t maxCh = 5;

 protected:
  uint8_t ch;         // 選択チャネル ch1がbit0, ch2がbit1... ch5がbit4に対応
  uint16_t ledCount;  // LEDの数 (物理的な総数ではなく、別々に制御できる数)
  uint32_t lastDrawTime = 0;  // アニメーション周期調整用 最後にdrawした時間を記録
  bool group = false;         // 複数のチャネルをまとめて制御する場合にtrue
  uint16_t ledCounts[maxCh];  // 複数のチャネルをまとめて制御する場合の各デバイスのLEDの数

 public:
  // 模様の種類の切り替え用
  static const uint8_t patSingle = 0;
  static const uint8_t patGrad = 1;
  static const uint8_t patSym = 2;
  static const uint8_t patPoint1 = 3;
  static const uint8_t patPoint2 = 4;
  uint8_t pattern = patGrad;  // 表示する模様の種類

  uint8_t* sendBuf;       // LEDに送信するデータを格納しておくバッファー
  uint16_t* scr;          // 模様の表示する順番を入れ替えるスクランブル情報
  float pos = 0.0f;       // 表示する模様の基準の位置(移動、回転用) 0.0 - 100.0
  HSVColor color;         // 表示する模様の基準の色
  HSVColor colorPattern;  // 表示する模様の場所による色の変化量
  float val = 100.0f;     // 表示する模様の明るさ調整(点滅用) 0.0 - 100.0
  uint8_t repeat = 1;     // 表示する模様の繰り返し回数

  HSVColor colorChange;  // アニメーションの1stepあたり基準の色を変化させる量
  float posChange = 0.0f;  // アニメーションの1stepあたり位置を変化させる量
  float valChange = 0.0f;  // アニメーションの1stepあたり明るさ変化させる量

  // period : アニメーションする際の周期をミリ秒で指定する。
  //   デフォルト33(リフレッシュレート30Hz)
  //   推奨値は25 - 50。低い値のほうがなめらかに見えるが、計算処理が間に合わない場合は
  //     設定値以上の間隔になる。
  uint8_t period = 33;

  // コンストラクタ#1 1つのチャネルを1つのデバイスとして制御する
  //   ch : 出力チャネル
  //     ch1 | ch3 | ch5  とするとチャネル1, 3, 5に同じデータを出力する
  //   ledCount :  制御LEDの数を指定する
  ALED(uint8_t ch, uint16_t ledCount) : ch(ch), ledCount(ledCount) {
    // バッファー初期化
    sendBuf = new uint8_t[ledCount * 3];
    for (uint16_t i = 0; i < ledCount * 3; i++) {
      sendBuf[i] = 0;
    }

    // スクランブル初期化
    scr = new uint16_t[ledCount];
    for (uint16_t i = 0; i < ledCount; i++) {
      scr[i] = i;
    }

    reset();
  }

  // コンストラクタ#2 複数のチャネルをまとめて1つのデバイスのように制御する
  //   ch : 出力チャネル
  //     ch1 | ch3 | ch5  とするとチャネル1, 3, 5をまとめて1つのデバイスのように扱う
  //   ledCountN :  各チャネルの制御LEDの数。選択チャネルが3つの場合は1, 2, 3に指定し、4,
  //   5は0にする。
  ALED(uint8_t ch, uint16_t ledCount1, uint16_t ledCount2, uint16_t ledCount3 = 0,
       uint16_t ledCount4 = 0, uint16_t ledCount5 = 0)
      : ch(ch) {
    ledCount = 0;
    ledCounts[0] = ledCount1;
    ledCounts[1] = ledCount2;
    ledCounts[2] = ledCount3;
    ledCounts[3] = ledCount4;
    ledCounts[4] = ledCount5;
    for (uint8_t i = 0; i < maxCh; i++) {
      ledCount += ledCounts[i];
    }

    // バッファー初期化
    sendBuf = new uint8_t[ledCount * 3];
    for (uint16_t i = 0; i < ledCount * 3; i++) {
      sendBuf[i] = 0;
    }

    // スクランブル初期化
    scr = new uint16_t[ledCount];
    for (uint16_t i = 0; i < ledCount; i++) {
      scr[i] = i;
    }

    reset();

    group = true;  // グループ化フラグセット
  }

  // デストラクタ 終了時の処理
  ~ALED() {
    delete[] sendBuf;  // バッファーをメモリから破棄
    delete[] scr;      // スクランブルをメモリから破棄
  }

  // 変数をリセット
  //   sendBufとscrは変更しない
  //   on : trueでval=100.0 (点灯), falseでval=0.0 (消灯) にセットする
  virtual void reset(bool on = true) {
    pattern = patGrad;
    color.hue = 0.0f;
    color.sat = 100.0f;
    color.val = 100.0f;
    colorPattern.hue = 0.0f;
    colorPattern.sat = 0.0f;
    colorPattern.val = 0.0f;
    colorChange.hue = 0.0f;
    colorChange.sat = 0.0f;
    colorChange.val = 0.0f;
    pos = 0.0f;
    posChange = 0.0f;
    val = on ? 100.0f : 0.0f;
    valChange = 0.0f;
    repeat = 1;
  }

  // バッファーのデータをデバイスに送信
  void sendLedData() {
    if (group) {
      // グループ化されている場合は各チャネルをそれぞれ送信
      uint16_t start = 0;
      uint16_t index = 0;  // ledCounts配列のインデックスカウンター
      uint8_t mask = 1;
      for (uint8_t i = 0; i < maxCh; i++) {
        if (0 != (mask & ch) && ledCounts[index] > 0) {
          genWaveform(mask & ch, start, ledCounts[index] * 3);
          start += ledCounts[index] * 3;
        }
        index++;
        mask = mask << 1;
      }

    } else {
      // グループ化されていない場合は全チャネルに同じデータを送信
      genWaveform(ch);
    }

    delayMicroseconds(350);  // 全てのデータ送信後はしばらくLOWにする
  }

  // LEDにデータを送る波形を出力
  //   ch : チャネル
  //   start : 送信を始めるバッファーの位置
  //   length : 送信するバイト数
  void genWaveform(uint8_t ch, uint16_t start = 0, uint16_t length = 0) {
    uint8_t buf;
    uint16_t i = start;
    uint8_t mask = 0x80;
    uint16_t stop = start + length;
    if (length == 0) stop = ledCount * 3;
    noInterrupts();
    while (true) {
      buf = sendBuf[i];
      while (true) {
        if ((buf & mask) == 0) {
          PORTC = ch;
          asm("nop");
          asm("nop");
          mask = mask >> 1;
          PORTC = 0;
          if (mask == 0) {
            i++;
            if (i == stop) {
              interrupts();
              return;
            }
            mask = 0x80;
            break;
          }
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
  
        } else {
          PORTC = ch;
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          asm("nop");
          mask = mask >> 1;
          if (mask == 0) {
            mask = 0x80;
            i++;
            if (i == stop) {
              PORTC = 0;
              interrupts();
              return;
            }
            PORTC = 0;
            break;
          }
          PORTC = 0;
        }
      }
    }
  }

  // n番目(0からスタート)のLEDのバッファーに指定した色データを格納
  //   スクランブル情報 scr に基づいて物理LEDの位置を入れ替える
  void loadLedDataRgb(uint16_t n, RGBColor rgb) {
    uint16_t i = scr[n] * 3;
    sendBuf[i] = rgb.red;
    sendBuf[i + 1] = rgb.green;
    sendBuf[i + 2] = rgb.blue;
  }

  // n番目(0からスタート)のLEDのバッファーに指定した色データを格納
  //   HSVColorで呼び出す
  virtual void loadLedData(uint16_t n, HSVColor color) { loadLedDataRgb(n, color.toRGB()); }

  // バッファーに単一の色データを格納
  //   val : 最終的な明るさの調整を0.0 - 100.0で指定する。点滅させる場合などに使用。
  virtual void loadSinglePattern(HSVColor color, float val) {
    color.val *= val / 100.0f;
    RGBColor rgb = color.toRGB();
    for (uint16_t i = 0; i < ledCount; i++) {
      loadLedDataRgb(i, rgb);
    }
  }

  // バッファーにグラデーションをつけた色データを格納
  //   loadGradPattern, loadSymPatternから呼び出される。
  //   startLedとcountで対象のLEDが決まる。startLed=2, count=4だと2番目から5番目までのLEDが対象。
  //   対象LEDの中の開始位置をstart, 範囲をrangeで指定する。これは0-100の値で指定する。
  //
  //   color : 基準になる色
  //   colorPattern : hue, sat, valに色相、濃度、明るさの変化量を指定
  //   start : 開始位置を0.0 - 100.0で指定する。0.0が対象LEDの1番目のLEDに対応
  //   range : 範囲を0.0 - 100.0で指定する。100.0だと対象LED全領域。50.0だと半分。
  //   val   : 最終的な明るさの調整を0.0 - 100.0で指定する。点滅させる場合などに使用。
  //   startLed : 対象LEDの最初番号(0から数える)。指定しないと0。
  //   count : 対象LEDの数。指定しないか0だとledCountを使う。
  virtual void loadGradBase(HSVColor color, HSVColor colorPattern, float start, float range,
                            float val, uint16_t startLed = 0, uint16_t count = 0) {
    HSVColor hsv;

    // 想定外の入力
    if (range < -100.0f || range > 100.0f || val < 0.0f || val > 100.0f) return;
    if (startLed >= ledCount || startLed + count > ledCount) return;

    // 範囲外の場合は補正する
    while (start < 0.0f) start += 100.0f;
    while (start > 100.0f) start -= 100.0f;

    // LED数の指定が無い場合は全てのLEDを対象
    if (0 == count) count = ledCount;

    // それぞれのLEDについて色を計算
    for (uint16_t i = 0; i < count; i++) {
      float posLed = ((float)i) / (float)count * 100.0f;
      float posStop = start + range;
      if (posStop > 100.0f && posLed < start) posLed += 100.0f;
      if (posStop < 0.0f && posLed > start) posLed -= 100.0f;
      if (range > 0.0f) {
        if (posLed < start || posLed >= posStop) continue;
      } else if (range < 0.0f) {
        if (posLed >= start || posLed < posStop) continue;
      } else
        return;

      hsv.hue = color.hue + colorPattern.hue * (posLed - start) / range;
      hsv.sat = color.sat + colorPattern.sat * (posLed - start) / range;
      hsv.val = color.val + colorPattern.val * (posLed - start) / range;

      // 範囲外の場合は補正する
      while (hsv.hue < 0.0f) hsv.hue += 360.0f;
      while (hsv.hue > 360.0f) hsv.hue -= 360.0f;
      while (hsv.sat < 0.0f) hsv.sat += 100.0f;
      while (hsv.sat > 100.0f) hsv.sat -= 100.0f;
      while (hsv.val < 0.0f) hsv.val += 100.0f;
      while (hsv.val > 100.0f) hsv.val -= 100.0f;

      hsv.val *= val / 100.0f;  // 全体の明るさ

      loadLedData(i + startLed, hsv);  // バッファーに保存
    }
  }

  // バッファーにグラデーション模様の色データを格納
  //   color : 基準になる色
  //   colorPattern : hue, sat, valに色相、濃度、明るさの変化量を指定
  //   start : 開始位置を0.0 - 100.0で指定する。0.0が一番目のLEDに対応
  //   range : 範囲を0.0 - 100.0で指定する。100.0だと全領域。50.0だと半分。
  //   val   : 最終的な明るさの調整を0.0 - 100.0で指定する。点滅させる場合などに使用。
  //   repeat : 模様の繰り返し回数
  virtual void loadGradPattern(HSVColor color, HSVColor colorPattern, float start = 0.0f,
                               float range = 100.0f, float val = 100.0f, uint8_t repeat = 1) {
    for (uint8_t i = 0; i < repeat; i++) {
      loadGradBase(color, colorPattern, start + ((float)i) * range / ((float)repeat),
                   range / ((float)repeat), val);
    }
  }

  // バッファーにグラデーション模様の色データを対称になるように格納
  //   loadGradPatternとの違いは、グラデーションで色を変化させていき、その後またもとの色に戻るような模様になる
  //   パラメーターの説明はloadGradPattern参照
  virtual void loadSymPattern(HSVColor color, HSVColor colorPattern, float start = 0.0f,
                              float range = 100.0f, float val = 100.0f, uint8_t repeat = 1) {
    for (uint8_t i = 0; i < repeat; i++) {
      loadGradBase(color, colorPattern, start + ((float)i) * range / ((float)repeat),
                   range / 2.0f / ((float)repeat), val);
      loadGradBase(color, colorPattern, start + ((float)i) * range / ((float)repeat),
                   -range / 2.0f / ((float)repeat), val);
    }
  }

  // positionの位置に一番近いLEDつに色を設定する
  //   color : positionに最も近いLEDに設定する色
  //   position : 色を変える場所を0.0 - 100.0で指定
  //   repeat : 指定した回数だけ等間隔に同じ描写をする
  //   clear : trueだと他のLEDを消灯。falseだと他のLEDは変更しない。
  virtual void loadPointPattern(HSVColor color, float position, bool clear, float val,
                                uint8_t repeat) {
    if (val < 0.0f || val > 100.0f) return;

    while (position < 0.0f) position += 100.0f;
    while (position >= 100.0f) position -= 100.0f;

    if (clear) loadSinglePattern(HSVColor(0.0f, 0.0f, 0.0f), 0.0f);

    // 最も近いLEDの明るさを計算
    color.val *= val / 100.0f;

    for (uint8_t r = 0; r < repeat; r++) {
      float minDist = 100.0f;        // positionから最も近いLEDの距離を保存
      uint16_t minDistLed = 0xFFFF;  // positionから最も近いLED番号を保存 (0から数える)
      float targetPos = position + 100.0f * ((float)r) / ((float)repeat);
      while (targetPos < 0.0f) targetPos += 100.0f;
      while (targetPos >= 100.0f) targetPos -= 100.0f;

      // targetPosから最も近いLEDを探す
      for (uint16_t i = 0; i < ledCount; i++) {
        float posLed = ((float)i) / (float)ledCount * 100.0f;
        if (targetPos < posLed) targetPos += 100.0f;
        float dist = targetPos - posLed;
        if (dist < 0.0f) dist = -dist;
        if (dist < minDist) {
          minDist = dist;
          minDistLed = i;
        }
      }

      // 近い位置のLEDの色をバッファーに格納
      loadLedData(minDistLed, color);
    }
  }

  // パラメータ color, change, positionをもとに静止模様を表示する
  //   パラメータを変化させながらdrawを連続して呼び出すことでアニメーションを表示可能
  //   デフォルトではloadGradColorを全体に表示する
  //   継承クラスを作成し、draw関数を上書きすることでオリジナルの模様も作成できる
  virtual void draw() {
    switch (pattern) {
      case patSingle:
        loadSinglePattern(color, val);
        break;
      case patGrad:
        loadGradPattern(color, colorPattern, pos, 100.0, val, repeat);
        break;
      case patSym:
        loadSymPattern(color, colorPattern, pos, 100.0, val, repeat);
        break;
      case patPoint1:
        loadPointPattern(color, pos, true, val, repeat);
        break;
      case patPoint2:
        loadPointPattern(color, pos, false, val, repeat);
        break;
    }
    sendLedData();
  }

  // アニメーション用にパラメーターを更新する
  //   colorChange, posChange, valChange を使って color, pos , val を変更
  virtual void animationUpdate() {
    color.hue += colorChange.hue;
    if (color.hue > 360.0f) color.hue -= 360.0f;
    if (color.hue < 0.0f) color.hue += 360.0f;

    color.sat += colorChange.sat;
    if (color.sat > 100.0f) color.sat -= 100.0f;
    if (color.sat < 0.0f) color.sat += 100.0f;

    color.val += colorChange.val;
    if (color.val > 100.0f) color.val -= 100.0f;
    if (color.val < 0.0f) color.val += 100.0f;

    pos += posChange;
    if (pos > 100.0f) pos -= 100.0f;
    if (pos < 0.0f) pos += 100.0f;

    val += valChange;
    if (val > 100.0f) val = 100.0f;
    if (val < 0.0f) val = 0.0f;
  }

  // 最後の更新からperiod以上時間が経過したらtrueを返す
  //   アニメーションの速度の調整用
  bool animationReady() {
    uint32_t t = millis();
#ifdef DEBUG_PERIOD
    Serial.print("Period : ");
    Serial.println(t - lastDrawTime);
#endif
    if (lastDrawTime + period <= t) {
      lastDrawTime = t;
      return true;
    }
    return false;
  }

  // アニメーション表示
  //   steps : 値を変化させて表示する合計回数
  //   メンバ変数colorChange, posChange, valChange の値も使用するので設定しておく
  //   periodで設定した周期を維持しながら表示するが、計算が間に合わない場合はそれ以上の時間がかかる
  void animation(uint32_t steps) {
    for (uint32_t i = 0; i < steps; i++) {
      while (!animationReady())
        ;
      draw();
      animationUpdate();
    }
  }
};

// 複数のALEDを並列にアニメーション表示するクラス
class ParallelAnimation {
 private:
  static const uint8_t maxALED = 5;  // 並列に処理できるALEDの最大数
  uint32_t lastDrawTime = 0;  // アニメーション周期調整用 最後にdrawした時間を記録

 public:
  ALED* aleds[maxALED];  // 登録するALEDの参照

  // それぞれのALEDのperiodは無視され、ここで設定した値で同期される
  //   periodの説明はALEDを参照
  uint8_t period = 33;

  // コンストラクタ
  //   aledN : 登録するALEDの参照を渡す
  ParallelAnimation(ALED* aled1, ALED* aled2, ALED* aled3 = NULL, ALED* aled4 = NULL,
                    ALED* aled5 = NULL) {
    aleds[0] = aled1;
    aleds[1] = aled2;
    aleds[2] = aled3;
    aleds[3] = aled4;
    aleds[4] = aled5;
  }

  // 静止模様を描写する
  void draw() {
    for (uint8_t i = 0; i < maxALED; i++) {
      if (aleds[i] != NULL) {
        aleds[i]->draw();
      }
    }
  }

  // アニメーション用にパラメーターを更新する
  virtual void animationUpdate() {
    for (uint8_t i = 0; i < maxALED; i++) {
      if (aleds[i] != NULL) {
        aleds[i]->animationUpdate();
      }
    }
  }

  // 最後の更新からperiod以上時間が経過したらtrueを返す
  //   アニメーションの速度の調整用
  bool animationReady() {
    uint32_t t = millis();
#ifdef DEBUG_PERIOD
    Serial.print("GroupPeriod : ");
    Serial.println(t - lastDrawTime);
#endif
    if (lastDrawTime + period <= t) {
      lastDrawTime = t;
      return true;
    }
    return false;
  }

  // アニメーション表示
  //   steps : 値を変化させて表示する合計回数
  //   periodで設定した周期を維持しながら表示するが、計算が間に合わない場合はそれ以上の時間がかかる
  void animation(uint32_t steps) {
    for (uint32_t i = 0; i < steps; i++) {
      while (!animationReady())
        ;
      draw();
      animationUpdate();
    }
  }
};

#endif
