// =================================================
// DM3 Controller V7.6 (NUR Dual-Encoder + Triple-Display-Hardware)
// Yamaha DM3 Multi-Channel Remote (Mute/Volume)
// ESP32-S3, zwei unabhängige Encoder (zwei Kanäle gleichzeitig),
// zwei eigenständige 128x32 SSD1306-Displays (eins pro Kanal, immer aktiv)
// PLUS das Heltec-Onboard-OLED (128x64) als dediziertes Menü-Display
//
// Läuft NICHT auf der Single-Encoder-Hardware (siehe DM3_Controller_V7.2.1_SingleEncoder/),
// NICHT auf der Dual-Encoder-Hardware mit nur einem OLED (siehe DM3_Controller_V7.3_DualEncoder/)
// und NICHT auf der Dual-Encoder/Dual-Display-Hardware ohne genutztes Onboard-OLED
// (siehe DM3_Controller_V7.4_DualEncoder_DualDisplay/).
//
// Aufgeteilt in mehrere Tabs:
// - DM3_Controller_V7.6_DualEncoder_TripleDisplay.ino: Konfiguration, Setup, Loop
// - Network.ino: WLAN/DM3-Verbindung, Status-LED
// - Input.ino: Encoder- und Taster-Bedienung
// - Config.ino: Laden/Speichern in Preferences (Flash)
// - Battery.ino: Akku messen und anzeigen
// - Display.ino: OLED-Bildschirme
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
  "DM3-test";
char wifiPassword[WIFI_MAX_LEN + 1] =
  "Test12345";

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
// DM3
// =================================================
#define DM3_PORT 49280
WiFiClient dm3;
IPAddress dm3IP(
  10,0,41,173);

bool dm3Online = false;
unsigned long lastDM3Try = 0;
unsigned long dm3RetryTime = 3000;

// =================================================
// DM3-Netzwerk-Task (läuft dauerhaft auf Core 0)
// =================================================
// dm3.print()/dm3.print() (Senden) kann laut ESP32-Arduino-Core-Quelle
// (NetworkClient.cpp) im schlechtesten Fall bis zu 10 Sekunden blockieren
// (fest einprogrammierter select()-Retry: 1s x 10 Versuche), wenn der
// Socket-Sendepuffer mal nicht schreibbar wird (z.B. WLAN-Aussetzer vor
// Ort) — das ist NICHT über dm3.setTimeout() konfigurierbar. Lief das
// bisher in loop() direkt, fror damit die komplette Bedienung (Encoder,
// Taster) für dieselbe Zeit ein. Deshalb: die komplette DM3-Socket-Ein-/
// Ausgabe (Verbindung, Senden, Empfangen, Poll-Timing) läuft jetzt in
// einem eigenen Task auf Core 0 — analog zum bestehenden displayTask, der
// aus demselben Grund für die I2C-Displays existiert. loop() (Core 1)
// sendet ausgehende Kommandos nur noch nicht-blockierend in eine Queue.
QueueHandle_t dm3SendQueue;
#define DM3_CMD_MAX_LEN 128
#define DM3_SEND_QUEUE_LEN 32

// =================================================
// DM3 Latenz-Anzeige (Zeit des letzten abgeschlossenen Poll-Zyklus)
// =================================================
// Ein Poll-Zyklus (pollDM3()) fragt Pegel (+ alle MUTE_POLL_DIVIDER
// Zyklen zusätzlich Mute) für beide aktiven Kanäle ab. Sobald alle
// zugehörigen Antworten eingetroffen sind, gilt der Zyklus als
// "ausgeführt" — dm3LastLatencyMs zeigt bewusst nur den zuletzt
// gemessenen Wert, keinen laufenden Mittelwert mehr (war zuvor ein
// gleitender Durchschnitt seit Boot, reagierte dadurch sehr träge auf
// z.B. einen AP-Wechsel und war schwer als "ist es JETZT gut oder
// schlecht" zu lesen). Ein neuer Zyklus startet erst, wenn der vorherige
// entweder vollständig beantwortet wurde ODER POLL_TIMEOUT_MS
// überschritten ist (siehe loop-Trigger in Network.ino::dm3NetworkTask)
// — vorher wurde bei JEDEM Poll (alle 50ms) pollSentAt/
// pollResponsesExpected bedingungslos zurückgesetzt, wodurch verspätete
// Antworten der alten Runde der neuen zugerechnet wurden und die Messung
// bei Latenz >50ms unbrauchbar wurde.
unsigned long pollSentAt = 0;
int pollResponsesExpected = 0;
int pollResponsesReceived = 0;
#define POLL_TIMEOUT_MS 500
unsigned long dm3LastLatencyMs = 0;
unsigned long dm3LatencySamples = 0;

