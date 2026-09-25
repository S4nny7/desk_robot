#include <Wire.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <WiFi.h>
#include <time.h>
#include <Preferences.h>

#include <FluxGarage_RoboEyes.h>

// Set 1 for Wokwi simulator, 0 for real board (TTP223)
#define WOKWI_SIMULATION 0

#if WOKWI_SIMULATION
#include <Adafruit_FT6206.h>
#include <Adafruit_ILI9341.h>
#endif

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C

#if WOKWI_SIMULATION
#define TFT_CS 15
#define TFT_DC 2
#endif

#define TOUCH_PIN 32
#define BUTTON_PIN 26

#define PET_JOYFUL_DURATION_MS 5500
#define TIRED_HOUR 20 // merlin gets tired at 8pm

// How long each reaction (mood/animation) holds before returning to idle (ms)
#define JOYFUL_DURATION_MS 5000
#define CONFUSED_DURATION_MS 5000
#define ANGRY_DURATION_MS 5200

// How long the "Logged at: HH:MM" status text stays on screen (ms)
#define STATUS_TEXT_JOYFUL_MS (30UL * 60UL * 1000UL) // 30 minutes
#define STATUS_TEXT_REMINDER_MS 10000UL              // 10 seconds (confused/angry)
#define STATUS_TEXT_Y 56                             // bottom row, below the eyes

#if WOKWI_SIMULATION
const char *ssid_Router = "Wokwi-GUEST";
const char *password_Router = "";
#else
const char *ssid_Router = "Telstra8A5240";
const char *password_Router = "k5954s3e24";
#endif

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET);

#if WOKWI_SIMULATION
Adafruit_FT6206 touch = Adafruit_FT6206();
Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
#endif

RoboEyes<Adafruit_SSD1306> roboEyes(display);

struct tm timeInfo;
Preferences journalPrefs;

bool touchCooldown = false;
bool wifiConnected = false;
bool timeIsValid = false;

// ---------- Background mood ----------

bool isAfterTiredTime()
{
  if (!timeIsValid)
  {
    return false;
  }

  return timeInfo.tm_hour >= TIRED_HOUR;
}

void updateBackgroundMood()
{
  if (isAfterTiredTime())
  {
    roboEyes.setMood(TIRED);
  }
  else
  {
    roboEyes.setMood(DEFAULT);
  }
}

void checkTiredTime()
{
  static bool wasTired = false;

  if (!timeIsValid)
  {
    return;
  }

  bool shouldBeTired = isAfterTiredTime();

  if (shouldBeTired && !wasTired)
  {
    Serial.println("Tired time reached - MERLIN IS TIRED.");

    roboEyes.setMood(TIRED);
    wasTired = true;
  }
  else if (!shouldBeTired && wasTired)
  {
    Serial.println("New day - MERLIN IS BACK TO DEFAULT.");

    roboEyes.setMood(DEFAULT);
    wasTired = false;
  }
}

// ---------- Status text overlay ----------

String statusText = "";
unsigned long statusTextExpiry = 0;
bool statusTextVisible = false;

void showStatusText(String text, unsigned long durationMs)
{
  statusText = text;
  statusTextExpiry = millis() + durationMs;
  statusTextVisible = true;
}

void updateStatusTextDisplay()
{
  if (!statusTextVisible)
    return;

  if (millis() >= statusTextExpiry)
  {
    display.fillRect(0, STATUS_TEXT_Y, SCREEN_WIDTH, 8, SSD1306_BLACK);
    display.display();

    statusTextVisible = false;
    statusText = "";
    return;
  }

  display.fillRect(0, STATUS_TEXT_Y, SCREEN_WIDTH, 8, SSD1306_BLACK);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  int16_t x1, y1;
  uint16_t textWidth, textHeight;

  display.getTextBounds(
      statusText,
      0, 0,
      &x1, &y1,
      &textWidth, &textHeight);

  int16_t x = (SCREEN_WIDTH - textWidth) / 2;

  display.setCursor(x, STATUS_TEXT_Y);
  display.print(statusText);

  display.display();
}

String buildLoggedAtText()
{
  unsigned long lastEntryEpoch = journalPrefs.getULong("lastEntryTime", 0);

  if (lastEntryEpoch == 0)
  {
    return "No entry logged yet";
  }

  time_t entryTime = (time_t)lastEntryEpoch;
  struct tm entryTm;
  localtime_r(&entryTime, &entryTm);

  char buf[24];
  snprintf(
      buf,
      sizeof(buf),
      "Logged at: %02d:%02d",
      entryTm.tm_hour,
      entryTm.tm_min);

  return String(buf);
}

