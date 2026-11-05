#pragma once
// Обработка звука: шкала громкости (VU) и разбор на частоты (FHT).
// Подключается из .ino после настроек - использует его дефайны и параметры.

const float averK = 0.006;    // коэффициент очень медленного фильтра (для автогромкости)

// разбить сигнал с SOUND_R_FREQ в спектр, результат в fht_log_out[]
void analyzeAudio() {
  for (int i = 0 ; i < FHT_N ; i++) {
    int sample = analogRead(SOUND_R_FREQ);
    fht_input[i] = sample; // put real data into bins
  }
  fht_window();  // window the data for better frequency response
  fht_reorder(); // reorder the data before doing the fht
  fht_run();     // process the data in the fht
  fht_mag_log(); // take the output of the fht
}

// ---------------- шкала громкости ----------------
class VuMeter {
  public:
    int Rlenght, Llenght;   // длина шкалы правого и левого канала (0.. MAX_CH)

    // измерить громкость. true - если звук выше порога и длины шкал пересчитаны
    bool update() {
      float RsoundLevel = 0, LsoundLevel = 0;
      for (byte i = 0; i < 100; i ++) {                                 // делаем 100 измерений
        int RcurrentLevel = analogRead(SOUND_R);                        // с правого
        if (RsoundLevel < RcurrentLevel) RsoundLevel = RcurrentLevel;   // ищем максимальное
        if (!MONO) {
          int LcurrentLevel = analogRead(SOUND_L);                      // и левого каналов
          if (LsoundLevel < LcurrentLevel) LsoundLevel = LcurrentLevel;
        }
      }

      filter(RsoundLevel, _RsoundLevel_f);
      if (!MONO) filter(LsoundLevel, _LsoundLevel_f);
      else _LsoundLevel_f = _RsoundLevel_f;  // если моно, то левый = правому

      // если значение ниже порога - рисовать нечего
      if (_RsoundLevel_f <= 15 || _LsoundLevel_f <= 15) return false;

      // расчёт общей средней громкости с обоих каналов, фильтрация.
      // Фильтр очень медленный, сделано специально для автогромкости
      _averageLevel = (float)(_RsoundLevel_f + _LsoundLevel_f) / 2 * averK + _averageLevel * (1 - averK);

      // принимаем максимальную громкость шкалы как среднюю, умноженную на некоторый коэффициент MAX_COEF
      int maxLevel = (float)_averageLevel * MAX_COEF;

      // преобразуем сигнал в длину ленты и ограничиваем до макс. числа светодиодов
      Rlenght = constrain(map(_RsoundLevel_f, 0, maxLevel, 0, MAX_CH), 0, MAX_CH);
      Llenght = constrain(map(_LsoundLevel_f, 0, maxLevel, 0, MAX_CH), 0, MAX_CH);
      return true;
    }

  private:
    float _RsoundLevel_f, _LsoundLevel_f;
    float _averageLevel = 50;

    static void filter(float level, float &filtered) {
      float smooth = param(P_SMOOTH) / 100.0;
      level = map(level, param(P_LOW_PASS), 1023, 0, 500);  // фильтруем по нижнему порогу шумов
      level = constrain(level, 0, 500);                     // ограничиваем диапазон
      level = pow(level, EXP);                              // возводим в степень (для большей чёткости работы)
      filtered = level * smooth + filtered * (1 - smooth);
    }
};

// ---------------- разбор на частоты ----------------
class FreqAnalyzer {
  public:
    // индекс полосы: 0 - низкие, 1 - средние, 2 - высокие
    int bright[3];          // текущая яркость полосы (вспыхивает до 255 и гаснет до EMPTY_BRIGHT)
    boolean flash[3];       // полоса вспыхнула в этом цикле
    boolean running[3];     // полоса ещё не погасла
    uint8_t freq_f[30];     // сглаженный спектр (для анализатора спектра)
    float freq_max_f;       // сглаженный максимум спектра

    void update() {
      analyzeAudio();
      int colorMusic[3] = {0, 0, 0};
      for (int i = 0 ; i < 32 ; i++) {
        if (fht_log_out[i] < param(P_SPEKTR_LOW_PASS)) fht_log_out[i] = 0;
      }
      // низкие частоты, выборка со 2 по 5 тон (0 и 1 зашумленные!)
      for (byte i = 2; i < 6; i++) {
        if (fht_log_out[i] > colorMusic[0]) colorMusic[0] = fht_log_out[i];
      }
      // средние частоты, выборка с 6 по 10 тон
      for (byte i = 6; i < 11; i++) {
        if (fht_log_out[i] > colorMusic[1]) colorMusic[1] = fht_log_out[i];
      }
      // высокие частоты, выборка с 11 по 31 тон
      for (byte i = 11; i < 32; i++) {
        if (fht_log_out[i] > colorMusic[2]) colorMusic[2] = fht_log_out[i];
      }
      int freq_max = 0;
      for (byte i = 0; i < 30; i++) {
        if (fht_log_out[i + 2] > freq_max) freq_max = fht_log_out[i + 2];
        if (freq_max < 5) freq_max = 5;

        if (freq_f[i] < fht_log_out[i + 2]) freq_f[i] = fht_log_out[i + 2];
        freq_f[i] = freq_f[i] > LIGHT_SMOOTH ? freq_f[i] - LIGHT_SMOOTH : 0;
      }
      freq_max_f = freq_max * averK + freq_max_f * (1 - averK);

      float smooth = param(P_SMOOTH_FREQ) / 100.0;
      float maxCoef = param(P_MAX_COEF_FREQ) / 10.0;
      for (byte i = 0; i < 3; i++) {
        _aver[i] = colorMusic[i] * averK + _aver[i] * (1 - averK);   // общая фильтрация
        _f[i] = colorMusic[i] * smooth + _f[i] * (1 - smooth);        // локальная
        if (_f[i] > _aver[i] * maxCoef) {
          bright[i] = 255;
          flash[i] = true;
          running[i] = true;
        } else flash[i] = false;
        if (bright[i] >= 0) bright[i] -= SMOOTH_STEP;
        if (bright[i] < param(P_EMPTY_BRIGHT)) {
          bright[i] = param(P_EMPTY_BRIGHT);
          running[i] = false;
        }
      }
    }

  private:
    float _f[3], _aver[3];
};
