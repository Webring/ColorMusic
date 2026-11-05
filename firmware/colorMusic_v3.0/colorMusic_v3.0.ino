/*
  Скетч к проекту "Светомузыка на Arduino"
  Страница проекта (схемы, описания): https://alexgyver.ru/colormusic/
  Исходники на GitHub: https://github.com/AlexGyver/ColorMusic
  Автор: AlexGyver Technologies, 2018
  https://AlexGyver.ru/

  Как откалибровать уровень шума расписано на странице проекта! https://alexgyver.ru/colormusic/
*/

/*
   Версия 3.0
   - Режимы вынесены в отдельные файлы (Modes*.h), список режимов - modeList[]
   - ИК пульт убран. Все настройки - параметры с постоянными номерами (Params.h),
     управление командами (Control.h): по Serial или из своего кода через handleCommand()
   - Настройки хранятся в EEPROM одним блоком (формат несовместим с 2.x, при первом запуске сброс)
   - Меньше расход RAM, сборка через PlatformIO
*/


// ***************************** НАСТРОЙКИ *****************************

// ----- управление
#define CONTROL_SERIAL 1    // 1 - принимать команды по Serial (см. Control.h), 0 - только кнопка (экономит ~170 байт RAM)
#define CONTROL_BAUD 9600   // скорость Serial

// ----- настройки параметров
#define KEEP_SETTINGS 1     // хранить ВСЕ настройки в энергонезависимой памяти
#define KEEP_STATE 1        // сохранять в памяти состояние вкл/выкл системы
#define RESET_SETTINGS 0    // сброс настроек в EEPROM памяти (поставить 1, прошиться, поставить обратно 0, прошиться. Всё)

// ----- настройки ленты
#define NUM_LEDS 60         // количество светодиодов
#define CURRENT_LIMIT 3000  // лимит по току в МИЛЛИАМПЕРАХ, автоматически управляет яркостью (пожалей свой блок питания!) 0 - выключить лимит
#define BRIGHTNESS 200      // яркость по умолчанию (0 - 255)

// ----- пины подключения
#define SOUND_R A2         // аналоговый пин вход аудио, правый канал
#define SOUND_L A1         // аналоговый пин вход аудио, левый канал
#define SOUND_R_FREQ A3    // аналоговый пин вход аудио для режима с частотами (через кондер)
#define BTN_PIN 3          // кнопка переключения режимов (PIN --- КНОПКА --- GND)

#if defined(__AVR_ATmega32U4__) // Пины для Arduino Pro Micro (смотри схему для Pro Micro на странице проекта!!!)
#define MLED_PIN 17             // пин светодиода режимов на ProMicro, т.к. обычный не выведен.
#define MLED_ON LOW
#define LED_PIN 9               // пин DI светодиодной ленты на ProMicro, т.к. обычный не выведен.
#else                           // Пины для других плат Arduino (по умолчанию)
#define MLED_PIN 13             // пин светодиода режимов
#define MLED_ON HIGH
#define LED_PIN 12              // пин DI светодиодной ленты
#endif

#define POT_GND A0              // пин земля для потенциометра

// ----- настройки радуги
#define RAINBOW_STEP 5.00       // шаг изменения цвета радуги

// ----- отрисовка
#define MODE 0                    // режим при запуске
#define MAIN_LOOP 5               // период основного цикла отрисовки (по умолчанию 5)

// ----- сигнал
#define MONO 1                    // 1 - только один канал (ПРАВЫЙ!!!!! SOUND_R!!!!!), 0 - два канала
#define EXP 1.4                   // степень усиления сигнала (для более "резкой" работы) (по умолчанию 1.4)
#define POTENT 0                  // 1 - используем потенциометр, 0 - используется внутренний источник опорного напряжения 1.1 В
#define EMPTY_BRIGHT 30           // яркость "не горящих" светодиодов (0 - 255)
#define EMPTY_COLOR HUE_PURPLE    // цвет "не горящих" светодиодов. Будет чёрный, если яркость 0

// ----- нижний порог шумов
#define LOW_PASS 100              // нижний порог шумов режим VU, ручная настройка
#define SPEKTR_LOW_PASS 40        // нижний порог шумов режим спектра, ручная настройка
#define AUTO_LOW_PASS 0           // настраивать нижний порог шумов при каждом запуске (по умолч. 0)
#define LOW_PASS_ADD 13           // "добавочная" величина к нижнему порогу, для надёжности (режим VU)
#define LOW_PASS_FREQ_ADD 3       // "добавочная" величина к нижнему порогу, для надёжности (режим частот)

// ----- режим шкала громкости
#define SMOOTH 0.3                // коэффициент плавности анимации VU (по умолчанию 0.5)
#define MAX_COEF 1.8              // коэффициент громкости (максимальное равно срднему * этот коэф) (по умолчанию 1.8)

