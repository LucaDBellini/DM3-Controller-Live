// =================================================
// DM3 Controller V7.2.2 (Single-Encoder-Hardware)
// Yamaha DM3 Multi-Channel Remote (Mute/Volume)
// ESP32-S3
//
// Aufgeteilt in mehrere Tabs:
// - DM3_Controller_V7.2.2_SingleEncoder.ino: Konfiguration, Setup, Loop
// - Network.ino: WLAN/DM3-Verbindung, Status-LED
// - Input.ino: Encoder- und Taster-Bedienung
// - Config.ino: Laden/Speichern in Preferences (Flash)
// - Battery.ino: Akku messen und anzeigen
// - Display.ino: OLED-Bildschirme
// - WebConfig.ino: Web-Konfigurationsserver + AP-Fallback
// =================================================
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <esp_wifi.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include <Preferences.h>

// =================================================
// WLAN
// =================================================
// Jetzt veränderbar (statt const char*), da am Gerät über SETTINGS -> WIFI editierbar
#define WIFI_MAX_LEN 32
char wifiSSID[WIFI_MAX_LEN + 1] =
  "YourSSID";
char wifiPassword[WIFI_MAX_LEN + 1] =
  "YourPassword";

// =================================================
// WLAN Bearbeitung (SSID/Passwort)
// =================================================
// Zeichensatz zur Eingabe per Encoder. Ein Index eins hinter dem letzten
// Zeichen (WIFI_CHARSET_LEN) steht für "END" und schließt die Eingabe ab.
const char wifiCharset[] =
  " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()-_=+";
#define WIFI_CHARSET_LEN (sizeof(wifiCharset) - 1)
#define WIFI_VISIBLE_CHARS 18
char wifiEditBuffer[WIFI_MAX_LEN + 1];
int wifiEditLength = 0;
int wifiEditCursor = 0;
int wifiCharIndex = 0;

// =================================================
// WLAN-Profile (gespeicherte Netzwerke zum schnellen Wechseln)
// =================================================
#define WIFI_PROFILE_MAX 5
#define WIFI_NAME_MAX_LEN 16
struct WifiProfile {
  char name[WIFI_NAME_MAX_LEN + 1];
  char ssid[WIFI_MAX_LEN + 1];
  char pass[WIFI_MAX_LEN + 1];
  bool used;
};
WifiProfile wifiProfiles[WIFI_PROFILE_MAX];
// Welches Feld gerade bearbeitet wird (Name/SSID/Passwort teilen sich
// denselben Zeichen-Editor-Bildschirm) und in welchem Kontext:
// wifiEditIsNew=true -> Teil des NEW-Ablaufs (Name->SSID->Passwort
// verkettet, am Ende speichern+verbinden); false -> einzelnes Feld eines
// bestehenden Profils bearbeiten (danach zurück ins Profil-Untermenü).
enum WifiEditField {
  WEF_NAME,
  WEF_SSID,
  WEF_PASS
};
WifiEditField wifiEditField = WEF_NAME;
bool wifiEditIsNew = false;
int wifiEditProfileIndex = -1;
int wifiSavedListIndex = 0;
int wifiEditListIndex = 0;
int wifiEditItemIndex = 0;
#define WIFI_EDIT_ITEM_COUNT 5

// =================================================
// Web-Konfigurationsserver (Browser-Zugriff auf alle Einstellungen)
// =================================================
// Läuft nur, wenn manuell über SETTINGS -> WEB CONFIG aktiviert (im
// normalen WLAN-Betrieb), ODER automatisch, solange der AP-Fallback aktiv
// ist (siehe unten) — sonst wäre der AP-Modus zur Ersteinrichtung nutzlos.
WebServer server(80);
bool webConfigEnabled = false;
bool webServerRunning = false;

// =================================================
// AP-Fallback (eigener Access Point, wenn keine WLAN-Verbindung klappt)
// =================================================
// SSID enthält eine geräteeigene Kennung (aus der Chip-MAC), damit nicht
// mehrere Controller denselben AP-Namen ausstrahlen. Passwort ist fest
// hinterlegt, aber über SETTINGS -> WEB CONFIG -> AP PASSWORD und über die
// Weboberfläche änderbar (min. 8 Zeichen, WPA2-Vorgabe).
char apSSID[32] = "";
char apPassword[WIFI_MAX_LEN + 1] = "dm3setup1";
bool apFallbackActive = false;
unsigned long staLostSince = 0;
#define AP_FALLBACK_DELAY 20000
// Globaler STA-Retry-Zeitstempel (statt static-lokal in connectWiFi()):
// startApFallback() setzt ihn direkt beim AP-Start auf "jetzt", damit
// nicht im selben Tick, in dem der AP gerade erst hochfährt, sofort noch
// ein WiFi.begin()-Scan mit ihm kollidiert (siehe Kommentar dort).
unsigned long lastWifiTry = 0;