// =================================================
// Kanäle (Mute/Volume für ST Master + frei wählbare Input/Mix/Matrix-Kanäle)
// =================================================
// ST Master ist immer da (RCP-Pfad "St", Index 0, kein Slot nötig).
// Zusätzlich 7 frei konfigurierbare Slots: 3x Input, 2x Mix, 2x Matrix.
// Jeder Slot ist einzeln aktivierbar/deaktivierbar (SETTINGS -> CHANNEL ->
// INPUT/MIX/MATRIX -> SLOT n). Encoder 1 und Encoder 2 zeigen/steuern
// jeweils ihren EIGENEN, unabhängigen Kanal gleichzeitig (activeSlot1/
// activeSlot2). Langer Druck auf dem jeweiligen Encoder-Taster wandert
// der Reihe nach (mit Wraparound) durch alle AKTIVIERTEN Slots dieses
// Encoders, ohne den anderen Encoder zu beeinflussen.
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
#define INCH_SLOT_COUNT 6
#define MIX_SLOT_BASE 6
#define MIX_SLOT_COUNT 4
#define MTRX_SLOT_BASE 10
#define MTRX_SLOT_COUNT 2
#define EXTRA_SLOT_COUNT 12

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
  {CT_INCH,4,false},
  {CT_INCH,5,false},
  {CT_INCH,6,false},
  {CT_MIX,1,false},
  {CT_MIX,2,false},
  {CT_MIX,3,false},
  {CT_MIX,4,false},
  {CT_MTRX,1,false},
  {CT_MTRX,2,false}
};
// Erhöhen, wann immer sich INCH/MIX/MTRX_SLOT_BASE/_COUNT ändert: erzwingt
// beim nächsten Boot ein Zurücksetzen der gespeicherten Slot-Belegung auf
// die obigen (durchnummerierten) Standardwerte, statt alte, jetzt durch
// die neue Slot-Reihenfolge falsch zugeordnete Preferences-Werte zu
// übernehmen (z.B. ein alter Mix-Kanal, der jetzt als Input-Slot gelesen
// würde) — sonst wirken die vorbelegten Kanäle nach einer Layoutänderung
// "zufällig".
#define CHANNEL_LAYOUT_VERSION 2
// SLOT_MASTER = ST Master, 0..EXTRA_SLOT_COUNT-1 = Index in chSlots[]
int activeSlot1 = SLOT_MASTER;
int activeSlot2 = SLOT_MASTER;

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
// DISPLAYS (drei Stück)
// =================================================
// Display 1 + 2: unabhängige 128x32 SSD1306, je eigener Software-I2C-
// Bus. Beide Module haben dieselbe feste I2C-Adresse (nicht per Jumper
// änderbar) und könnten sich daher keinen gemeinsamen Bus teilen -
// deshalb bekommt jedes Display sein eigenes SDA/SCL-Paar. Zeigen immer
// (unabhängig vom Menüzustand) Kanal 1 bzw. Kanal 2.
// VCC beider 128x32-Displays liegt direkt an 3.3V (nicht über GPIO, da
// der Stromverbrauch abhängig von der Pixelzahl variiert und über einem
// sicheren GPIO-Grenzwert liegen kann), GND an GND.
#define OLED1_SDA 34
#define OLED1_SCL 38
#define OLED2_SDA 39
#define OLED2_SCL 40
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C oled1(
  U8G2_R0,
  OLED1_SCL,
  OLED1_SDA,
  U8X8_PIN_NONE);
U8G2_SSD1306_128X32_UNIVISION_F_SW_I2C oled2(
  U8G2_R0,
  OLED2_SCL,
  OLED2_SDA,
  U8X8_PIN_NONE);

// Onboard-OLED des Heltec-Boards (128x64, fest verlötet, HW-I2C) — ab
// V7.5 als dediziertes Menü-/SETTINGS-Display genutzt, statt wie in
// V7.4 ungenutzt zu bleiben. Braucht wie gehabt Vext zum Einschalten.
#define OLED_MENU_SDA 17
#define OLED_MENU_SCL 18
#define OLED_MENU_RST 21
#define Vext 36
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oledMenu(
  U8G2_R0,
  OLED_MENU_RST,
  OLED_MENU_SCL,
  OLED_MENU_SDA);

