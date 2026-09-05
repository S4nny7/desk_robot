#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FT6206.h>

#include "robot_happy.h"
#include "robot_blush.h"
#include "robot_joy.h"
#include "robot_blink1.h"
#include "robot_blink2.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#define BUTTON_PIN 26

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET);

// FT6206 capacitive touch controller
Adafruit_FT6206 touch = Adafruit_FT6206();

// ==========================================
// VARIABLES
// ==========================================

unsigned long happyStartTime = 0;

bool showingHappy = false;
bool showingBlush = false;
bool showingJoy = false;

bool touchActive = false;
bool touchCooldown = false;

// ==========================================
// SETUP
// ==========================================

void setup()
{

  Serial.begin(115200);

  // ==========================================
  // I2C
  // SDA = GPIO 21
  // SCL = GPIO 22
  // ==========================================

  Wire.begin(21, 22);

  // ==========================================
  // JOURNAL BUTTON
  // ==========================================

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Starting Mochi...");

  // ==========================================
  // OLED
  // ==========================================

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {

    Serial.println("OLED not found!");

    while (true)
    {
      delay(100);
    }
  }

  Serial.println("OLED connected!");

  // ==========================================
  // FT6206 TOUCH
  // ==========================================

  if (!touch.begin(40))
  {

    Serial.println("FT6206 touch controller not found!");

    while (true)
    {
      delay(100);
    }
  }

  Serial.println("FT6206 touch connected!");

  // ==========================================
  // START WITH HAPPY FACE
  // ==========================================

  showHappyFace();

  showingHappy = true;
}

// ==========================================
// LOOP
// ==========================================

void loop()
{

  // ==========================================
  // JOURNAL BUTTON
  // ==========================================

  if (digitalRead(BUTTON_PIN) == LOW)
  {

    Serial.println("Button pressed!");

    showHappyFace();

    happyStartTime = millis();

    showingHappy = true;
    showingBlush = false;
    showingJoy = false;

    // Wait for button release
    while (digitalRead(BUTTON_PIN) == LOW)
    {
      delay(10);
    }

    // Debounce
    delay(50);
  }

  // ==========================================
  // CAPACITIVE TOUCH
  // ==========================================

  if (touch.touched() && !touchActive && !touchCooldown)
  {

    Serial.println("BLINK!");

    // Stop other face animations
    showingHappy = false;
    showingBlush = false;
    showingJoy = false;

    // Start blink animation
    touchActive = true;

    // ------------------------------------------
    // FRAME 1 - HAPPY
    // ------------------------------------------

    showHappyFace();

    delay(100);

    // ------------------------------------------
    // FRAME 2 - BLINK 1
    // ------------------------------------------

    showBlink1();

    delay(70);

    // ------------------------------------------
    // FRAME 3 - BLINK 2
    // ------------------------------------------

    showBlink2();

    delay(60);

    showBlink1();

    delay(70);

    // ------------------------------------------
    // FRAME 4 - HAPPY AGAIN
    // ------------------------------------------

    showHappyFace();

    Serial.println("Blink finished!");

    // Animation finished
    touchActive = false;

    // Prevent another blink while finger
    // is still touching
    touchCooldown = true;
  }

  // ==========================================
  // FINGER HAS BEEN RELEASED
  // ==========================================

  if (!touch.touched() && touchCooldown)
  {

    Serial.println("Touch released - ready again!");

    touchCooldown = false;
  }

  // ==========================================
  // HAPPY FACE
  // ==========================================

  void showHappyFace()
  {

    display.clearDisplay();

    display.drawBitmap(
        0,
        0,
        epd_bitmap_happy,
        128,
        64,
        SSD1306_WHITE);

    display.display();
  }

  // ==========================================
  // BLUSH FACE
  // ==========================================

  void showBlushFace()
  {

    display.clearDisplay();

    display.drawBitmap(
        0,
        0,
        epd_bitmap_robot_blush,
        128,
        64,
        SSD1306_WHITE);

    display.display();
  }

  // ==========================================
  // JOY FACE
  // ==========================================

  void showJoyFace()
  {

    display.clearDisplay();

    display.drawBitmap(
        0,
        0,
        epd_bitmap_robot_joy,
        128,
        64,
        SSD1306_WHITE);

    display.display();
  }

  // ==========================================
  // BLINK 1
  // ==========================================

  void showBlink1()
  {

    display.clearDisplay();

    display.drawBitmap(
        0,
        0,
        epd_bitmap_blink_1,
        128,
        64,
        SSD1306_WHITE);

    display.display();
  }

  // ==========================================
  // BLINK 2
  // ==========================================

  void showBlink2()
  {

    display.clearDisplay();

    display.drawBitmap(
        0,
        0,
        epd_bitmap_blink_2,
        128,
        64,
        SSD1306_WHITE);

    display.display();
  }