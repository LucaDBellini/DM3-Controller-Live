// =================================================
// WLAN VERBINDEN
// =================================================
void connectWiFi() {
  // lastWifiTry=0 bedeutet "noch nie versucht" und darf NICHT wie ein
  // Versuch bei t=0 behandelt werden, sonst wird der allererste Verbindungs-
  // versuch nach dem Booten fälschlich für die ersten 10s blockiert
  // (millis()-0 ist in dieser Zeit immer < 10000). Global statt static-
  // lokal, damit startApFallback() ihn direkt setzen kann (siehe dort).
  // Solange gerade jemand mit dem AP-Fallback verbunden ist (z.B. während
  // der Konfiguration über die Weboberfläche), GAR KEINE STA-Versuche
  // starten — WiFi.begin() löst sonst einen Kanal-Scan aus, der die
  // Verbindung des AP-Clients unterbricht ("rausschmeisst"). Sobald
  // niemand mehr am AP hängt, darf im Hintergrund wieder nach dem
  // eigentlichen WLAN gesucht werden.
  if (
    apFallbackActive && WiFi.softAPgetStationNum() > 0) {
    return;
  }
  // Während der AP-Fallback aktiv ist (aber gerade niemand verbunden ist),
  // trotzdem viel seltener retryen als im Normalbetrieb: WiFi.begin() ohne
  // festen Kanal löst einen kompletten Kanal-Scan aus, der die einzige
  // Funkschnittstelle des ESP32 dabei laufend vom AP-Kanal wegzieht — bei
  // einem 10s-Rhythmus ist der AP dadurch für scannende Handys/Laptops
  // kaum noch sichtbar. Mit 60s Pause bleibt der AP die meiste Zeit stabil
  // auf seinem Kanal, während automatische Rückkehr zum eigentlichen WLAN
  // trotzdem funktioniert, sobald es wieder erreichbar ist.
  unsigned long retryInterval =
    apFallbackActive ? 60000 : 10000;
  if (
    lastWifiTry != 0 && millis() - lastWifiTry < retryInterval)
    return;
  lastWifiTry = millis();
  if (
    WiFi.status() == WL_CONNECTED)
    return;
  Serial.println(
    "[t=" + String(millis()) + "] WIFI CONNECT");
  // Laufenden Verbindungsversuch sauber abbrechen, sonst bleibt er stecken
  // ("sta is connecting, cannot set config") und WiFi.begin() verbindet nie.
  // WiFi.disconnect() betrifft nur die Station-Schnittstelle, der AP-Fallback
  // (falls aktiv) bleibt unberührt — der Modus muss dann aber AP_STA bleiben,
  // sonst würde WIFI_STA den gerade laufenden AP abschalten.
  WiFi.disconnect();
  WiFi.mode(
    apFallbackActive ? WIFI_AP_STA : WIFI_STA);
  delay(
    100);
  WiFi.begin(
    wifiSSID,
    wifiPassword);
}

// =================================================
// BSSID ALS HEX-STRING (fürs Log)
// =================================================
String bssidToStr(
  uint8_t *bssid) {
  char buf[18];
  snprintf(
    buf,sizeof(buf),"%02X:%02X:%02X:%02X:%02X:%02X",
    bssid[0],bssid[1],bssid[2],bssid[3],bssid[4],bssid[5]);
  return String(
    buf);
}