// =================================================
// Encoder 1 (Kanal 1 / Menü-Navigation)
// =================================================
// A/B auf dieser physischen Einheit vertauscht: sonst war die
// Drehrichtung umgekehrt (hoch = Pegel runter).
#define ENC1_A 4
#define ENC1_B 5
#define ENC1_SW 6
ESP32Encoder encoder1;
long lastEncoder1 = 0;
unsigned long buttonStart1 = 0;
#define LONG_PRESS_TIME 600

// =================================================
// Encoder 2 (Kanal 2) — komplett unabhängig, bedient jederzeit seinen
// eigenen Kanal, unabhängig vom aktuellen Menüzustand. Taster auf GPIO7
// statt GPIO0, da GPIO0 der BOOT-Strapping-Pin ist und jetzt dem
// separaten Menü-Taster vorbehalten bleibt.
// A/B auf dieser physischen Einheit ebenfalls vertauscht (gleicher Grund).
// =================================================
#define ENC2_A 15
#define ENC2_B 16
#define ENC2_SW 7
ESP32Encoder encoder2;
long lastEncoder2 = 0;
unsigned long buttonStart2 = 0;

// =================================================
// Menü-Taster — eigener, dedizierter Knopf nur zum Öffnen von SETTINGS
// (ersetzt den bisherigen Weg über den langen Druck auf Encoder 1)
// =================================================
#define MENU_BTN 0

// =================================================
// DM3 Werte (pro Kanal/Encoder getrennt)
// =================================================
int level1 = -99999;
int level2 = -99999;
bool mute1 = false;
bool mute2 = false;
// Sperrfenster nach eigenem Mute-Toggle: verhindert, dass eine noch
// unterwegs befindliche (veraltete) Poll-Antwort unseren gerade
// gesetzten Zustand kurz wieder zurückdreht (sichtbares MUTE-Geflacker).
unsigned long muteOverrideUntil1 = 0;
unsigned long muteOverrideUntil2 = 0;
#define MUTE_OVERRIDE_TIME 800
// Gleiches Prinzip für den Fader-Pegel: nach einer eigenen Encoder-Änderung
// kurz keine Poll-Antworten übernehmen, sonst überschreibt eine noch
// veraltete Antwort den gerade gesetzten Wert wieder (Zahl springt hoch
// und dann wieder zurück).
unsigned long levelOverrideUntil1 = 0;
unsigned long levelOverrideUntil2 = 0;
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
// Reihenfolge: CHANNEL, STEP SIZE, SLEEP TIME, IP ADDRESS, WIFI, WEB CONFIG, BACK
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
void checkWifiRoaming();
void surveyBestAP();
String bssidToStr(uint8_t *bssid);
void sendDM3(String cmd);
void pollDM3();
void readDM3();
void parseDM3(String msg);
void handleEncoder1();
void handleButton1();
void handleEncoder2();
void handleButton2();
void handleMenuButton();
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
String channelPath(int slot);
int channelIdx(int slot);
void resetChannelState(int which);
int nextEnabledSlot(int from);
int nextEnabledSlotWrap(int from);
int nextEnabledSlotWrapExcl(int from, int exclude);
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
void drawBattery(U8G2 &d, int x, int y, int percent);
void drawBatteryEmpty(U8G2 &d, int x, int y);
void drawBatteryIndicator(U8G2 &d, int x, int y);
void drawScrollingLabel(U8G2 &d, int x0, int x1, int y, String text, int &offset, unsigned long &lastMove);
void drawWebIcon(U8G2 &d, int x, int y);
void drawChannel1Screen();
void drawChannel2Screen();
void drawMenuScreen();
void displayTask(void *pvParameters);
void dm3NetworkTask(void *pvParameters);

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
  // Software-I2C läuft sonst mit sehr konservativem Standardtiming und
  // blockiert loop() für hunderte ms pro Frame (gemessen: ~590ms für
  // alle drei Displays zusammen) — das reicht, um kurze Tastendrücke
  // komplett zu verschlucken. Bus-Takt für beide SW-I2C-Displays hochsetzen.
  oled1.setBusClock(
    400000);
  oled1.begin();
  oled1.setFont(
    u8g2_font_6x12_tf);
  oled2.setBusClock(
    400000);
  oled2.begin();
  oled2.setFont(
    u8g2_font_6x12_tf);
  oledMenu.begin();
  oledMenu.setFont(
    u8g2_font_6x12_tf);
  pinMode(
    ENC1_SW,
    INPUT_PULLUP);
  pinMode(
    ENC2_SW,
    INPUT_PULLUP);
  pinMode(
    MENU_BTN,
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
  encoder1.attachHalfQuad(
    ENC1_A,
    ENC1_B);
  encoder1.clearCount();
  lastEncoder1 =
    encoder1.getCount();
  encoder2.attachHalfQuad(
    ENC2_A,
    ENC2_B);
  encoder2.clearCount();
  lastEncoder2 =
    encoder2.getCount();
  loadConfig();
  // Kanal 2 startet nicht auch auf ST Master (sonst zeigen beide Zeilen
  // anfangs dasselbe), sondern gleich auf dem ersten aktivierten Kanal,
  // falls einer konfiguriert ist. Kanal 1 bleibt auf ST Master.
  int firstOther =
    nextEnabledSlot(
      SLOT_MASTER);
  activeSlot2 =
    (firstOther != SLOT_NONE) ? firstOther : SLOT_MASTER;
  initApSSID();
  setupWebServerRoutes();
  connectWiFi();
  delay(500);
  dm3SendQueue =
    xQueueCreate(
      DM3_SEND_QUEUE_LEN,DM3_CMD_MAX_LEN);
  bootTime = millis();
  Serial.println(
    "[t=" + String(bootTime) + "] BOOTTIME SET");
  lastActivity = millis();
  readBattery();
  lastBatteryRead = millis();
  // Displays laufen als eigener Task auf dem zweiten Kern, damit die
  // langsamen Software-I2C-Displays (~280ms pro Frame, gemessen) nicht
  // loop() blockieren und dadurch kurze Tastendrücke verschlucken.
  // Alle Display-Objekt-Zugriffe (inkl. setContrast) bleiben deshalb in
  // diesem einen Task gebündelt statt über mehrere Tasks verstreut, um
  // gleichzeitige I2C-Bus-Zugriffe von zwei Kernen zu vermeiden.
  xTaskCreatePinnedToCore(
    displayTask,"Display",8192,NULL,1,NULL,0);
  // Siehe Kommentar bei dm3SendQueue weiter oben: komplette DM3-Socket-
  // I/O läuft hier, damit ein blockierendes dm3.print()/dm3.read() nie
  // die Bedienung auf Core 1 (loop()) einfrieren lässt.
  xTaskCreatePinnedToCore(
    dm3NetworkTask,"DM3Net",4096,NULL,1,NULL,0);
}

