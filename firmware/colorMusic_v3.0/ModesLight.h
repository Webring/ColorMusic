#pragma once
// Режимы, не зависящие от звука: огонь и подсветка

// 6 - огонь на шуме Перлина (по мотивам FireFX, AlexGyver, MIT).
// Цвет каждого светодиода берётся из 2D-шума: по ленте - позиция, по второй оси - время.
// Чем больше значение шума, тем сильнее сдвиг цвета, ниже насыщенность и выше яркость
namespace FireMode {
  uint16_t counter;       // "время" шума
  unsigned long timer;

  void update() {
    if (millis() - timer > 20) {
      timer = millis();
      counter += param(P_FIRE_SPEED);
    }
    for (int i = 0; i < NUM_LEDS; i++) {
      byte val = inoise8((uint16_t)(i * param(P_FIRE_SCALE)), counter);
      leds[i] = CHSV(param(P_FIRE_HUE) + map(val, 0, 255, 0, FIRE_HUE_GAP),
                     constrain(map(val, 0, 255, FIRE_MAX_SAT, FIRE_MIN_SAT), 0, 255),
                     constrain(map(val, 0, 255, FIRE_MIN_BRIGHT, FIRE_MAX_BRIGHT), 0, 255));
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