// =================================================
// BESTEN 2,4GHz-AP FÜR DIE DM3-ROUTE FINDEN (einmalig nach dem Boot)
// =================================================
// RSSI (siehe checkWifiRoaming() weiter unten) sagt nur etwas über die
// Funkstrecke ESP32<->AP aus, NICHT über die tatsächliche Route/Backhaul
// vom AP zum DM3 — ein AP kann volles Signal haben und trotzdem über
// einen schlecht angebundenen Mesh-Hop zum DM3 geroutet sein (live
// beobachtet: -51dBm Signal, aber 110+ms zum DM3, während ein Laptop auf
// einem ANDEREN Band nur 5-6ms hatte). Das ESP32-S3 kann ohnehin nur
// 2,4GHz (kein 5/6GHz-Hardware) — das hier testet deshalb der Reihe nach
// jeden per SSID-Scan gefundenen 2,4GHz-AP, misst die echte TCP-Connect-
// Zeit zum DM3 als Latenz-Proxy, und bleibt danach auf dem besten. Läuft
// bewusst NUR EINMAL kurz nach dem Boot (nicht periodisch): jeder
// Kandidat kostet eine echte Trennung+Neuverbindung, macht insgesamt
// mehrere -zig Sekunden Aussetzer — unproblematisch fürs Timing selbst
// (läuft isoliert in dm3NetworkTask, loop()/Bedienung bleibt unberührt),
// aber ein wiederholter Aussetzer mitten in einer laufenden Show wäre
// unerwünscht. Läuft in dm3NetworkTask, siehe dortigen Aufruf.
#define AP_SURVEY_MAX_CANDIDATES 8
#define AP_SURVEY_WIFI_TIMEOUT 4000
#define AP_SURVEY_DM3_TIMEOUT 500
bool apSurveyDone = false;

void surveyBestAP() {
  if (
    apSurveyDone)
    return;
  if (
    WiFi.status() != WL_CONNECTED)
    return;
  apSurveyDone = true;

  int n =
    WiFi.scanNetworks();
  uint8_t candBSSID[AP_SURVEY_MAX_CANDIDATES][6];
  int32_t candChannel[AP_SURVEY_MAX_CANDIDATES];
  int candCount = 0;
  for (
    int i = 0; i < n && candCount < AP_SURVEY_MAX_CANDIDATES; i++) {
    if (
      WiFi.SSID(i) != String(wifiSSID))
      continue;
    memcpy(
      candBSSID[candCount],WiFi.BSSID(i),6);
    candChannel[candCount] =
      WiFi.channel(i);
    candCount++;
  }
  WiFi.scanDelete();

  if (
    candCount == 0) {
    Serial.println(
      "AP SURVEY: keine passenden APs gefunden, bleibe wie verbunden");
    return;
  }

  Serial.println(
    "AP SURVEY: teste " + String(candCount) + " AP(s) fuer beste DM3-Route...");
  long bestLatency = -1;
  int bestIdx = -1;
  for (
    int i = 0; i < candCount; i++) {
    WiFi.disconnect();
    dm3.stop();
    WiFi.begin(
      wifiSSID,wifiPassword,candChannel[i],candBSSID[i]);
    unsigned long connectStart =
      millis();
    while (
      WiFi.status() != WL_CONNECTED && millis() - connectStart < AP_SURVEY_WIFI_TIMEOUT) {
      delay(
        50);
    }
    if (
      WiFi.status() != WL_CONNECTED) {
      Serial.println(
        "AP SURVEY: " + bssidToStr(candBSSID[i]) + " Ch" + String(candChannel[i]) + " -> kein WLAN-Connect");
      continue;
    }
    unsigned long dm3Start =
      millis();
    bool ok =
      dm3.connect(
        dm3IP,DM3_PORT,AP_SURVEY_DM3_TIMEOUT);
    unsigned long dm3Elapsed =
      millis() - dm3Start;
    dm3.stop();
    if (
      !ok) {
      Serial.println(
        "AP SURVEY: " + bssidToStr(candBSSID[i]) + " Ch" + String(candChannel[i]) + " -> DM3 nicht erreichbar");
      continue;
    }
    Serial.println(
      "AP SURVEY: " + bssidToStr(candBSSID[i]) + " Ch" + String(candChannel[i]) + " -> DM3-Connect " + String(dm3Elapsed) + "ms");
    if (
      bestIdx == -1 || (long)dm3Elapsed < bestLatency) {
      bestLatency = dm3Elapsed;
      bestIdx = i;
    }
  }

  if (
    bestIdx >= 0) {
    Serial.println(
      "AP SURVEY: bester AP " + bssidToStr(candBSSID[bestIdx]) + " Ch" + String(candChannel[bestIdx]) + " (" + String(bestLatency) + "ms), verbinde final");
    WiFi.disconnect();
    WiFi.begin(
      wifiSSID,wifiPassword,candChannel[bestIdx],candBSSID[bestIdx]);
  } else {
    Serial.println(
      "AP SURVEY: kein Kandidat erfolgreich, verbinde normal (automatische AP-Wahl)");
    WiFi.begin(
      wifiSSID,wifiPassword);
  }
}

