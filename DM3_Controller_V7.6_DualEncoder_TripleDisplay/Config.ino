// =================================================
// CONFIG LADEN
// =================================================
void loadConfig() {
  prefs.begin(
    "DM3",true);
  stepIndex =
    prefs.getInt(
      "step",4);
  stepIndex =
    constrain(
      stepIndex,0,STEP_COUNT - 1);
  encoderStep =
    stepValues[stepIndex];
  ipOctets[0] =
    prefs.getUChar(
      "ip0",dm3IP[0]);
  ipOctets[1] =
    prefs.getUChar(
      "ip1",dm3IP[1]);
  ipOctets[2] =
    prefs.getUChar(
      "ip2",dm3IP[2]);
  ipOctets[3] =
    prefs.getUChar(
      "ip3",dm3IP[3]);
  dm3IP =
    IPAddress(
      ipOctets[0],
      ipOctets[1],
      ipOctets[2],
      ipOctets[3]);
  sleepTimeoutMs =
    (unsigned long)prefs.getInt(
      "sleepTO",60) * 1000UL;
  sleepTimeoutMs =
    constrain(
      sleepTimeoutMs,SLEEP_TIMEOUT_MIN,SLEEP_TIMEOUT_MAX);
  prefs.getString(
    "wifiSSID",wifiSSID,WIFI_MAX_LEN + 1);
  prefs.getString(
    "wifiPass",wifiPassword,WIFI_MAX_LEN + 1);
  prefs.getString(
    "apPass",apPassword,WIFI_MAX_LEN + 1);
  webConfigEnabled =
    prefs.getBool(
      "webCfgOn",false);

  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    String base =
      "wp" + String(i);
    prefs.getString(
      (base + "n").c_str(),wifiProfiles[i].name,WIFI_NAME_MAX_LEN + 1);
    prefs.getString(
      (base + "s").c_str(),wifiProfiles[i].ssid,WIFI_MAX_LEN + 1);
    prefs.getString(
      (base + "p").c_str(),wifiProfiles[i].pass,WIFI_MAX_LEN + 1);
    wifiProfiles[i].used =
      prefs.getBool(
        (base + "u").c_str(),false);
  }

  int storedLayoutVersion =
    prefs.getInt(
      "chLayoutVer",0);
  bool layoutChanged =
    (storedLayoutVersion != CHANNEL_LAYOUT_VERSION);
  if (
    !layoutChanged) {
    for (
      int i = 0; i < EXTRA_SLOT_COUNT; i++) {
      String numKey =
        "s" + String(i) + "n";
      String onKey =
        "s" + String(i) + "o";
      chSlots[i].num =
        prefs.getInt(
          numKey.c_str(),chSlots[i].num);
      chSlots[i].enabled =
        prefs.getBool(
          onKey.c_str(),chSlots[i].enabled);
    }
  }
  prefs.end();

  if (
    layoutChanged) {
    // Alte Slot-Zuordnung passt nicht mehr zur neuen Reihenfolge -
    // stattdessen bei den durchnummerierten Standardwerten bleiben
    // (chSlots[]-Initializer) und die neue Version + diese Defaults
    // gleich als aktuellen Stand speichern.
    Serial.println(
      "CHANNEL LAYOUT CHANGED, RESETTING SLOT CONFIG TO DEFAULTS");
    saveChannelConfig();
    prefs.begin(
      "DM3",false);
    prefs.putInt(
      "chLayoutVer",CHANNEL_LAYOUT_VERSION);
    prefs.end();
  }
}

// =================================================
// CONFIG SPEICHERN
// =================================================
void saveConfig() {
  prefs.begin(
    "DM3",false);
  prefs.putInt(
    "step",stepIndex);
  prefs.end();
  Serial.println(
    "CONFIG SAVED");
}

// =================================================
// IP BEARBEITUNG STARTEN
// =================================================
void loadIPEdit() {
  ipOctets[0] = dm3IP[0];
  ipOctets[1] = dm3IP[1];
  ipOctets[2] = dm3IP[2];
  ipOctets[3] = dm3IP[3];
  ipEditIndex = 0;
}

