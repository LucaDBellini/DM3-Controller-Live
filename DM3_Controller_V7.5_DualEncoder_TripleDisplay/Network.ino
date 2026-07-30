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
  // DM3 die komplette loop() (und damit Encoder/Taster/Display) für
  // mehrere Sekunden pro Verbindungsversuch.
  if (
    dm3.connect(
      dm3IP,
      DM3_PORT,
      500)) {
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
void sendDM3(
  String cmd) {
  if (
    !dm3.connected())
    return;
  if (
    cmd.startsWith(
      "set")) {
    Serial.println(
      "SEND: " + cmd);
  }
  dm3.print(
    cmd);
  dm3.print(
    "\n");
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
void pollDM3() {
  pollSentAt = millis();
  pollResponsesExpected = 4;
  pollResponsesReceived = 0;
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/Level " + String(channelIdx(activeSlot1)) + " 0");
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot1) + "/Fader/On " + String(channelIdx(activeSlot1)) + " 0");
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/Level " + String(channelIdx(activeSlot2)) + " 0");
  sendDM3(
    "get MIXER:Current/" + channelPath(activeSlot2) + "/Fader/On " + String(channelIdx(activeSlot2)) + " 0");
}

// =================================================
// DM3 EMPFANG
// =================================================
void readDM3() {
  while (
    dm3.available()) {
    String msg =
      dm3.readStringUntil('\n');
    msg.trim();
    if (
      msg.length() == 0)
      continue;
    parseDM3(
      msg);
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
  // POLL-LATENZ: eine der vier erwarteten Poll-Antworten ist eingetroffen
  // (unabhängig vom Override-Fenster, das nur die WERT-Übernahme betrifft,
  // nicht die reine Ankunft der Antwort)
  // =================================================
  if (
    pollResponsesExpected > 0 && (msg.startsWith(level1Prefix) || msg.startsWith(on1Prefix) || msg.startsWith(level2Prefix) || msg.startsWith(on2Prefix))) {
    pollResponsesReceived++;
    if (
      pollResponsesReceived >= pollResponsesExpected) {
      unsigned long elapsed =
        millis() - pollSentAt;
      dm3LatencySamples++;
      dm3AvgLatencyMs =
        dm3AvgLatencyMs + (elapsed - dm3AvgLatencyMs) / dm3LatencySamples;
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