// =================================================
// WLAN-ROAMING: periodisch nach einem deutlich stärkeren AP mit
// derselben SSID suchen und dorthin wechseln
// =================================================
// ESP-IDF/Arduino roamt nicht von selbst: WiFi.begin() verbindet einmal
// mit dem zum Verbindungszeitpunkt stärksten AP und bleibt dann fest
// daran hängen — auch wenn später ein anderer AP derselben SSID (z.B. in
// einem anderen Raum, näher am aktuellen Standort) viel stärker wäre.
// Läuft bewusst in dm3NetworkTask (Core 0, siehe dort dessen Aufruf),
// NICHT in loop(): WiFi.scanNetworks() blockiert 1-3 Sekunden, das würde
// sonst wieder die Bedienung einfrieren lassen — genau das, was wir mit
// dm3NetworkTask gerade erst behoben haben.
#define ROAM_CHECK_INTERVAL 30000
// Ziel-AP muss um mindestens so viel stärker sein (dBm), sonst bleibt
// die Verbindung stehen — verhindert Hin-und-Her-Wechseln (Flapping)
// zwischen zwei etwa gleich starken APs.
#define ROAM_RSSI_MARGIN 8
unsigned long lastRoamCheck = 0;

void checkWifiRoaming() {
  if (
    WiFi.status() != WL_CONNECTED)
    return;
  // Scan würde den AP-Fallback-Kanal stören (siehe Kommentar in
  // connectWiFi() zum selben Thema) — während des Fallbacks nicht roamen.
  if (
    apFallbackActive)
    return;
  if (
    lastRoamCheck != 0 && millis() - lastRoamCheck < ROAM_CHECK_INTERVAL)
    return;
  lastRoamCheck = millis();

  int currentRSSI =
    WiFi.RSSI();
  uint8_t currentBSSID[6];
  memcpy(
    currentBSSID,WiFi.BSSID(),6);

  int n =
    WiFi.scanNetworks();
  int bestIdx = -1;
  int bestRSSI = currentRSSI;
  for (
    int i = 0; i < n; i++) {
    if (
      WiFi.SSID(i) != String(wifiSSID))
      continue;
    if (
      memcmp(WiFi.BSSID(i),currentBSSID,6) == 0)
      continue;
    if (
      WiFi.RSSI(i) > bestRSSI + ROAM_RSSI_MARGIN) {
      bestRSSI = WiFi.RSSI(i);
      bestIdx = i;
    }
  }
  if (
    bestIdx >= 0) {
    Serial.println(
      "ROAM: wechsle zu staerkerem AP (" + String(bestRSSI) + "dBm, war " + String(currentRSSI) + "dBm)");
    WiFi.begin(
      wifiSSID,wifiPassword,WiFi.channel(bestIdx),WiFi.BSSID(bestIdx));
  } else {
    Serial.println(
      "ROAM CHECK: aktueller AP " + String(currentRSSI) + "dBm, " + String(n) + " Netz(e) gescannt, kein staerkerer gefunden");
  }
  WiFi.scanDelete();
}