// ----- режим цветомузыки
#define SMOOTH_FREQ 0.8           // коэффициент плавности анимации частот (по умолчанию 0.8)
#define MAX_COEF_FREQ 1.2         // коэффициент порога для "вспышки" цветомузыки (по умолчанию 1.5)
#define SMOOTH_STEP 20            // шаг уменьшения яркости в режиме цветомузыки (чем больше, тем быстрее гаснет)
#define LOW_COLOR HUE_RED         // цвет низких частот
#define MID_COLOR HUE_GREEN       // цвет средних
#define HIGH_COLOR HUE_YELLOW     // цвет высоких

// ----- режим огня
#define FIRE_HUE 0                // цвет огня (0 красный, 80 зелёный, 140 молния, 190 розовый)
#define FIRE_SPEED 20             // скорость движения пламени
#define FIRE_SCALE 15             // масштаб пламени: меньше - крупнее языки
#define FIRE_HUE_GAP 21           // разброс цвета от FIRE_HUE
#define FIRE_MIN_BRIGHT 70        // мин. яркость пламени
#define FIRE_MAX_BRIGHT 255       // макс. яркость пламени
#define FIRE_MIN_SAT 245          // мин. насыщенность
#define FIRE_MAX_SAT 255          // макс. насыщенность

// ----- режим подсветки
#define LIGHT_COLOR 0             // начальный цвет подсветки
#define LIGHT_SAT 255             // начальная насыщенность подсветки
#define COLOR_SPEED 100
#define RAINBOW_PERIOD 1
#define RAINBOW_STEP_2 0.5

// ----- режим бегущих частот
#define RUNNING_SPEED 11

// ----- режим анализатора спектра
#define HUE_START 0
#define HUE_STEP 5
#define LIGHT_SMOOTH 2

/*
  Цвета для HSV
  HUE_RED
  HUE_ORANGE
  HUE_YELLOW
  HUE_GREEN
  HUE_AQUA
  HUE_BLUE
  HUE_PURPLE
  HUE_PINK
*/


// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ --------------------------------
#define STRIPE NUM_LEDS / 5
#define MAX_CH (NUM_LEDS / 2)

#define FHT_N 64         // ширина спектра х2
#define LOG_OUT 1
#include <FHT.h>         // преобразование Хартли

#define FASTLED_ALLOW_INTERRUPTS 1
#include "FastLED.h"
CRGB leds[NUM_LEDS];

#include "GyverButton.h"
GButton butt1(BTN_PIN);

unsigned long main_timer;

#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

#include "Params.h"
#include "Mode.h"
#include "Audio.h"
#include "ModesVu.h"
#include "ModesFreq.h"
#include "ModesLight.h"

// ----- список режимов: кнопка переключает по кругу, P_MODE выбирает по номеру
const ModeDesc modeList[] PROGMEM = {
  // update                   onEvent              флаги
  {VuGradientMode::update,    nullptr,             0},              // 0 - шкала громкости, градиент
  {VuRainbowMode::update,     nullptr,             0},              // 1 - шкала громкости, радуга
  {FiveStripesMode::update,   nullptr,             0},              // 2 - цветомузыка, 5 полос
  {ThreeStripesMode::update,  nullptr,             0},              // 3 - цветомузыка, 3 полосы
  {FlashMode::update,         nullptr,             0},              // 4 - цветомузыка, вспышки (P_FREQ_STROBE_MODE)
  {FireMode::update,          nullptr,             0},              // 5 - огонь
  {LightMode::update,         nullptr,             0},              // 6 - подсветка (P_LIGHT_MODE)
  {RunningMode::update,       nullptr,             MODE_NO_CLEAR},  // 7 - бегущие частоты (P_FREQ_STROBE_MODE)
  {SpectrumMode::update,      nullptr,             0},              // 8 - анализатор спектра
};
const uint8_t MODE_COUNT = sizeof(modeList) / sizeof(modeList[0]);

