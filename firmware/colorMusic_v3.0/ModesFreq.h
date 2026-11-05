#pragma once
// Режимы, работающие по частотам (цветомузыка, бегущие частоты, анализатор спектра)

namespace Freq {
  FreqAnalyzer analyzer;

  // цвет полосы: 0 - низкие, 1 - средние, 2 - высокие
  CHSV bandColor(byte band) {
    static const byte hues[3] = {LOW_COLOR, MID_COLOR, HIGH_COLOR};
    return CHSV(hues[band], 255, analyzer.bright[band]);
  }
  CHSV emptyColor() {
    return CHSV(EMPTY_COLOR, 255, param(P_EMPTY_BRIGHT));
  }

  // какую полосу показывать по подрежиму P_FREQ_STROBE_MODE:
  // 0 - все (приоритет у высоких), 1 - высокие, 2 - средние, 3 - низкие.
  // -1 - ни одна полоса не активна
  int8_t pickBand(const boolean active[3]) {
    int8_t mode = param(P_FREQ_STROBE_MODE);
    if (mode == 0) {
      for (int8_t band = 2; band >= 0; band--)
        if (active[band]) return band;
      return -1;
    }
    int8_t band = 3 - mode;
    return active[band] ? band : -1;
  }
}

// 3 - цветомузыка, 5 полос
namespace FiveStripesMode {
  void update() {
    Freq::analyzer.update();
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < STRIPE)          leds[i] = Freq::bandColor(2);
      else if (i < STRIPE * 2) leds[i] = Freq::bandColor(1);
      else if (i < STRIPE * 3) leds[i] = Freq::bandColor(0);
      else if (i < STRIPE * 4) leds[i] = Freq::bandColor(1);
      else if (i < STRIPE * 5) leds[i] = Freq::bandColor(2);
    }
  }
}

// 4 - цветомузыка, 3 полосы
namespace ThreeStripesMode {
  void update() {
    Freq::analyzer.update();
    for (int i = 0; i < NUM_LEDS; i++) {
      if (i < NUM_LEDS / 3)          leds[i] = Freq::bandColor(2);
      else if (i < NUM_LEDS * 2 / 3) leds[i] = Freq::bandColor(1);
      else if (i < NUM_LEDS)         leds[i] = Freq::bandColor(0);
    }
  }
}

// 5 - цветомузыка, вспышки всей лентой
namespace FlashMode {
  void update() {
    Freq::analyzer.update();
    int8_t band = Freq::pickBand(Freq::analyzer.flash);
    fill_solid(leds, NUM_LEDS, band >= 0 ? Freq::bandColor(band) : Freq::emptyColor());
  }
}

// 8 - бегущие частоты: вспышки рождаются в центре и разбегаются к краям.
// Картинка сдвигается, поэтому режим с флагом MODE_NO_CLEAR
namespace RunningMode {
  unsigned long timer;

  void update() {
    Freq::analyzer.update();
    int8_t band = Freq::pickBand(Freq::analyzer.running);
    leds[NUM_LEDS / 2] = band >= 0 ? Freq::bandColor(band) : Freq::emptyColor();
    leds[(NUM_LEDS / 2) - 1] = leds[NUM_LEDS / 2];
    if (millis() - timer > (unsigned long)param(P_RUNNING_SPEED)) {
      timer = millis();
      for (int i = 0; i < NUM_LEDS / 2 - 1; i++) {
        leds[i] = leds[i + 1];
        leds[NUM_LEDS - i - 1] = leds[i];
      }
    }
  }
}

// 9 - анализатор спектра, симметрично от краёв к центру
namespace SpectrumMode {
  void update() {
    Freq::analyzer.update();
    const float freq_to_stripe = NUM_LEDS / 40; // /2 так как симметрия, и /20 так как 20 частот
    byte HUEindex = param(P_HUE_START);
    for (int i = 0; i < NUM_LEDS / 2; i++) {
      byte this_bright = map(Freq::analyzer.freq_f[(int)floor((NUM_LEDS / 2 - i) / freq_to_stripe)], 0, Freq::analyzer.freq_max_f, 0, 255);
      this_bright = constrain(this_bright, 0, 255);
      leds[i] = CHSV(HUEindex, 255, this_bright);
      leds[NUM_LEDS - i - 1] = leds[i];
      HUEindex += param(P_HUE_STEP);
    }
  }
}