void touchSetup()
{
#if WOKWI_SIMULATION
  Serial.println("Starting ILI9341 touch screen...");

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(ILI9341_BLACK);

  Serial.println("ILI9341 connected!");

  Serial.println("Starting FT6206 touch controller...");

  if (!touch.begin(40))
  {
    Serial.println("FT6206 touch controller not found!");

    while (true)
    {
      delay(100);
    }
  }

  Serial.println("FT6206 touch connected!");
#else
  Serial.println("Configuring TTP223 touch sensor...");

  pinMode(TOUCH_PIN, INPUT);

  Serial.println("TTP223 touch sensor ready!");
#endif
}

bool touchIsTouched()
{
#if WOKWI_SIMULATION
  return touch.touched();
#else
  return digitalRead(TOUCH_PIN) == HIGH;
#endif
}

// ---------- WiFi / NTP ----------

bool connectToWiFi()
{
  Serial.println();
  Serial.println("===== WIFI CONNECTION =====");

  WiFi.begin(ssid_Router, password_Router);

  Serial.println(String("Connecting to ") + ssid_Router);

  unsigned long startAttempt = millis();
  const unsigned long timeoutMs = 15000;

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startAttempt < timeoutMs)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("Wi-Fi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.println("===========================");
    return true;
  }

  Serial.println("Wi-Fi connection FAILED (timed out).");
  Serial.println("Continuing without network - check SSID/password.");
  Serial.println("===========================");
  return false;
}

bool getCurrentTime()
{
  if (!wifiConnected)
  {
    Serial.println("Skipping NTP sync - no Wi-Fi.");
    return false;
  }

  Serial.println();
  Serial.println("===== NTP TIME =====");

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  setenv("TZ", "AEST-10AEDT,M10.1.0,M4.1.0/3", 1);
  tzset();

  Serial.println("Waiting for time...");

  unsigned long startAttempt = millis();
  const unsigned long timeoutMs = 10000;

  while (!getLocalTime(&timeInfo) &&
         millis() - startAttempt < timeoutMs)
  {
    delay(200);
  }

  if (timeInfo.tm_year + 1900 < 2020)
  {
    Serial.println("Failed to get time!");
    timeIsValid = false;
    return false;
  }

  timeIsValid = true;

  Serial.println("Time received!");
  Serial.println(&timeInfo, "%A, %d %B %Y");
  Serial.println(&timeInfo, "Time: %H:%M:%S");
  Serial.println("====================");

  return true;
}

void printCurrentDateTime()
{
  if (!getLocalTime(&timeInfo))
  {
    Serial.println("Could not get current time.");
    return;
  }

  Serial.println();
  Serial.println("===== CURRENT DATE/TIME =====");

  Serial.printf(
      "Date: %02d-%02d-%04d\n",
      timeInfo.tm_mday,
      timeInfo.tm_mon + 1,
      timeInfo.tm_year + 1900);

  Serial.printf(
      "Time: %02d:%02d:%02d\n",
      timeInfo.tm_hour,
      timeInfo.tm_min,
      timeInfo.tm_sec);

  Serial.println("=============================");
}

// ---------- Journal streak tracking ----------
// NVS keys: trackDay, pressCount, lastEntryDay, streak, lastEntryTime

long getEpochDay()
{
  time_t nowEpoch = mktime(&timeInfo);
  return (long)(nowEpoch / 86400L);
}

void joyfulReaction(long todayEpochDay)
{
  Serial.println("MERLIN IS JOYFUL! Journal entry logged.");

  long lastEntryDay = journalPrefs.getLong("lastEntryDay", -1);
  int streak = journalPrefs.getInt("streak", 0);

  streak = (lastEntryDay == todayEpochDay - 1) ? streak + 1 : 1;

  journalPrefs.putLong("lastEntryDay", todayEpochDay);
  journalPrefs.putInt("streak", streak);
  journalPrefs.putULong("lastEntryTime", (unsigned long)mktime(&timeInfo));

  Serial.printf("Current streak: %d day(s)\n", streak);
  Serial.println(&timeInfo, "Logged at: %A, %d %B %Y %H:%M:%S");

  showStatusText(buildLoggedAtText(), STATUS_TEXT_JOYFUL_MS);

  roboEyes.setMood(HAPPY);
  roboEyes.anim_laugh();

  unsigned long startTime = millis();
  while (millis() - startTime < JOYFUL_DURATION_MS)
  {
    roboEyes.update();
    updateStatusTextDisplay();
    delay(30);
  }

  updateBackgroundMood();
}

void confusedReaction()
{
  Serial.println("MERLIN IS CONFUSED! You already journaled today.");

  showStatusText(buildLoggedAtText(), STATUS_TEXT_REMINDER_MS);

  roboEyes.anim_confused();

  unsigned long startTime = millis();
  while (millis() - startTime < CONFUSED_DURATION_MS)
  {
    roboEyes.update();
    updateStatusTextDisplay();
    delay(10);
  }

  updateBackgroundMood();
}

