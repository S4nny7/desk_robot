#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_FT6206.h>

#include "robot_happy.h"
#include "robot_blush.h"
#include "robot_joy.h"

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
unsigned long touchStartTime = 0;

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

    Serial.println("TOUCHED!");

    // Stop all face animations
    showingHappy = false;
    showingBlush = false;
    showingJoy = false;

    // Clear robot face
    display.clearDisplay();

    // Show TOUCHED!
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 25);

    display.println("TOUCHED!");

    display.display();

    // Start 3 second timer
    touchStartTime = millis();

    touchActive = true;
  }

  // ==========================================
  // 3 SECOND TIMER
  // ==========================================

  if (touchActive && millis() - touchStartTime >= 3000)
  {

    Serial.println("3 seconds finished!");

    // Clear TOUCHED!
    display.clearDisplay();
    display.display();

    // Show JOY face
    showJoyFace();

    showingJoy = true;

    // Touch interaction finished
    touchActive = false;

    // Prevent another trigger while finger
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
  // HAPPY → BLUSH AFTER 1 SECOND
  // ==========================================

  if (
      !touchActive &&
      !touchCooldown &&
      showingHappy &&
      millis() - happyStartTime >= 1000)
  {

    showBlushFace();

    showingHappy = false;
    showingBlush = true;
  }
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
      epd_bitmap_robot_happy1,
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