// =================================================
// DM3 Latenz-Messung (durchschnittliche Antwortzeit seit dem letzten Boot)
// =================================================
// Ein Poll-Zyklus (pollDM3()) fragt Pegel+Mute ab (2 "get"-Kommandos).
// Sobald beide zugehörigen Antworten eingetroffen sind, gilt der Zyklus
// als "ausgeführt" und die verstrichene Zeit fließt per gleitendem
// Mittelwert (laufender Durchschnitt) in dm3AvgLatencyMs ein.
unsigned long pollSentAt = 0;
int pollResponsesExpected = 0;
int pollResponsesReceived = 0;
unsigned long dm3AvgLatencyMs = 0;
unsigned long dm3LatencySamples = 0;

// =================================================
// DM3
// =================================================
#define DM3_PORT 49280
WiFiClient dm3;
IPAddress dm3IP(
  0,0,0,0);  // YourMixerIP - IP-Adresse deines DM3 hier eintragen (z.B. 192,168,1,50)

bool dm3Online = false;
unsigned long lastDM3Try = 0;
unsigned long dm3RetryTime = 3000;

// =================================================
// Kanäle (Mute/Volume für ST Master + frei wählbare Input/Mix/Matrix-Kanäle)
// =================================================
// ST Master ist immer da (RCP-Pfad "St", Index 0, kein Slot nötig).
// Zusätzlich 7 frei konfigurierbare Slots: 3x Input, 2x Mix, 2x Matrix.
// Jeder Slot ist einzeln aktivierbar/deaktivierbar (SETTINGS -> CHANNEL ->
// INPUT/MIX/MATRIX -> SLOT n). Langer Druck auf dem MASTER-/Kanal-Screen
// wandert der Reihe nach durch alle AKTIVIERTEN Slots in der Array-Reihenfolge
// (also erst Input, dann Mix, dann Matrix) und landet danach in SETTINGS.
#define CT_INCH 0
#define CT_MIX 1
#define CT_MTRX 2
#define CH_TYPE_COUNT 3
const char* chTypeRcp[CH_TYPE_COUNT] = {
  "InCh","Mix","Mtrx"
};
const char* chTypeLabel[CH_TYPE_COUNT] = {
  "INPUT","MIX","MATRIX"
};
#define INPUT_CHANNEL_TOTAL 16
#define MIX_CHANNEL_TOTAL 6
#define MTRX_CHANNEL_TOTAL 2
int chTypeMax[CH_TYPE_COUNT] = {
  INPUT_CHANNEL_TOTAL,MIX_CHANNEL_TOTAL,MTRX_CHANNEL_TOTAL
};

#define INCH_SLOT_BASE 0
#define INCH_SLOT_COUNT 3
#define MIX_SLOT_BASE 3
#define MIX_SLOT_COUNT 2
#define MTRX_SLOT_BASE 5
#define MTRX_SLOT_COUNT 2
#define EXTRA_SLOT_COUNT 7

#define SLOT_MASTER -1
#define SLOT_NONE -2

struct ChSlot {
  int type;
  int num;
  bool enabled;
};
ChSlot chSlots[EXTRA_SLOT_COUNT] = {
  {CT_INCH,1,false},
  {CT_INCH,2,false},
  {CT_INCH,3,false},
  {CT_MIX,1,false},
  {CT_MIX,2,false},
  {CT_MTRX,1,false},
  {CT_MTRX,2,false}
};
// SLOT_MASTER = ST Master, 0..EXTRA_SLOT_COUNT-1 = Index in chSlots[]
int activeSlot = SLOT_MASTER;

// =================================================
// Kanalnamen (vom DM3 abgefragt)
// =================================================
// Leer bis eine DM3-Verbindung besteht und die Namen synchronisiert wurden.
// Bis dahin zeigt die UI Standard-Platzhalter ("ST MASTER", "CH n", "MIX n",
// "MATRIX n"), danach automatisch die echten, vom DM3 abgefragten Namen.
char stMasterName[9] = "";
char inputChannelNames[INPUT_CHANNEL_TOTAL][9];
char mixChannelNames[MIX_CHANNEL_TOTAL][9];
char matrixChannelNames[MTRX_CHANNEL_TOTAL][9];

