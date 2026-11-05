#pragma once
// Режимы, не зависящие от звука: стробоскоп и подсветка

// 6 - стробоскоп
namespace StrobeMode {
  unsigned long timer;
  int bright;
  boolean up, down;

  void update() {
    unsigned long lightTime = (long)param(P_STROBE_PERIOD) * STROBE_DUTY / 100;  // длительность вспышки
    if (millis() - timer > (unsigned long)param(P_STROBE_PERIOD)) {
      timer = millis();
      up = true;
      down = false;
    }
    if (millis() - timer > lightTime) {
      down = true;
    }
    if (up) {                             // если настало время пыхнуть
      if (bright < 255)                   // если яркость не максимальная
        bright += param(P_STROBE_SMOOTH); // увелчить
      if (bright > 255) {                 // если пробили макс. яркость
        bright = 255;                     // оставить максимум
        up = false;                       // флаг опустить
      }
    }
    if (down) {                           // гаснем
      if (bright > 0)                     // если яркость не минимальная
        bright -= param(P_STROBE_SMOOTH); // уменьшить
      if (bright < 0) {                   // если пробили мин. яркость
        down = false;
        bright = 0;                       // оставить 0
      }
    }

    if (bright > 0) fill_solid(leds, NUM_LEDS, CHSV(STROBE_COLOR, STROBE_SAT, bright));
    else fill_solid(leds, NUM_LEDS, CHSV(EMPTY_COLOR, 255, param(P_EMPTY_BRIGHT)));
  }

  void onEvent(const Event &e) {
    // при включении режима начинаем с тёмной ленты, первая вспышка - сразу
    if (e.type == EV_ENTER) {
      bright = 0;
      up = down = false;
      timer = millis() - param(P_STROBE_PERIOD) - 1;
    }
  }
}

// 7 - подсветка. Подрежимы (P_LIGHT_MODE): 0 - постоянный цвет, 1 - плавная смена цвета, 2 - бегущая радуга
namespace LightMode {
  int color;
  unsigned long colorTimer, rainbowTimer;

  void update() {
    switch (param(P_LIGHT_MODE)) {
      case 0:
        fill_solid(leds, NUM_LEDS, CHSV(param(P_LIGHT_COLOR), param(P_LIGHT_SAT), 255));
        break;
      case 1:
        if (millis() - colorTimer > (unsigned long)param(P_COLOR_SPEED)) {
          colorTimer = millis();
          if (++color > 255) color = 0;
        }
        fill_solid(leds, NUM_LEDS, CHSV(color, param(P_LIGHT_SAT), 255));
        break;
      case 2:
        if (millis() - rainbowTimer > 30) {
          rainbowTimer = millis();
          color += param(P_RAINBOW_PERIOD);
          if (color > 255) color = 0;
          if (color < 0) color = 255;
        }
        float rainbow_steps = color;
        float step = param(P_RAINBOW_STEP_2) / 10.0;
        for (int i = 0; i < NUM_LEDS; i++) {
          leds[i] = CHSV((int)floor(rainbow_steps), 255, 255);
          rainbow_steps += step;
          if (rainbow_steps > 255) rainbow_steps = 0;
          if (rainbow_steps < 0) rainbow_steps = 255;
        }
        break;
    }
  }
}