// =================================================
// IP SPEICHERN
// =================================================
void saveIPConfig() {
  dm3IP =
    IPAddress(
      ipOctets[0],
      ipOctets[1],
      ipOctets[2],
      ipOctets[3]);
  prefs.begin(
    "DM3",false);
  prefs.putUChar(
    "ip0",ipOctets[0]);
  prefs.putUChar(
    "ip1",ipOctets[1]);
  prefs.putUChar(
    "ip2",ipOctets[2]);
  prefs.putUChar(
    "ip3",ipOctets[3]);
  prefs.end();

  // Neuverbindung mit neuer IP erzwingen
  dm3.stop();
  dm3Online = false;
  dm3RetryTime = 0;
  lastDM3Try = 0;
  level1 = -99999;
  level2 = -99999;
  bootTime = millis();
  Serial.println(
    "IP SAVED");
}

// =================================================
// RUHEZUSTAND-ZEIT SPEICHERN
// =================================================
void saveSleepTimeout() {
  prefs.begin(
    "DM3",false);
  prefs.putInt(
    "sleepTO",(int)(sleepTimeoutMs / 1000));
  prefs.end();
  Serial.println(
    "SLEEP TIMEOUT SAVED");
}

// =================================================
// WLAN ZEICHEN-INDEX SUCHEN
// =================================================
int wifiCharsetIndexFor(
  char c) {
  for (
    int i = 0; i < (int)WIFI_CHARSET_LEN; i++) {
    if (
      wifiCharset[i] == c)
      return i;
  }
  return 1;
}

// =================================================
// WLAN SPEICHERN (aktive Zugangsdaten, unabhängig von Profilen)
// =================================================
void saveWifiConfig() {
  prefs.begin(
    "DM3",false);
  prefs.putString(
    "wifiSSID",wifiSSID);
  prefs.putString(
    "wifiPass",wifiPassword);
  prefs.end();
  Serial.println(
    "WIFI CONFIG SAVED");

  // Neuverbindung mit neuen Zugangsdaten erzwingen (AP-Fallback-Modus
  // bleibt erhalten, falls gerade aktiv, siehe connectWiFi())
  WiFi.disconnect();
  WiFi.mode(
    apFallbackActive ? WIFI_AP_STA : WIFI_STA);
  WiFi.begin(
    wifiSSID,wifiPassword);
  Serial.println(
    "WIFI RECONNECT");
}

// =================================================
// WEB-CONFIG SPEICHERN (Server-Freigabe + AP-Passwort)
// =================================================
void saveWebConfig() {
  prefs.begin(
    "DM3",false);
  prefs.putBool(
    "webCfgOn",webConfigEnabled);
  prefs.putString(
    "apPass",apPassword);
  prefs.end();
  Serial.println(
    "WEB CONFIG SAVED");
}

// =================================================
// AP-PASSWORT BEARBEITUNG STARTEN (teilt sich den WLAN-Zeichen-Editor)
// =================================================
void loadApPassEdit() {
  wifiEditField = WEF_PASS;
  wifiEditIsNew = false;
  wifiEditBuffer[0] = '\0';
  wifiEditLength = 0;
  wifiEditCursor = 0;
  wifiCharIndex =
    wifiCharsetIndexFor(
      ' ');
}

// =================================================
// WLAN-PROFIL-FELD BEARBEITUNG STARTEN (Name/SSID/Passwort teilen sich
// denselben Zeichen-Editor)
// =================================================
// Editor startet absichtlich leer statt mit dem alten Wert vorausgefüllt
// (gleicher Grund wie früher beim einfachen SSID/PASS-Editor: sonst
// blieben Alt-Zeichen hinter der neuen Eingabe stehen).
void loadWifiProfileFieldEdit(
  int profileIndex,WifiEditField field,bool isNew) {
  wifiEditProfileIndex = profileIndex;
  wifiEditField = field;
  wifiEditIsNew = isNew;
  wifiEditBuffer[0] = '\0';
  wifiEditLength = 0;
  wifiEditCursor = 0;
  wifiCharIndex =
    wifiCharsetIndexFor(
      ' ');
}

// =================================================
// WLAN-PROFIL-FELD SPEICHERN (schreibt wifiEditBuffer ins passende Feld
// des gerade bearbeiteten Profils)
// =================================================
void saveWifiProfileField() {
  int idx =
    wifiEditProfileIndex;
  if (
    wifiEditField == WEF_NAME) {
    strncpy(
      wifiProfiles[idx].name,wifiEditBuffer,WIFI_NAME_MAX_LEN);
    wifiProfiles[idx].name[WIFI_NAME_MAX_LEN] = '\0';
  } else if (
    wifiEditField == WEF_SSID) {
    strncpy(
      wifiProfiles[idx].ssid,wifiEditBuffer,WIFI_MAX_LEN);
    wifiProfiles[idx].ssid[WIFI_MAX_LEN] = '\0';
  } else {
    strncpy(
      wifiProfiles[idx].pass,wifiEditBuffer,WIFI_MAX_LEN);
    wifiProfiles[idx].pass[WIFI_MAX_LEN] = '\0';
  }
  wifiProfiles[idx].used = true;
  saveWifiProfiles();
}