// =================================================
// DISPLAY-TASK (läuft dauerhaft auf Core 0)
// =================================================
void displayTask(
  void *pvParameters) {
  MENU lastMenuForContrast =
    MENU_MASTER;
  for (
    ;;) {
    if (
      menu == MENU_SLEEP && lastMenuForContrast != MENU_SLEEP) {
      oled1.setContrast(
        SLEEP_CONTRAST);
      oled2.setContrast(
        SLEEP_CONTRAST);
      oledMenu.setContrast(
        SLEEP_CONTRAST);
    } else if (
      menu != MENU_SLEEP && lastMenuForContrast == MENU_SLEEP) {
      oled1.setContrast(
        NORMAL_CONTRAST);
      oled2.setContrast(
        NORMAL_CONTRAST);
      oledMenu.setContrast(
        NORMAL_CONTRAST);
    }
    lastMenuForContrast = menu;

    drawChannel1Screen();
    drawChannel2Screen();
    drawMenuScreen();

    vTaskDelay(
      pdMS_TO_TICKS(50));
  }
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
  // könnte (beobachtet: AP meldet "ok=1", sendet aber trotzdem keine
  // sichtbaren Beacons mehr, wenn beides im selben Tick passiert).
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
  // DM3-Verbindung, Senden/Empfangen und Poll-Timing laufen komplett in
  // dm3NetworkTask (Core 0) — siehe Kommentar bei dm3SendQueue weiter
  // oben. loop() (Core 1) fasst den dm3-Socket nicht mehr direkt an.
  updateWebServer();
  // Status LED
  if (
    ledOn && millis() >= ledOffAt) {
    digitalWrite(
      WHITE_LED,LOW);
    ledOn = false;
  }
  // Akku
  if (
    millis() - lastBatteryRead >= BATTERY_READ_INTERVAL) {
    readBattery();
    lastBatteryRead = millis();
  }
  // Bedienung
  handleEncoder1();
  handleButton1();
  handleEncoder2();
  handleButton2();
  handleMenuButton();

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
    // Kontrast wird vom Display-Task selbst gesetzt (erkennt den
    // Menüwechsel), nicht hier — alle Display-Zugriffe bleiben in
    // diesem einen Task, um gleichzeitige I2C-Zugriffe von zwei Kernen
    // zu vermeiden.
    Serial.println(
      "SLEEP");
  }
}
