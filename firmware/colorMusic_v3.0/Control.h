#pragma once
// Управление: изменение параметров, события режимам, команды извне, хранение в EEPROM.
// Подключается из .ino после modeList[] и paramMeta[].
//
// Команды не зависят от способа передачи: handleCommand(cmd, id, value) можно вызывать
// откуда угодно (UART, Bluetooth, кнопки, энкодер, I2C...). В комплекте - транспорт по Serial:
//   запрос: [0xA5] [cmd] [id] [value младший байт] [value старший байт]
//   ответ:  [0xA5] [CMD_VALUE] [id] [value мл.] [value ст.] - шлётся при КАЖДОМ изменении параметра
//           (откуда бы оно ни пришло, хоть с кнопки), и в ответ на CMD_GET

#include <EEPROM.h>

enum Command : uint8_t {
  CMD_SET = 1,      // id, value - установить параметр
  CMD_ADD,          // id, value - прибавить value к параметру (value может быть отрицательным)
  CMD_GET,          // id - прислать значение параметра. id = 0xFF - прислать все
  CMD_EVENT,        // id, value - передать текущему режиму событие EV_USER с этими данными
  CMD_SAVE,         // сохранить настройки в EEPROM сейчас
  CMD_RESET,        // вернуть настройки по умолчанию
  CMD_CALIBRATE,    // измерить уровень шумов
  CMD_VALUE = 0x80, // ответ: id, value - текущее значение параметра
};

#define PACKET_START 0xA5

const uint8_t EEPROM_KEY = 0x40 + P_COUNT;  // меняется вместе с количеством параметров
unsigned long eeprom_timer;
boolean eeprom_flag;

// ---------------- режимы ----------------
ModeDesc readMode(uint8_t i) {
  ModeDesc d;
  memcpy_P(&d, &modeList[i], sizeof(d));
  return d;
}

void sendEvent(uint8_t type, uint8_t id = 0, int16_t value = 0) {
  ModeDesc mode = readMode(param(P_MODE));
  if (mode.onEvent) {
    Event e = {type, id, value};
    mode.onEvent(e);
  }
}

// ---------------- транспорт ----------------
void sendPacket(uint8_t cmd, uint8_t id, int16_t value) {
#if CONTROL_SERIAL
  uint8_t buf[5] = {PACKET_START, cmd, id, (uint8_t)value, (uint8_t)(value >> 8)};
  Serial.write(buf, 5);
#endif
}

void reportParam(uint8_t id) {
  sendPacket(CMD_VALUE, id, param(id));
}

// ---------------- параметры ----------------
void saveSettings();

ParamMeta readMeta(uint8_t id) {
  ParamMeta m;
  memcpy_P(&m, &paramMeta[id], sizeof(m));
  return m;
}

// установить параметр (с ограничением по границам). true - если значение изменилось
bool setParam(uint8_t id, int16_t value) {
  if (id >= P_COUNT) return false;
  ParamMeta m = readMeta(id);
  value = constrain(value, m.min, m.max);
  if (paramValues[id] == value) return false;
  paramValues[id] = value;

  eeprom_timer = millis();     // сохраним через 30 секунд после последнего изменения
  eeprom_flag = true;

  // реакция системы
  switch (id) {
    case P_BRIGHTNESS: FastLED.setBrightness(value);
      break;
    case P_POWER:
      FastLED.clear();
      FastLED.show();
      if (KEEP_STATE) saveSettings();
      break;
  }

  // реакция режима
  if (id == P_MODE) sendEvent(EV_ENTER);
  else sendEvent(EV_PARAM, id, value);

  reportParam(id);
  return true;
}

void loadDefaults() {
  for (uint8_t i = 0; i < P_COUNT; i++) paramValues[i] = readMeta(i).def;
}

void saveSettings() {
  eeprom_flag = false;
  if (!KEEP_SETTINGS) return;
  EEPROM.update(0, EEPROM_KEY);
  EEPROM.put(1, paramValues);   // put() пишет только изменившиеся байты
}