// =================================================
// Kanal Bearbeitung (Kanalnummer für den aktuell bearbeiteten Slot)
// =================================================
int channelEditSlot = 0;
int channelEditNum = 1;

// =================================================
// Status LED
// =================================================
#define WHITE_LED 35
#define LED_BLINK_TIME 150
#define LED_CONNECTED_TIME 10000
bool ledOn = false;
unsigned long ledOffAt = 0;

// =================================================
// OLED
// =================================================
#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define Vext 36
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  OLED_RST,
  OLED_SCL,
  OLED_SDA);

// =================================================
// Encoder
// =================================================
#define ENC_A 16
#define ENC_B 15
#define ENC_SW 0
ESP32Encoder encoder;
long lastEncoder = 0;
unsigned long buttonStart = 0;
#define LONG_PRESS_TIME 600

// =================================================
// DM3 Werte
// =================================================
int masterLevel = -99999;
bool masterMute = false;
// Sperrfenster nach eigenem Mute-Toggle: verhindert, dass eine noch
// unterwegs befindliche (veraltete) Poll-Antwort unseren gerade
// gesetzten Zustand kurz wieder zurückdreht (sichtbares MUTE-Geflacker).
unsigned long muteOverrideUntil = 0;
#define MUTE_OVERRIDE_TIME 800
// Gleiches Prinzip für den Fader-Pegel: nach einer eigenen Encoder-Änderung
// kurz keine Poll-Antworten übernehmen, sonst überschreibt eine noch
// veraltete Antwort den gerade gesetzten Wert wieder (Zahl springt hoch
// und dann wieder zurück).
unsigned long levelOverrideUntil = 0;
#define LEVEL_OVERRIDE_TIME 400

// =================================================
// Step Werte
// =================================================
float stepValues[] = {
  0.1,
  0.2,
  0.3,
  0.4,
  0.5,
  0.6,
  0.7,
  0.8,
  0.9,
  1.0
};
#define STEP_COUNT 10
int stepIndex = 4;
float encoderStep = 0.5;
bool stepChanged = false;
unsigned long stepLastChange = 0;
#define STEP_TIMEOUT 2500
unsigned long sleepTimeLastChange = 0;

// =================================================
// Menü
// =================================================
enum MENU {
  MENU_MASTER,
  MENU_STEP,
  MENU_SLEEP,
  MENU_SETTINGS,
  MENU_IP,
  MENU_SLEEP_TIME,
  MENU_WIFI,
  MENU_WIFI_SAVED_LIST,
  MENU_WIFI_EDIT_LIST,
  MENU_WIFI_EDIT_ITEM,
  MENU_WIFI_PROFILE_FIELD,
  MENU_WEB_CONFIG,
  MENU_AP_PASS_EDIT,
  MENU_CHANNEL_TYPE,
  MENU_CHANNEL,
  MENU_CHANNEL_ITEM,
  MENU_CHANNEL_SLOT
};
MENU menu = MENU_MASTER;

// =================================================
// Settings Menü
// =================================================
// Reihenfolge: CHANNEL, STEP SIZE, SLEEP TIME, DM3 IP, WIFI, WEB CONFIG, BACK
#define SETTINGS_COUNT 7
int settingsIndex = 0;

// =================================================
// Web-Config-Menü (SERVER ON/OFF, AP PASSWORD, BACK)
// =================================================
#define WEB_CONFIG_COUNT 3
int webConfigMenuIndex = 0;

// =================================================
// WLAN-Menü (gespeicherte Profile zum schnellen Wechseln)
// =================================================
// Reihenfolge: NEW, SAVED, BACK
#define WIFI_MENU_COUNT 3
int wifiMenuIndex = 0;

// =================================================
// Channel-Typ Menü
// =================================================
// Reihenfolge: INPUT, MIX, MATRIX, BACK
#define CHANNEL_TYPE_MENU_COUNT 4
int channelTypeMenuIndex = 0;
// Welcher Typ gerade in MENU_CHANNEL durchsucht wird (CT_INCH/CT_MIX/CT_MTRX)
int channelMenuType = CT_INCH;

