#pragma once
// Описание режима и события, которые ему передаются.
// Режим - это namespace со своим состоянием и функциями update() и (по желанию) onEvent().
// Чтобы добавить режим: напиши такой namespace и добавь строку в modeList[] в .ino.
// Таблица режимов лежит во flash (PROGMEM), поэтому RAM на неё не тратится.

// что произошло
enum EventType : uint8_t {
  EV_ENTER,   // режим стал активным (id и value не используются)
  EV_PARAM,   // изменился параметр: id - номер параметра (ParamId), value - новое значение
  EV_USER,    // произвольные данные извне (команда CMD_EVENT): смысл id и value задаёт сам режим
};

struct Event {
  uint8_t type;   // EventType
  uint8_t id;
  int16_t value;
};

// флаги режима
#define MODE_NO_CLEAR 1   // не очищать ленту после вывода кадра

struct ModeDesc {
  void (*update)();                 // обработать звук и нарисовать кадр в leds[]
  void (*onEvent)(const Event &e);  // реакция на события, nullptr - не нужна
  uint8_t flags;
};