void loadSettings() {
  loadDefaults();
  if (KEEP_SETTINGS) {
    if (RESET_SETTINGS || EEPROM.read(0) != EEPROM_KEY) {
      saveSettings();             // первый запуск - записать значения по умолчанию
    } else {
      EEPROM.get(1, paramValues);
      for (uint8_t i = 0; i < P_COUNT; i++) {   // на всякий случай загоняем в границы
        ParamMeta m = readMeta(i);
        paramValues[i] = constrain(paramValues[i], m.min, m.max);
      }
      if (!KEEP_STATE) paramValues[P_POWER] = 1;
    }
  }
  FastLED.setBrightness(param(P_BRIGHTNESS));
}

void eepromTick() {
  if (eeprom_flag && millis() - eeprom_timer > 30000) saveSettings();
}

// ---------------- калибровка шумов ----------------
void autoLowPass() {
  // для режима VU
  delay(10);                                // ждём инициализации АЦП
  int thisMax = 0;                          // максимум
  int thisLevel;
  for (byte i = 0; i < 200; i++) {
    thisLevel = analogRead(SOUND_R);        // делаем 200 измерений
    if (thisLevel > thisMax)                // ищем максимумы
      thisMax = thisLevel;                  // запоминаем
    delay(4);                               // ждём 4мс
  }
  setParam(P_LOW_PASS, thisMax + LOW_PASS_ADD);   // нижний порог как максимум тишины + некая величина

  // для режима спектра
  thisMax = 0;
  for (byte i = 0; i < 100; i++) {          // делаем 100 измерений
    analyzeAudio();                         // разбить в спектр
    for (byte j = 2; j < 32; j++) {         // первые 2 канала - хлам
      thisLevel = fht_log_out[j];
      if (thisLevel > thisMax)              // ищем максимумы
        thisMax = thisLevel;                // запоминаем
    }
    delay(4);                               // ждём 4мс
  }
  setParam(P_SPEKTR_LOW_PASS, thisMax + LOW_PASS_FREQ_ADD);  // нижний порог как максимум тишины
}

void calibrateNoise() {
  digitalWrite(MLED_PIN, MLED_ON);    // включить светодиод
  FastLED.setBrightness(0);           // погасить ленту
  FastLED.clear();                    // очистить массив пикселей
  FastLED.show();                     // отправить значения на ленту
  delay(500);                         // подождать чутка
  autoLowPass();                      // измерить шумы
  delay(500);                         // подождать
  FastLED.setBrightness(param(P_BRIGHTNESS));  // вернуть яркость
  digitalWrite(MLED_PIN, !MLED_ON);   // выключить светодиод
  if (!AUTO_LOW_PASS) saveSettings();
}

// ---------------- команды ----------------
void handleCommand(uint8_t cmd, uint8_t id, int16_t value) {
  switch (cmd) {
    case CMD_SET:
      if (!setParam(id, value) && id < P_COUNT) reportParam(id);  // не изменилось - всё равно ответить
      break;
    case CMD_ADD:
      if (id < P_COUNT && !setParam(id, param(id) + value)) reportParam(id);
      break;
    case CMD_GET:
      if (id == 0xFF) for (uint8_t i = 0; i < P_COUNT; i++) reportParam(i);
      else if (id < P_COUNT) reportParam(id);
      break;
    case CMD_EVENT: sendEvent(EV_USER, id, value);
      break;
    case CMD_SAVE: saveSettings();
      break;
    case CMD_RESET:
      for (uint8_t i = 0; i < P_COUNT; i++) setParam(i, readMeta(i).def);
      break;
    case CMD_CALIBRATE: calibrateNoise();
      break;
  }
}

#if CONTROL_SERIAL
// разбор пакетов [0xA5][cmd][id][lo][hi] из Serial
void serialTick() {
  static uint8_t buf[4];
  static int8_t pos = -1;     // -1 - ждём стартовый байт
  while (Serial.available()) {
    uint8_t b = Serial.read();
    if (pos < 0) {
      if (b == PACKET_START) pos = 0;
      continue;
    }
    buf[pos++] = b;
    if (pos == 4) {
      pos = -1;
      handleCommand(buf[0], buf[1], (int16_t)(buf[2] | (buf[3] << 8)));
    }
  }
}
#endif