// =================================================
// WLAN-PROFILE SPEICHERN (alle 5 Slots)
// =================================================
void saveWifiProfiles() {
  prefs.begin(
    "DM3",false);
  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    String base =
      "wp" + String(i);
    prefs.putString(
      (base + "n").c_str(),wifiProfiles[i].name);
    prefs.putString(
      (base + "s").c_str(),wifiProfiles[i].ssid);
    prefs.putString(
      (base + "p").c_str(),wifiProfiles[i].pass);
    prefs.putBool(
      (base + "u").c_str(),wifiProfiles[i].used);
  }
  prefs.end();
}

// =================================================
// ANZAHL GESPEICHERTER WLAN-PROFILE
// =================================================
int wifiProfileCount() {
  int c = 0;
  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    if (
      wifiProfiles[i].used)
      c++;
  }
  return c;
}

// =================================================
// ERSTEN FREIEN PROFIL-SLOT FINDEN (für NEW)
// =================================================
int findFreeWifiProfileSlot() {
  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    if (
      !wifiProfiles[i].used)
      return i;
  }
  return -1;
}

// =================================================
// PROFIL-ARRAY-INDEX FÜR DIE N-TE BELEGTE LISTENPOSITION
// =================================================
int wifiProfileIndexAtListPosition(
  int pos) {
  int count = 0;
  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    if (
      wifiProfiles[i].used) {
      if (
        count == pos)
        return i;
      count++;
    }
  }
  return -1;
}

// =================================================
// MIT EINEM GESPEICHERTEN WLAN-PROFIL VERBINDEN
// =================================================
void connectToWifiProfile(
  int profileIndex) {
  strncpy(
    wifiSSID,wifiProfiles[profileIndex].ssid,WIFI_MAX_LEN);
  wifiSSID[WIFI_MAX_LEN] = '\0';
  strncpy(
    wifiPassword,wifiProfiles[profileIndex].pass,WIFI_MAX_LEN);
  wifiPassword[WIFI_MAX_LEN] = '\0';
  saveWifiConfig();
  Serial.println(
    "WIFI PROFILE CONNECT: " + String(wifiProfiles[profileIndex].name));
}

// =================================================
// WLAN-PROFIL LÖSCHEN
// =================================================
void deleteWifiProfile(
  int profileIndex) {
  wifiProfiles[profileIndex].used = false;
  wifiProfiles[profileIndex].name[0] = '\0';
  wifiProfiles[profileIndex].ssid[0] = '\0';
  wifiProfiles[profileIndex].pass[0] = '\0';
  saveWifiProfiles();
  Serial.println(
    "WIFI PROFILE DELETED");
}

// =================================================
// KANAL-SLOT BEARBEITUNG STARTEN
// =================================================
void loadChannelSlotEdit(
  int slot) {
  channelEditSlot = slot;
  channelEditNum = chSlots[slot].num;
}

// =================================================
// KANAL-SLOT SPEICHERN
// =================================================
void saveChannelSlotEdit() {
  chSlots[channelEditSlot].num = channelEditNum;
  saveChannelConfig();
  Serial.println(
    "CHANNEL SLOT SAVED");
}

// =================================================
// KANAL AKTIVIEREN/DEAKTIVIEREN
// =================================================
void toggleChannelEnabled() {
  chSlots[channelItemSlot].enabled =
    !chSlots[channelItemSlot].enabled;
  saveChannelConfig();
  Serial.println(
    chSlots[channelItemSlot].enabled ? "CHANNEL ENABLED" : "CHANNEL DISABLED");
}

// =================================================
// KANALKONFIGURATION SPEICHERN
// =================================================
void saveChannelConfig() {
  prefs.begin(
    "DM3",false);
  for (
    int i = 0; i < EXTRA_SLOT_COUNT; i++) {
    String numKey =
      "s" + String(i) + "n";
    String onKey =
      "s" + String(i) + "o";
    prefs.putInt(
      numKey.c_str(),chSlots[i].num);
    prefs.putBool(
      onKey.c_str(),chSlots[i].enabled);
  }
  prefs.end();
}