// =================================================
// DM3 VERBINDEN
// =================================================
void connectDM3() {
  if (
    WiFi.status() != WL_CONNECTED)
    return;
  if (
    millis() - lastDM3Try < dm3RetryTime)
    return;
  lastDM3Try = millis();
  if (
    dm3.connected())
    return;
  Serial.println(
    "[t=" + String(millis()) + "] DM3 CONNECT");
  dm3.stop();
  ledOnFor(
    LED_BLINK_TIME);
  // Kurzer Timeout, sonst blockiert dm3.connect() bei nicht erreichbarem
  // DM3 die komplette loop() (und damit Encoder/Taster/Display) für die
  // volle Dauer jedes einzelnen Verbindungsversuchs — bei komplett
  // unerreichbarem DM3 (keine ARP-Antwort) wird IMMER das volle Timeout
  // ausgeschöpft, da lwIP dort nichts zum Abkürzen hat. 100ms reicht für
  // ein echtes, im selben LAN erreichbares DM3 (typisch <5ms Connect-
  // Zeit) locker, hält den Menü-Stall im Offline-Fall aber unter der
  // menschlichen Wahrnehmungsschwelle statt spürbar hängenzubleiben.
  if (
    dm3.connect(
      dm3IP,
      DM3_PORT,
      100)) {
    dm3Online = true;
    dm3RetryTime = 3000;
    ledOnFor(
      LED_CONNECTED_TIME);
    Serial.println(
      "[t=" + String(millis()) + "] DM3 ONLINE");
    requestChannelNames();
  } else {
    dm3Online = false;
    dm3RetryTime = 10000;
    Serial.println(
      "DM3 OFFLINE RETRY 10s");
  }
}

// =================================================
// STATUS LED
// =================================================
void ledOnFor(
  unsigned long ms) {
  digitalWrite(
    WHITE_LED,HIGH);
  ledOn = true;
  ledOffAt =
    millis() + ms;
}

// =================================================
// DM3 SENDEN
// =================================================
// Blockiert NICHT: legt das Kommando nur in dm3SendQueue ab, das
// eigentliche (potenziell mehrere Sekunden blockierende) dm3.print()
// passiert ausschließlich in dm3NetworkTask (siehe dort). Aufrufbar aus
// loop() (Encoder/Taster, Core 1) genau wie aus dm3NetworkTask selbst
// (Polling, Core 0) — xQueueSend ist von beiden Cores aus sicher nutzbar.
// Wartezeit 0: ist die Queue mal voll (sollte praktisch nie vorkommen),
// wird das Kommando verworfen statt den Aufrufer blockieren zu lassen.
void sendDM3(
  String cmd) {
  if (
    cmd.startsWith(
      "set")) {
    Serial.println(
      "SEND: " + cmd);
  }
  char buf[DM3_CMD_MAX_LEN];
  snprintf(
    buf,sizeof(buf),"%s",cmd.c_str());
  xQueueSend(
    dm3SendQueue,buf,0);
}

// =================================================
// KANAL: RCP-PFAD/INDEX (für den übergebenen Slot, SLOT_MASTER oder
// Index in chSlots[])
// =================================================
String channelPath(
  int slot) {
  if (
    slot == SLOT_MASTER)
    return "St";
  return String(
    chTypeRcp[chSlots[slot].type]);
}

int channelIdx(
  int slot) {
  if (
    slot == SLOT_MASTER)
    return 0;
  return chSlots[slot].num - 1;
}

// =================================================
// KANALWECHSEL: STATUS ZURÜCKSETZEN (which = 1 oder 2)
// =================================================
void resetChannelState(
  int which) {
  if (
    which == 1) {
    level1 = -99999;
    mute1 = false;
    levelOverrideUntil1 = 0;
    muteOverrideUntil1 = 0;
  } else {
    level2 = -99999;
    mute2 = false;
    levelOverrideUntil2 = 0;
    muteOverrideUntil2 = 0;
  }
  bootTime = millis();
}

// =================================================
// NÄCHSTEN AKTIVIERTEN SLOT SUCHEN (für langen Druck auf Encoder-Taster)
// =================================================
// Durchsucht chSlots[] in fester Reihenfolge (erst Input, dann Mix, dann
// Matrix) ab dem Slot NACH "from". from=SLOT_MASTER durchsucht ab Index 0.
int nextEnabledSlot(
  int from) {
  for (
    int s = from + 1; s < EXTRA_SLOT_COUNT; s++) {
    if (
      chSlots[s].enabled)
      return s;
  }
  return SLOT_NONE;
}