void angryReaction()
{
  Serial.println("MERLIN IS ANGRY! Stop pressing, one entry per day.");

  showStatusText(buildLoggedAtText(), STATUS_TEXT_REMINDER_MS);

  roboEyes.setMood(ANGRY);

  unsigned long startTime = millis();
  while (millis() - startTime < ANGRY_DURATION_MS)
  {
    roboEyes.update();
    updateStatusTextDisplay();
    delay(20);
  }

  updateBackgroundMood();
}

void handleJournalButtonPress()
{
  if (!timeIsValid)
  {
    Serial.println("Can't log journal entry - time not synced (check Wi-Fi/NTP).");
    return;
  }

  long todayEpochDay = getEpochDay();
  long storedDay = journalPrefs.getLong("trackDay", -1);
  int pressCount;

  if (storedDay != todayEpochDay)
  {
    pressCount = 0;
    journalPrefs.putLong("trackDay", todayEpochDay);
  }
  else
  {
    pressCount = journalPrefs.getInt("pressCount", 0);
  }

  pressCount++;
  journalPrefs.putInt("pressCount", pressCount);

  Serial.printf("Button press #%d today.\n", pressCount);

  if (pressCount == 1)
    joyfulReaction(todayEpochDay);
  else if (pressCount == 2)
    confusedReaction();
  else
    angryReaction();
}

// TO DO: IF button is not pressed for a day, robot will be sad tomorrow, streak lost, and
// keeps track of new streak and back to default when button is pressed again OR if robot is
// patted on head few times, becomes happy again

// ---------- Touch reaction ----------

void touchReaction()
{
  Serial.println("MERLIN PETTED! Merlin is joyful!");

  roboEyes.setMood(HAPPY);
  roboEyes.anim_laugh();

  unsigned long startTime = millis();

  while (millis() - startTime < PET_JOYFUL_DURATION_MS)
  {
    roboEyes.update();
    updateStatusTextDisplay();
    delay(20);
  }

  // Return to normal background mood
  updateBackgroundMood();

  touchCooldown = true;
}

// ---------- Setup ----------

void setup()
{
  Serial.begin(115200);
  delay(2000);

  Serial.println();
  Serial.println("=================================");
  Serial.println("       MERLIN JOURNAL ROBOT");
  Serial.println("=================================");

  Wire.begin(21, 22);
  Serial.println("I2C started.");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Journal button configured.");

  Serial.println("Starting OLED...");

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS))
  {
    Serial.println("OLED not found!");

    while (true)
    {
      delay(100);
    }
  }

  Serial.println("OLED connected!");

  touchSetup();

  journalPrefs.begin("journal", false);
  Serial.printf(
      "Loaded streak from flash: %d day(s)\n",
      journalPrefs.getInt("streak", 0));

  Serial.println("Starting RoboEyes...");

  roboEyes.begin(SCREEN_WIDTH, SCREEN_HEIGHT, 60);
  roboEyes.setDisplayColors(0, 1);
  roboEyes.setWidth(40, 40);
  roboEyes.setHeight(40, 40);
  roboEyes.setBorderradius(10, 10);
  roboEyes.setSpacebetween(10);
  roboEyes.setMood(DEFAULT);
  roboEyes.setAutoblinker(ON, 3, 2);
  roboEyes.setIdleMode(ON, 2, 2);

  Serial.println();
  Serial.println("Connecting to Wi-Fi...");

  wifiConnected = connectToWiFi();

  if (wifiConnected)
  {
    getCurrentTime();
    printCurrentDateTime();
  }
  updateBackgroundMood();

  Serial.println();
  Serial.println("----------------------------");
  Serial.println("RoboEyes test ready!");
  Serial.println("Touch = blink");
  Serial.println("Button = journal entry");
  Serial.print("Wi-Fi = ");
  Serial.println(wifiConnected ? "connected" : "NOT connected");
  Serial.print("NTP = ");
  Serial.println(wifiConnected ? "synchronised" : "skipped");
  Serial.println("----------------------------");
}

// ---------- Loop ----------

void loop()
{
  roboEyes.update();
  updateStatusTextDisplay();

  // Update the current time every second
  static unsigned long lastTimeCheck = 0;

  if (millis() - lastTimeCheck >= 1000)
  {
    lastTimeCheck = millis();

    if (getLocalTime(&timeInfo))
    {
      timeIsValid = true;
      checkTiredTime();
    }
  }

  if (digitalRead(BUTTON_PIN) == LOW)
  {
    Serial.println("BUTTON PRESSED");

    handleJournalButtonPress();

    while (digitalRead(BUTTON_PIN) == LOW)
    {
      roboEyes.update();
      updateStatusTextDisplay();
      delay(10);
    }

    delay(50);
  }

  if (touchIsTouched() && !touchCooldown)
  {
    touchReaction();
  }

  if (!touchIsTouched() && touchCooldown)
  {
    Serial.println("TOUCH RELEASED");
    touchCooldown = false;
  }
}