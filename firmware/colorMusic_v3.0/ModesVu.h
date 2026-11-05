#pragma once
// Режимы "шкала громкости" (VU meter): шкала растёт из центра ленты в обе стороны

namespace Vu {
  VuMeter meter;

  // измерить громкость и залить подложку. true - есть что рисовать
  bool measure() {
    bool loud = meter.update();
    // заливаем "подложку", если яркость достаточная
    if (param(P_EMPTY_BRIGHT) > 5) fill_solid(leds, NUM_LEDS, CHSV(EMPTY_COLOR, 255, param(P_EMPTY_BRIGHT)));
    return loud;
  }

  // нарисовать шкалы. color(pos) - цвет светодиода, pos - позиция от центра в масштабе 0.. 255
  template <class ColorFn>
  void drawBars(ColorFn color) {
    const float index = (float)255 / MAX_CH;   // коэффициент перевода для палитры
    byte count = 0;
    for (int i = (MAX_CH - 1); i > ((MAX_CH - 1) - meter.Rlenght); i--) {
      leds[i] = color(count * index);
      count++;
    }
    count = 0;
    for (int i = (MAX_CH); i < (MAX_CH + meter.Llenght); i++ ) {
      leds[i] = color(count * index);
      count++;
    }
    if (param(P_EMPTY_BRIGHT) > 0) {
      CHSV this_dark = CHSV(EMPTY_COLOR, 255, param(P_EMPTY_BRIGHT));
      for (int i = ((MAX_CH - 1) - meter.Rlenght); i > 0; i--)
        leds[i] = this_dark;
      for (int i = MAX_CH + meter.Llenght; i < NUM_LEDS; i++)
        leds[i] = this_dark;
    }
  }
}

// 1 - шкала громкости, градиент от зелёного к красному
namespace VuGradientMode {
  DEFINE_GRADIENT_PALETTE(soundlevel_gp) {
    0,    0,    255,  0,  // green
    100,  255,  255,  0,  // yellow
    150,  255,  100,  0,  // orange
    200,  255,  50,   0,  // red
    255,  255,  0,    0   // red
  };

  void update() {
    if (!Vu::measure()) return;
    CRGBPalette32 pal = soundlevel_gp;  // собираем на стеке, чтобы не держать 96 байт RAM постоянно
    Vu::drawBars([&](float pos) {
      return ColorFromPalette(pal, pos);
    });
  }
}

// 2 - шкала громкости, бегущая радуга
namespace VuRainbowMode {
  int hue;
  unsigned long timer;

  void update() {
    if (!Vu::measure()) return;
    if (millis() - timer > 30) {
      timer = millis();
      hue = floor((float)hue + param(P_RAINBOW_STEP) / 10.0);
    }
    Vu::drawBars([](float pos) {
      return ColorFromPalette(RainbowColors_p, pos / 2 - hue);
    });
  }
}