// =================================================
// WIE OBEN, ABER MIT WRAPAROUND ÜBER ST MASTER
// =================================================
// Zyklus: ST MASTER -> erster aktivierter Slot -> ... -> letzter
// aktivierter Slot -> zurück zu ST MASTER -> ... Landet die Suche am
// Ende der Liste (SLOT_NONE), geht es zurück zu ST MASTER statt beim
// letzten Slot hängen zu bleiben — sonst wäre ST Master nach dem ersten
// Weiterschalten nie wieder erreichbar.
int nextEnabledSlotWrap(
  int from) {
  int next =
    nextEnabledSlot(
      from);
  if (
    next == SLOT_NONE) {
    return SLOT_MASTER;
  }
  return next;
}

// =================================================
// WIE OBEN, ABER ÜBERSPRINGT EINEN AUSGESCHLOSSENEN SLOT
// =================================================
// Damit Encoder 1 und Encoder 2 nie denselben Kanal (auch nicht beide
// ST Master) gleichzeitig anzeigen: "exclude" ist der aktuell vom
// JEWEILS ANDEREN Encoder belegte Slot. Findet sich nach einer vollen
// Runde durch alle möglichen Werte (ST Master + alle Slots) nichts
// anderes als "exclude", bleibt "from" unverändert (nichts zum
// Wechseln verfügbar).
int nextEnabledSlotWrapExcl(
  int from,int exclude) {
  int next =
    from;
  for (
    int i = 0; i <= EXTRA_SLOT_COUNT + 1; i++) {
    next =
      nextEnabledSlotWrap(
        next);
    if (
      next != exclude) {
      return next;
    }
  }
  return from;
}

// =================================================
// SLOT-BEREICH FÜR EINEN TYP
// =================================================
int typeSlotBase(
  int type) {
  if (
    type == CT_INCH)
    return INCH_SLOT_BASE;
  if (
    type == CT_MIX)
    return MIX_SLOT_BASE;
  return MTRX_SLOT_BASE;
}

int typeSlotCount(
  int type) {
  if (
    type == CT_INCH)
    return INCH_SLOT_COUNT;
  if (
    type == CT_MIX)
    return MIX_SLOT_COUNT;
  return MTRX_SLOT_COUNT;
}

// =================================================
// KANALNAMEN ANFRAGEN (ST MASTER, INPUT, MIX, MATRIX)
// =================================================
void requestChannelNames() {
  sendDM3(
    "get MIXER:Current/St/Label/Name 0 0");
  for (
    int i = 0; i < INPUT_CHANNEL_TOTAL; i++) {
    sendDM3(
      "get MIXER:Current/InCh/Label/Name " + String(i) + " 0");
  }
  for (
    int i = 0; i < MIX_CHANNEL_TOTAL; i++) {
    sendDM3(
      "get MIXER:Current/Mix/Label/Name " + String(i) + " 0");
  }
  for (
    int i = 0; i < MTRX_CHANNEL_TOTAL; i++) {
    sendDM3(
      "get MIXER:Current/Mtrx/Label/Name " + String(i) + " 0");
  }
}

// =================================================
// INPUT-KANAL ANZEIGENAME
// =================================================
String inputChannelLabel(
  int num) {
  if (
    num >= 1 && num <= INPUT_CHANNEL_TOTAL && strlen(inputChannelNames[num - 1]) > 0) {
    return String(
      inputChannelNames[num - 1]);
  }
  return "CH " + String(num);
}

// =================================================
// SLOT-ANZEIGENAME (TYPUNABHÄNGIG)
// =================================================
String channelSlotLabel(
  int slotIdx) {
  int type =
    chSlots[slotIdx].type;
  int num =
    chSlots[slotIdx].num;
  if (
    type == CT_INCH) {
    return inputChannelLabel(
      num);
  }
  if (
    type == CT_MIX && strlen(mixChannelNames[num - 1]) > 0) {
    return String(
      mixChannelNames[num - 1]);
  }
  if (
    type == CT_MTRX && strlen(matrixChannelNames[num - 1]) > 0) {
    return String(
      matrixChannelNames[num - 1]);
  }
  return String(
           chTypeLabel[type])
    + " " + String(num);
}