// ----- границы и значения по умолчанию, строго в порядке ParamId (Params.h)
const ParamMeta paramMeta[] PROGMEM = {
  // min    max             по умолчанию
  {0,       MODE_COUNT - 1, MODE},                    // P_MODE
  {0,       1,              1},                       // P_POWER
  {0,       255,            BRIGHTNESS},              // P_BRIGHTNESS
  {0,       255,            EMPTY_BRIGHT},            // P_EMPTY_BRIGHT
  {0,       1023,           LOW_PASS},                // P_LOW_PASS
  {0,       255,            SPEKTR_LOW_PASS},         // P_SPEKTR_LOW_PASS
  {5,       100,            FIX100(SMOOTH)},          // P_SMOOTH, сотые
  {5,       200,            FIX10(RAINBOW_STEP)},     // P_RAINBOW_STEP, десятые
  {5,       100,            FIX100(SMOOTH_FREQ)},     // P_SMOOTH_FREQ, сотые
  {0,       100,            FIX10(MAX_COEF_FREQ)},    // P_MAX_COEF_FREQ, десятые
  {0,       3,              0},                       // P_FREQ_STROBE_MODE
  {0,       255,            FIRE_HUE},                // P_FIRE_HUE
  {1,       100,            FIRE_SPEED},              // P_FIRE_SPEED
  {0,       2,              0},                       // P_LIGHT_MODE
  {0,       255,            LIGHT_COLOR},             // P_LIGHT_COLOR
  {0,       255,            LIGHT_SAT},               // P_LIGHT_SAT
  {0,       255,            COLOR_SPEED},             // P_COLOR_SPEED
  {-20,     20,             RAINBOW_PERIOD},          // P_RAINBOW_PERIOD
  {5,       100,            FIX10(RAINBOW_STEP_2)},   // P_RAINBOW_STEP_2, десятые
  {1,       255,            RUNNING_SPEED},           // P_RUNNING_SPEED
  {0,       255,            HUE_START},               // P_HUE_START
  {1,       255,            HUE_STEP},                // P_HUE_STEP
  {1,       100,            FIRE_SCALE},              // P_FIRE_SCALE
};
static_assert(sizeof(paramMeta) / sizeof(paramMeta[0]) == P_COUNT, "paramMeta[] не совпадает с ParamId");

#include "Control.h"
// ------------------------------ ДЛЯ РАЗРАБОТЧИКОВ --------------------------------

void setup() {
#if CONTROL_SERIAL
  Serial.begin(CONTROL_BAUD);
#endif
  FastLED.addLeds<WS2811, LED_PIN, GRB>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  if (CURRENT_LIMIT > 0) FastLED.setMaxPowerInVoltsAndMilliamps(5, CURRENT_LIMIT);

#if defined(__AVR_ATmega32U4__)   //Выключение светодиодов на Pro Micro
  TXLED1;                           //на ProMicro выключим и TXLED
  delay (1000);                     //При питании по usb от компьютера нужна задержка перед выключением RXLED. Если питать от БП, то можно убрать эту строку.
#endif
  pinMode(MLED_PIN, OUTPUT);        //Режим пина для светодиода режима на выход
  digitalWrite(MLED_PIN, !MLED_ON); //Выключение светодиода режима

  pinMode(POT_GND, OUTPUT);
  digitalWrite(POT_GND, LOW);
  butt1.setTimeout(900);

  // для увеличения точности уменьшаем опорное напряжение,
  // выставив EXTERNAL и подключив Aref к выходу 3.3V на плате через делитель
  // GND ---[10-20 кОм] --- REF --- [10 кОм] --- 3V3
  // в данной схеме GND берётся из А0 для удобства подключения
  if (POTENT) analogReference(EXTERNAL);
  else
#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    analogReference(INTERNAL1V1);
#else
    analogReference(INTERNAL);
#endif

  // жуткая магия, меняем частоту оцифровки до 18 кГц
  // команды на ебучем ассемблере, даже не спрашивайте, как это работает
  // поднимаем частоту опроса аналогового порта до 38.4 кГц, по теореме
  // Котельникова (Найквиста) частота дискретизации будет 19.2 кГц
  // http://yaab-arduino.blogspot.ru/2015/02/fast-sampling-from-analog-input.html
  sbi(ADCSRA, ADPS2);
  cbi(ADCSRA, ADPS1);
  sbi(ADCSRA, ADPS0);

  loadSettings();
  if (AUTO_LOW_PASS) autoLowPass();   // настройка нижнего порога шумов при запуске
  sendEvent(EV_ENTER);                // сообщить стартовому режиму, что он включён
}

void loop() {
  buttonTick();     // опрос и обработка кнопки
#if CONTROL_SERIAL
  serialTick();     // приём команд
#endif
  mainLoop();       // главный цикл обработки и отрисовки
  eepromTick();     // проверка не пора ли сохранить настройки
}

void mainLoop() {
  // главный цикл отрисовки
  if (param(P_POWER) && millis() - main_timer > MAIN_LOOP) {
    ModeDesc mode = readMode(param(P_MODE));
    mode.update();              // обработать звук и нарисовать кадр
    FastLED.show();             // отправить значения на ленту
    if (!(mode.flags & MODE_NO_CLEAR))
      FastLED.clear();          // очистить массив пикселей
    main_timer = millis();      // сбросить таймер
  }
}

void buttonTick() {
  butt1.tick();  // обязательная функция отработки. Должна постоянно опрашиваться
  if (butt1.isSingle())       // если единичное нажатие - следующий режим
    setParam(P_MODE, (param(P_MODE) + 1) % MODE_COUNT);

  if (butt1.isHolded())       // кнопка удержана - калибровка шумов
    calibrateNoise();
}