// =================================================
// Channel Menü (Slot-Liste für den gewählten Typ)
// =================================================
// Reihenfolge: SLOT 1..N, BACK (N hängt vom Typ ab: 3 Input, 2 Mix, 2 Matrix)
int channelMenuIndex = 0;

// =================================================
// Channel-Item Menü (pro Slot): ACTIVATE/DEACTIVATE, SET CHANNEL, BACK
// =================================================
#define CHANNEL_ITEM_COUNT 3
int channelItemSlot = 0;
int channelItemIndex = 0;

// =================================================
// IP Konfiguration
// =================================================
byte ipOctets[4] = {
  10,0,41,173
};
int ipEditIndex = 0;

// =================================================
// Ruhezustand
// =================================================
unsigned long lastActivity = 0;
unsigned long sleepTimeoutMs = 60000;
#define SLEEP_TIMEOUT_MIN 10000
#define SLEEP_TIMEOUT_MAX 300000
#define SLEEP_TIMEOUT_STEP 10000
#define SLEEP_CONTRAST 10
#define NORMAL_CONTRAST 255

// =================================================
// Akku
// =================================================
#define VBAT_ADC 1
#define VBAT_CTRL 37
#define VBAT_DIVISOR 227.4
// 0% liegt bewusst über dem echten Tiefentladeschluss (~3,0V), als Sicherheitspuffer für die Zellen-Lebensdauer
#define VBAT_EMPTY 3.1
// Ladeschluss schwankt in der Praxis etwas (4.17-4.21V beobachtet) statt exakt 4.2V,
// daher etwas niedriger angesetzt, damit eine volle Zelle zuverlässig 100% zeigt
#define VBAT_FULL 4.15
// Mittelwertbildung gegen ADC-Rauschen
#define VBAT_SAMPLES 8
#define VBAT_MIN_RAW 50
#define BATTERY_READ_INTERVAL 5000
float batteryVoltage = 0;
int batteryPercent = 0;
bool batteryConnected = true;
unsigned long lastBatteryRead = 0;

// =================================================
// Timing
// =================================================
unsigned long lastPoll = 0;
unsigned long lastDraw = 0;
unsigned long bootTime = 0;
#define POLL_TIME 50
#define DM3_WAIT_TIME 10000

// =================================================
// Speicher
// =================================================
Preferences prefs;

// =================================================
// Prototypen
// =================================================
void connectWiFi();
void connectDM3();
void sendDM3(String cmd);
void pollDM3();
void readDM3();
void parseDM3(String msg);
void handleEncoder();
void handleButton();
void loadConfig();
void saveConfig();
void loadIPEdit();
void saveIPConfig();
void saveSleepTimeout();
void saveWifiConfig();
void saveWifiProfiles();
void loadWifiProfileFieldEdit(int profileIndex, WifiEditField field, bool isNew);
void saveWifiProfileField();
void connectToWifiProfile(int profileIndex);
void deleteWifiProfile(int profileIndex);
int wifiProfileCount();
int findFreeWifiProfileSlot();
int wifiProfileIndexAtListPosition(int pos);
int wifiCharsetIndexFor(char c);
void saveWebConfig();
void loadApPassEdit();
void initApSSID();
void startApFallback();
void stopApFallback();
void updateWebServer();
void setupWebServerRoutes();
String channelPath();
int channelIdx();
void resetChannelState();
int nextEnabledSlot(int from);
int typeSlotBase(int type);
int typeSlotCount(int type);
String inputChannelLabel(int num);
String channelSlotLabel(int slotIdx);
String masterLabel();
String extractLabelName(String msg, String marker);
void parseChannelName(String msg, String marker, char arr[][9], int maxIdx);
void requestChannelNames();
void loadChannelSlotEdit(int slot);
void saveChannelSlotEdit();
void toggleChannelEnabled();
void saveChannelConfig();
void ledOnFor(unsigned long ms);
void readBattery();
void drawBattery(int x, int y, int percent);
void drawBatteryEmpty(int x, int y);
void drawBatteryIndicator(int x, int y);
void drawWebIcon(int x, int y);
void drawScreen();