// =================================================
// ST MASTER ANZEIGENAME
// =================================================
String masterLabel() {
  if (
    strlen(stMasterName) > 0) {
    return String(
      stMasterName);
  }
  return "ST MASTER";
}

// =================================================
// DM3 POLLING (beide Kanäle unabhängig)
// =================================================
// Aufrufer (dm3NetworkTask) startet einen neuen Zyklus nur, wenn der
// vorherige entweder fertig ist (pollResponsesExpected==0) oder seit
// POLL_TIMEOUT_MS keine Antwort mehr kam — siehe dort. Dadurch werden
// pollSentAt/pollResponsesExpected nicht mehr bei jedem 50ms-Tick
// bedingungslos zurückgesetzt, was zuvor verspätete Antworten der alten
// Runde der neuen zurechnete und die Latenzmessung bei >50ms Latenz
// verfälschte.
//
// Fader/On (Mute) wird bewusst nicht in jedem Zyklus mit abgefragt: live
// gemessen beantwortet das DM3 die Kommandos eines Zyklus offenbar
// seriell mit spürbarem Abstand pro Kommando (Einzel-Roundtrip ~10-20ms,
// 4 Kommandos zusammen ~100-120ms) — das ist DM3-seitige Verarbeitung,
// keine Netzwerklatenz (per AP-Wechsel bestätigt unveränderbar). Mute
// ändert sich in der Praxis viel seltener als der Pegel, deshalb reicht
// es, Fader/On nur bei jedem MUTE_POLL_DIVIDER-ten Zyklus mitzufragen
// (alle ~200ms statt alle 50ms) — reduziert die meisten Zyklen von 4 auf
// 2 Kommandos und damit auch die gemessene/angezeigte Latenz spürbar.
#define MUTE_POLL_DIVIDER 4
unsigned long pollCycleCount = 0;

void pollDM3() {
  pollSentAt = millis();
  bool includeMute =
    (pollCycleCount % MUTE_POLL_DIVIDER == 0);
  pollCycleCount++;
  pollResponsesExpected =
    includeMute ? 4 : 2;
  pollResponsesReceived = 0;
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/Level " + String(channelIdx(activeSlot1)) + " 0");
  if (
    includeMute) {
    sendDM3(
      "get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/On " + String(channelIdx(activeSlot1)) + " 0");
  }
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/Level " + String(channelIdx(activeSlot2)) + " 0");
  if (
    includeMute) {
    sendDM3(
      "get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/On " + String(channelIdx(activeSlot2)) + " 0");
  }
}

// =================================================
// DM3 EMPFANG
// =================================================
// Bewusst NICHT readStringUntil('\n'): das pollt bei einer noch
// unvollständig angekommenen Zeile (TCP-Fragmentierung, z.B. bei
// WLAN-Störungen vor Ort) bis zu Stream::setTimeout() (Arduino-Default
// 1000ms) und blockiert damit dm3NetworkTask unnötig lange. Stattdessen
// byteweise in einen statischen Puffer sammeln — available()/read() sind
// beim ESP32-Core immer nicht-blockierend (interner RX-Puffer arbeitet
// durchgehend mit MSG_DONTWAIT), diese Funktion kann also nie hängen.
void readDM3() {
  static char rxBuf[300];
  static size_t rxLen = 0;
  while (
    dm3.available()) {
    char c =
      dm3.read();
    if (
      c == '\n') {
      rxBuf[rxLen] = '\0';
      String msg =
        String(rxBuf);
      msg.trim();
      if (
        msg.length() > 0) {
        parseDM3(
          msg);
      }
      rxLen = 0;
    } else if (
      c != '\r' && rxLen < sizeof(rxBuf) - 1) {
      rxBuf[rxLen++] = c;
    }
  }
}

