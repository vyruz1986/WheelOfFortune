#include <Arduino.h>
#include <WS2812FX.h>

#define WHEEL_NUM_LEDS 512
#define WHEEL_NUM_SEGMENTS 8
#define WHEEL_LEDS_PER_SEGMENT 64
#define WHEEL_DATA_PIN 13
WS2812FX wheel = WS2812FX(WHEEL_NUM_LEDS, WHEEL_DATA_PIN, NEO_GRB + NEO_KHZ800);

#define BUTTON_LED_DATA_PIN 14
#define BUTTON_LED_NUM_LEDS 1
WS2812FX button = WS2812FX(BUTTON_LED_NUM_LEDS, BUTTON_LED_DATA_PIN, NEO_GRB + NEO_KHZ800);

#define MAX_SPINNING_TIME 15       // seconds, max time we will spin for
#define MIN_SPINNING_TIME 5        // seconds, min time we will spin for
#define COOLDOWN_AFTER_SPINNING 10 // seconds, time to remain on last active segment when we stop spinning, before restarting idle-animation

#define BUTTON_PIN 27
#define BUTTON_DEBOUNCE 500

static unsigned long spinningStopTime = 0;
static uint spinningDelay = 100;
static unsigned long lastSegmentSwitch = 0;
static uint8_t currentActiveSegment = 0;
static unsigned long cooldownStop = 0;
static unsigned long nextButtonCheck = 0;

enum DeviceMode
{
  INITIAL,
  IDLE,
  SPINNING,
  COOLDOWN
};

struct segmentConfig
{
  uint startLed;
  uint stopLed;
};

enum DeviceMode currentMode;

void checkButton();
void startSpinning();
void handleWheelOfFortune();
void setSegmentToColor(uint8_t segmentNumber, uint32_t color);
void setIdleModes();
void setCooldownModes();
void setSpinningModes();
segmentConfig getSegmentConfig(uint8_t segmentNumber);

void setup()
{
  Serial.begin(115200);
  wheel.init();
  wheel.setBrightness(80);
  wheel.setColor(BLACK);

  button.init();

  setIdleModes();

  wheel.start();
  button.start();

  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void loop()
{
  checkButton();
  handleWheelOfFortune();
  wheel.service();
  button.service();
}

void checkButton()
{
  auto now = millis();
  if (now >= nextButtonCheck && digitalRead(BUTTON_PIN) == LOW)
  {
    nextButtonCheck = now + BUTTON_DEBOUNCE;
    startSpinning();
  }
}

void startSpinning()
{
  spinningStopTime = millis() + random(7000, 15000);
  cooldownStop = spinningStopTime + (COOLDOWN_AFTER_SPINNING * 1000);
  spinningDelay = 100;
  setSpinningModes();
}

void handleWheelOfFortune()
{
  if (currentMode != DeviceMode::SPINNING && currentMode != DeviceMode::COOLDOWN)
    return;
  auto now = millis();

  auto spinningStopped = now >= spinningStopTime;
  auto cooldownActive = spinningStopped && now <= cooldownStop;

  if (spinningStopped)
  {
    if (cooldownActive)
    {
      setCooldownModes();
      return;
    }

    setIdleModes();
    return;
  }

  auto nextSpinningCycleNeeded = now - lastSegmentSwitch > spinningDelay;
  if (!nextSpinningCycleNeeded)
  {
    return;
  }

  lastSegmentSwitch = now;
  spinningDelay = spinningDelay + 10;

  currentActiveSegment = currentActiveSegment == 7
                             ? 0
                             : currentActiveSegment + 1;
  auto previousSegment = currentActiveSegment == 0
                             ? 7
                             : currentActiveSegment - 1;

  setSegmentToColor(previousSegment, BLACK);
  setSegmentToColor(currentActiveSegment, GREEN);
  wheel.show();
}

void setSegmentToColor(uint8_t segmentNumber, uint32_t color)
{
  auto config = getSegmentConfig(segmentNumber);

  for (uint i = config.startLed; i <= config.stopLed; i++)
  {
    wheel.setPixelColor(i, color);
  }
}

segmentConfig getSegmentConfig(uint8_t segmentNumber)
{
  segmentConfig config;
  config.startLed = segmentNumber * WHEEL_LEDS_PER_SEGMENT;
  config.stopLed = config.startLed + WHEEL_LEDS_PER_SEGMENT - 1;

  return config;
}

void setIdleModes()
{
  if (currentMode == DeviceMode::IDLE)
    return;

  currentMode = DeviceMode::IDLE;

  wheel.resetSegments();
  wheel.setSegment(0, 0, WHEEL_NUM_LEDS - 1, FX_MODE_RAINBOW_CYCLE, BLACK, 5000);
  button.setColor(GREEN);
  button.setMode(FX_MODE_BREATH);
}

void setSpinningModes()
{
  if (currentMode == DeviceMode::SPINNING)
    return;

  currentMode = DeviceMode::SPINNING;

  wheel.setMode(FX_MODE_STATIC);
  button.setMode(FX_MODE_STATIC);
  button.setColor(RED);
}

void setCooldownModes()
{
  if (currentMode == DeviceMode::COOLDOWN)
    return;

  currentMode = DeviceMode::COOLDOWN;
  wheel.setSpeed(1000);
  wheel.setColor(GREEN);

  auto config = getSegmentConfig(currentActiveSegment);

  wheel.setSegment(0, config.startLed, config.stopLed, FX_MODE_BLINK, GREEN, 100);
}