// =================================================
// Setup
// =================================================
void setup() {
  Serial.begin(
    115200);
  delay(200);
  pinMode(
    Vext,
    OUTPUT);
  digitalWrite(
    Vext,
    LOW);
  delay(100);
  oled.begin();
  oled.setFont(
    u8g2_font_6x12_tf);
  pinMode(
    ENC_SW,
    INPUT_PULLUP);
  pinMode(
    WHITE_LED,
    OUTPUT);
  digitalWrite(
    WHITE_LED,
    LOW);
  delay(200);
  ESP32Encoder::useInternalWeakPullResistors =
    puType::up;
  encoder.attachHalfQuad(
    ENC_A,
    ENC_B);
  encoder.clearCount();
  lastEncoder =
    encoder.getCount();
  loadConfig();
  initApSSID();
  setupWebServerRoutes();
  connectWiFi();
  delay(500);
  connectDM3();
  bootTime = millis();
  Serial.println(
    "[t=" + String(bootTime) + "] BOOTTIME SET");
  lastActivity = millis();
  readBattery();
  lastBatteryRead = millis();
  drawScreen();
}

// =================================================
// LOOP
// =================================================
void loop() {
  // =================================================
  // AP-FALLBACK: kein WLAN nach AP_FALLBACK_DELAY -> eigenen AP aufbauen,
  // damit die Weboberfläche auch ganz ohne bestehendes Netz erreichbar ist.
  // Läuft bewusst VOR der "WLAN prüfen"-Sektion: startApFallback() setzt
  // lastWifiTry, damit connectWiFi() weiter unten im selben Tick NICHT
  // sofort noch einen WiFi.begin()-Scan hinterherschickt, der mit dem
  // gerade erst hochfahrenden AP kollidieren und dessen Beacon lahmlegen
  // könnte.
  // =================================================
  if (
    WiFi.status() != WL_CONNECTED) {
    if (
      staLostSince == 0)
      staLostSince = millis();
    if (
      !apFallbackActive && millis() - staLostSince >= AP_FALLBACK_DELAY) {
      startApFallback();
    }
  } else {
    staLostSince = 0;
    if (
      apFallbackActive) {
      stopApFallback();
    }
  }

  // WLAN prüfen
  static bool wifiWasConnected = false;
  if (
    WiFi.status() != WL_CONNECTED) {
    if (
      wifiWasConnected) {
      Serial.println(
        "WIFI LOST");
    }
    wifiWasConnected = false;
    connectWiFi();
  } else if (
    !wifiWasConnected) {
    wifiWasConnected = true;
    Serial.println(
      "[t=" + String(millis()) + "] WIFI CONNECTED " + WiFi.localIP().toString());
  }
  // DM3 prüfen
  if (
    WiFi.status() == WL_CONNECTED && !dm3.connected()) {
    connectDM3();
  }
  updateWebServer();
  // Status LED
  if (
    ledOn && millis() >= ledOffAt) {
    digitalWrite(
      WHITE_LED,LOW);
    ledOn = false;
  }
  // DM3 Daten
  readDM3();
  // DM3 Polling
  if (
    dm3.connected() && millis() - lastPoll >= POLL_TIME) {
    pollDM3();
    lastPoll = millis();
  }
  // Akku
  if (
    millis() - lastBatteryRead >= BATTERY_READ_INTERVAL) {
    readBattery();
    lastBatteryRead = millis();
  }
  // Bedienung
  handleEncoder();
  handleButton();

  // =================================================
  // STEP AUTO EXIT
  // =================================================
  if (
    menu == MENU_STEP && millis() - stepLastChange >= STEP_TIMEOUT) {
    if (
      stepChanged) {
      saveConfig();
      stepChanged = false;
      Serial.println(
        "STEP SAVED");
    }
    menu = MENU_SETTINGS;
    Serial.println(
      "RETURN SETTINGS");
  }

  // =================================================
  // SLEEP TIME AUTO EXIT
  // =================================================
  if (
    menu == MENU_SLEEP_TIME && millis() - sleepTimeLastChange >= STEP_TIMEOUT) {
    saveSleepTimeout();
    menu = MENU_SETTINGS;
    Serial.println(
      "RETURN SETTINGS");
  }

  // =================================================
  // RUHEZUSTAND
  // =================================================
  if (
    menu != MENU_SLEEP && millis() - lastActivity >= sleepTimeoutMs) {
    menu = MENU_SLEEP;
    oled.setContrast(
      SLEEP_CONTRAST);
    Serial.println(
      "SLEEP");
  }
  // OLED

  if (
    millis() - lastDraw >= 50) {
    drawScreen();
    lastDraw = millis();
  }
}