// =================================================
// DM3 PARSER
// =================================================
void parseDM3(
  String msg) {

  // Antworten werden gegen PFAD+INDEX von Kanal 1 UND Kanal 2 geprüft
  // (Index jetzt Teil des Prefix, sonst wären zwei Kanäle vom selben Typ
  // z.B. zwei InCh-Kanäle nicht mehr unterscheidbar). Antworten, die zu
  // keinem der beiden aktuell aktiven Kanäle passen, werden ignoriert.
  String level1Prefix =
    "OK get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/Level " + String(channelIdx(activeSlot1)) + " 0 ";
  String on1Prefix =
    "OK get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/On " + String(channelIdx(activeSlot1)) + " 0 ";
  String level2Prefix =
    "OK get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/Level " + String(channelIdx(activeSlot2)) + " 0 ";
  String on2Prefix =
    "OK get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/On " + String(channelIdx(activeSlot2)) + " 0 ";

  // =================================================
  // POLL-LATENZ: eine der erwarteten Poll-Antworten ist eingetroffen
  // (unabhängig vom Override-Fenster, das nur die WERT-Übernahme betrifft,
  // nicht die reine Ankunft der Antwort)
  // =================================================
  if (
    pollResponsesExpected > 0 && (msg.startsWith(level1Prefix) || msg.startsWith(on1Prefix) || msg.startsWith(level2Prefix) || msg.startsWith(on2Prefix))) {
    pollResponsesReceived++;
    if (
      pollResponsesReceived >= pollResponsesExpected) {
      dm3LastLatencyMs =
        millis() - pollSentAt;
      dm3LatencySamples++;
      pollResponsesExpected = 0;
    }
  }

  // =================================================
  // Fader Level
  // =================================================
  if (
    msg.startsWith(
      level1Prefix) && millis() >= levelOverrideUntil1) {
    int pos =
      msg.lastIndexOf(' ');
    if (
      pos > 0) {
      level1 =
        msg.substring(
             pos + 1)
          .toInt();
    }
  }
  if (
    msg.startsWith(
      level2Prefix) && millis() >= levelOverrideUntil2) {
    int pos =
      msg.lastIndexOf(' ');
    if (
      pos > 0) {
      level2 =
        msg.substring(
             pos + 1)
          .toInt();
    }
  }

  // =================================================
  // Fader Mute
  // =================================================
  if (
    msg.startsWith(
      on1Prefix) && millis() >= muteOverrideUntil1) {
    int pos =
      msg.lastIndexOf(' ');
    if (
      pos > 0) {
      int value =
        msg.substring(
             pos + 1)
          .toInt();
      mute1 =
        (value == 0);
    }
  }
  if (
    msg.startsWith(
      on2Prefix) && millis() >= muteOverrideUntil2) {
    int pos =
      msg.lastIndexOf(' ');
    if (
      pos > 0) {
      int value =
        msg.substring(
             pos + 1)
          .toInt();
      mute2 =
        (value == 0);
    }
  }

  // =================================================
  // ST Master Name
  // =================================================
  if (
    msg.startsWith(
      "OK get MIXER:Current/St/Label/Name")) {
    String namePart =
      extractLabelName(
        msg,"St/Label/Name");
    namePart.toCharArray(
      stMasterName,9);
  }

  // =================================================
  // Input-Kanalname
  // =================================================
  parseChannelName(
    msg,"InCh/Label/Name",inputChannelNames,INPUT_CHANNEL_TOTAL);

  // =================================================
  // Mix-Kanalname
  // =================================================
  parseChannelName(
    msg,"Mix/Label/Name",mixChannelNames,MIX_CHANNEL_TOTAL);

  // =================================================
  // Matrix-Kanalname
  // =================================================
  parseChannelName(
    msg,"Mtrx/Label/Name",matrixChannelNames,MTRX_CHANNEL_TOTAL);

  // =================================================
  // UNBEKANNTE/FEHLER-ANTWORTEN (Diagnose)
  // =================================================
  if (
    !msg.startsWith(
      "OK get MIXER:Current/") && !msg.startsWith(
      "OK set MIXER:Current/")) {
    Serial.println(
      "DM3 MSG: " + msg);
  }
}

// =================================================
// LABEL/NAME AUS EINER "OK get ..."-ANTWORT EXTRAHIEREN
// =================================================
// Erwartet z.B. "OK get MIXER:Current/InCh/Label/Name 3 0 "Vocal 1""
// und liefert bei <marker>="InCh/Label/Name" den Index (per out-Parameter)
// und den (von Anführungszeichen befreiten) Namen zurück.
String extractLabelName(
  String msg,String marker) {
  String rest =
    msg.substring(
      msg.indexOf(marker) + marker.length());
  rest.trim();
  int sp1 =
    rest.indexOf(' ');
  if (
    sp1 <= 0)
    return "";
  String remainder =
    rest.substring(
      sp1 + 1);
  remainder.trim();
  int sp2 =
    remainder.indexOf(' ');
  String namePart =
    (sp2 >= 0) ? remainder.substring(sp2 + 1) : remainder;
  namePart.trim();
  if (
    namePart.startsWith("\"") && namePart.endsWith("\"") && namePart.length() >= 2) {
    namePart =
      namePart.substring(
        1,namePart.length() - 1);
  }
  return namePart;
}

// =================================================
// KANALNAME PARSEN (INPUT/MIX/MATRIX, JE EIN ARRAY-EINTRAG PRO INDEX)
// =================================================
void parseChannelName(
  String msg,String marker,char arr[][9],int maxIdx) {
  String fullMarker =
    "OK get MIXER:Current/" + marker;
  if (
    !msg.startsWith(fullMarker))
    return;
  String rest =
    msg.substring(
      msg.indexOf(marker) + marker.length());
  rest.trim();
  int sp1 =
    rest.indexOf(' ');
  if (
    sp1 <= 0)
    return;
  int idx =
    rest.substring(
         0,sp1)
      .toInt();
  String namePart =
    extractLabelName(
      msg,marker);
  if (
    idx >= 0 && idx < maxIdx) {
    namePart.toCharArray(
      arr[idx],9);
  }
}

// =================================================
// DM3-NETZWERK-TASK (läuft dauerhaft auf Core 0)
// =================================================
// Besitzt den kompletten dm3-Socket exklusiv: Verbindungsaufbau, Senden
// (aus dm3SendQueue), Empfangen, Poll-Timing. loop() (Core 1) fasst dm3.*
// nicht mehr direkt an, außer lesend über dm3.connected() fürs Display
// (genau wie displayTask das für level1/mute1/etc. bereits tut — siehe
// Kommentar bei dm3SendQueue in der .ino-Hauptdatei).
void dm3NetworkTask(
  void *pvParameters) {
  for (
    ;;) {
    surveyBestAP();
    if (
      WiFi.status() == WL_CONNECTED && !dm3.connected()) {
      connectDM3();
    }
    checkWifiRoaming();

    // Ausgehende Kommandos aus der Queue senden (hier darf dm3.print()
    // im Zweifel blockieren — betrifft nur diesen Task, nicht loop()).
    char cmd[DM3_CMD_MAX_LEN];
    while (
      xQueueReceive(
        dm3SendQueue,cmd,0) == pdTRUE) {
      if (
        dm3.connected()) {
        dm3.print(
          cmd);
        dm3.print(
          "\n");
      }
    }

    readDM3();

    // Poll-Trigger: neuer Zyklus nur, wenn der vorherige fertig ist
    // (pollResponsesExpected==0) oder seit POLL_TIMEOUT_MS keine
    // Antwort mehr kam (dann gilt er als verloren, siehe Kommentar bei
    // pollDM3()).
    if (
      dm3.connected() && millis() - lastPoll >= POLL_TIME) {
      if (
        pollResponsesExpected == 0 || millis() - pollSentAt > POLL_TIMEOUT_MS) {
        pollDM3();
      }
      lastPoll = millis();
    }

    vTaskDelay(
      pdMS_TO_TICKS(5));
  }
}
