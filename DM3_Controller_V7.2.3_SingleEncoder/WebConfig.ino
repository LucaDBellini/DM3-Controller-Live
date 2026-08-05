// =================================================
// AP-SSID AUS CHIP-MAC BILDEN (einmalig in setup())
// =================================================
// Damit nicht mehrere Controller im selben Raum denselben AP-Namen
// ausstrahlen: Suffix aus den unteren 16 Bit der Chip-eigenen MAC.
void initApSSID() {
  uint64_t chipId =
    ESP.getEfuseMac();
  snprintf(
    apSSID,sizeof(apSSID),"DM3-Setup-%04X",
    (unsigned int)(chipId & 0xFFFF));
}

// =================================================
// AP-FALLBACK STARTEN/STOPPEN
// =================================================
void startApFallback() {
  // Laufenden STA-Scan/Verbindungsversuch zuerst sauber abbrechen — startet
  // WiFi.softAP() während der Radio noch mit einem STA-Scan beschäftigt
  // ist, kann der AP-Start fehlschlagen oder verzögert wirksam werden.
  WiFi.disconnect();
  WiFi.mode(
    WIFI_AP_STA);
  delay(
    100);
  // Fester, manuell gesetzter Regulatory-Domain-Code statt der ESP-IDF-
  // Standardlogik: die übernimmt sonst automatisch das von zuvor
  // verbundenen Routern per 802.11d ausgestrahlte Länder-Kennzeichen und
  // speichert es dauerhaft im NVS — das kann die für den eigenen AP
  // erlaubten Kanäle/Sendeleistung unbemerkt einschränken. CH/ETSI erlaubt
  // Kanäle 1-13, was für Kanal 1 (unser AP-Standardkanal) sicher ausreicht.
  wifi_country_t country = {
    "CH",1,13,20,WIFI_COUNTRY_POLICY_MANUAL
  };
  esp_wifi_set_country(
    &country);
  bool ok =
    WiFi.softAP(
      apSSID,apPassword);
  apFallbackActive = true;
  // Verhindert, dass connectWiFi() im selben oder nächsten Tick sofort
  // noch einen kollidierenden STA-Scan hinterherschickt (siehe Kommentar
  // in loop()) — der AP bekommt so garantiert mindestens die volle
  // Retry-Pause Zeit, um sein Beacon sauber hochzufahren.
  lastWifiTry = millis();
  Serial.println(
    "AP FALLBACK STARTED: " + String(apSSID) + " IP " + WiFi.softAPIP().toString() + " ok=" + String(ok) + " ch=" + String(WiFi.channel()));
}

void stopApFallback() {
  WiFi.softAPdisconnect(
    true);
  WiFi.mode(
    WIFI_STA);
  apFallbackActive = false;
  Serial.println(
    "AP FALLBACK STOPPED");
}

// =================================================
// WEBSERVER START/STOP JE NACH BEDARF (manuell aktiviert ODER AP-Fallback)
// =================================================
void updateWebServer() {
  bool shouldRun =
    webConfigEnabled || apFallbackActive;
  if (
    shouldRun && !webServerRunning) {
    server.begin();
    webServerRunning = true;
    Serial.println(
      "WEB SERVER STARTED");
  } else if (
    !shouldRun && webServerRunning) {
    server.stop();
    webServerRunning = false;
    Serial.println(
      "WEB SERVER STOPPED");
  }
  if (
    webServerRunning) {
    server.handleClient();
  }
}

// =================================================
// HTML BAUSTEINE
// =================================================
// Zentrales Stylesheet für alle Seiten — dunkles, kartenbasiertes Layout
// mit Status-Badges statt reinem Fließtext.
const char WEB_CSS[] PROGMEM =
  ":root{--bg:#0f1115;--card:#171a21;--card2:#12141a;--border:#262b36;"
  "--text:#e8eaed;--muted:#8d97a6;--accent:#4da3ff;--accent2:#2d6cdf;"
  "--danger:#e0596a;--ok:#3ddc84;--radius:10px;}"
  "*{box-sizing:border-box;}"
  "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,"
  "Helvetica,Arial,sans-serif;background:var(--bg);color:var(--text);"
  "margin:0;padding:0 0 40px;font-size:15px;line-height:1.45;}"
  ".container{max-width:640px;margin:0 auto;padding:0 18px;}"
  ".topcard{margin-top:18px;padding:14px 16px;}"
  ".brand{font-weight:600;font-size:15px;letter-spacing:.2px;}"
  "nav{display:flex;flex-wrap:wrap;gap:6px;margin-top:10px;}"
  "nav a{color:var(--muted);text-decoration:none;padding:6px 12px;"
  "border-radius:20px;font-size:13px;transition:background .15s,color .15s;}"
  "nav a:hover{background:#20242e;color:var(--text);}"
  "nav a.active{background:var(--accent2);color:#fff;}"
  "h1{font-size:19px;margin:20px 0 14px;}"
  "h2{font-size:14px;margin:0 0 10px;color:var(--muted);"
  "text-transform:uppercase;letter-spacing:.5px;}"
  ".card{background:var(--card);border:1px solid var(--border);"
  "border-radius:var(--radius);padding:16px;margin-bottom:16px;}"
  ".row{margin-bottom:8px;}"
  ".row:last-child{margin-bottom:0;}"
  ".hint{color:var(--muted);font-size:12.5px;margin-top:6px;}"
  ".badge{display:inline-flex;align-items:center;gap:7px;}"
  ".dot{width:8px;height:8px;border-radius:50%;flex-shrink:0;}"
  ".table-wrap{overflow-x:auto;}"
  "table{border-collapse:collapse;width:100%;font-size:13.5px;"
  "margin-bottom:4px;}"
  "th{text-align:left;color:var(--muted);font-weight:500;padding:8px 6px;"
  "border-bottom:1px solid var(--border);white-space:nowrap;}"
  "td{padding:8px 6px;border-bottom:1px solid var(--border);}"
  "tr:last-child td{border-bottom:none;}"
  "label{display:block;margin:12px 0 5px;font-size:13px;color:var(--muted);}"
  "input,select{background:var(--card2);color:var(--text);"
  "border:1px solid var(--border);padding:9px 10px;border-radius:8px;"
  "width:100%;max-width:340px;font-size:14px;font-family:inherit;}"
  "input:focus,select:focus{outline:none;border-color:var(--accent);}"
  "input[type=checkbox]{width:auto;accent-color:var(--accent2);"
  "vertical-align:middle;margin-right:4px;}"
  "input[type=number]{max-width:110px;}"
  "button{background:var(--accent2);color:#fff;border:none;"
  "padding:9px 16px;border-radius:8px;cursor:pointer;font-size:13.5px;"
  "font-family:inherit;margin:10px 6px 2px 0;transition:background .15s;}"
  "button:hover{background:var(--accent);}"
  "button.danger{background:var(--danger);}"
  "button.danger:hover{background:#ff6b7a;}"
  "button.small{padding:6px 12px;font-size:12.5px;margin:2px 6px 2px 0;}"
  "form.inline{display:inline;}"
  "footer{max-width:640px;margin:24px auto 0;padding:0 18px;"
  "color:var(--muted);font-size:12px;}";

String navLink(
  String path,String label,String activePage) {
  String cls =
    (path == activePage) ? " class='active'" : "";
  return "<a href='" + path + "'" + cls + ">" + label + "</a>";
}

// Ein globales togglePw(id, button) statt pro Passwortfeld dupliziertem
// Inline-JS: blendet den echten Wert nur auf Wunsch ein (Standard: als
// Punkte maskiert), damit gespeicherte Passwörter nicht offen auf der
// Seite stehen.
const char WEB_JS[] PROGMEM =
  "function togglePw(id,btn){var i=document.getElementById(id);"
  "if(i.type==='password'){i.type='text';btn.textContent='Verbergen';}"
  "else{i.type='password';btn.textContent='Anzeigen';}}";

String pwField(
  String id,String name,String value,int maxLen) {
  String h =
    "<div style='display:flex;gap:6px;align-items:center;'>";
  h += "<input type='password' id='" + id + "' name='" + name + "' value='" + value + "' maxlength='" + String(maxLen) + "'>";
  h += "<button type='button' class='small' onclick=\"togglePw('" + id + "',this)\">Anzeigen</button>";
  h += "</div>";
  return h;
}

String htmlHeader(
  String title,String activePage) {
  String h =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>DM3 Controller - " + title + "</title>"
    "<style>" + String(WEB_CSS) + "</style>"
    "<script>" + String(WEB_JS) + "</script></head><body><div class='container'>"
    "<div class='card topcard'><div class='brand'>DM3 Controller</div><nav>";
  h += navLink(
    "/","Status",activePage);
  h += navLink(
    "/wifi","WLAN",activePage);
  h += navLink(
    "/dm3","DM3",activePage);
  h += navLink(
    "/channels","Kanäle",activePage);
  h += navLink(
    "/settings","Einstellungen",activePage);
  h += navLink(
    "/webconfig","Web Config",activePage);
  h += "</nav></div><h1>" + title + "</h1>";
  return h;
}

String htmlFooter() {
  return "</div></body></html>";
}

// Kleines Status-Badge (farbiger Punkt + Text), z.B. für WLAN/DM3-Zeilen
String statusBadge(
  bool ok,String textOk,String textBad) {
  String color =
    ok ? "var(--ok)" : "var(--danger)";
  return "<span class='badge'><span class='dot' style='background:" + color + "'></span>" + (ok ? textOk : textBad) + "</span>";
}

void sendRedirect(
  String path) {
  server.sendHeader(
    "Location",path);
  server.send(
    303);
}

// =================================================
// / — STATUS-DASHBOARD
// =================================================
void handleRoot() {
  String h =
    htmlHeader(
      "Status","/");

  h += "<div class='card'>";
  h += "<h2>Verbindung</h2>";
  h += "<div class='row'>WLAN: " + statusBadge(WiFi.status() == WL_CONNECTED,"verbunden (" + WiFi.localIP().toString() + ")","nicht verbunden") + "</div>";
  if (
    apFallbackActive) {
    h += "<div class='row'>AP-Fallback aktiv: " + String(apSSID) + " (" + WiFi.softAPIP().toString() + ")</div>";
  }
  String dm3Extra =
    "verbunden (" + dm3IP.toString() + ")";
  if (
    dm3.connected() && dm3LatencySamples > 0) {
    dm3Extra += " <span class='hint'>· " + String(dm3LastLatencyMs) + " ms</span>";
  }
  h += "<div class='row'>DM3: " + statusBadge(dm3.connected(),dm3Extra,"offline (" + dm3IP.toString() + ")") + "</div>";
  h += "<div class='row'>Akku: " + String(batteryPercent) + "%</div>";
  h += "</div>";

  h += "<div class='card'>";
  h += "<h2>Kanal</h2>";
  String ch =
    (activeSlot == SLOT_MASTER) ? masterLabel() : channelSlotLabel(activeSlot);
  h += "<div class='row'>" + ch + (masterMute ? " <span style='color:var(--danger)'>MUTE</span>" : "") + (masterLevel != -99999 ? (" — " + String(masterLevel / 100.0,1) + " dB") : "") + "</div>";
  h += "</div>";

  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

// =================================================
// /wifi — GESPEICHERTE WLAN-PROFILE VERWALTEN
// =================================================
void handleWifiPage() {
  String h =
    htmlHeader(
      "WLAN-Profile","/wifi");

  h += "<div class='card'><h2>Gespeicherte Profile</h2><div class='table-wrap'>";
  h += "<table><tr><th>Name</th><th>SSID</th><th>Passwort</th><th></th></tr>";
  for (
    int i = 0; i < WIFI_PROFILE_MAX; i++) {
    if (
      !wifiProfiles[i].used)
      continue;
    h += "<tr><form class='inline' method='POST' action='/wifi/edit'>";
    h += "<input type='hidden' name='idx' value='" + String(i) + "'>";
    h += "<td><input name='name' value='" + String(wifiProfiles[i].name) + "' maxlength='" + String(WIFI_NAME_MAX_LEN) + "'></td>";
    h += "<td><input name='ssid' value='" + String(wifiProfiles[i].ssid) + "' maxlength='" + String(WIFI_MAX_LEN) + "' required></td>";
    h += "<td>" + pwField("wppass" + String(i),"pass",String(wifiProfiles[i].pass),WIFI_MAX_LEN) + "</td>";
    h += "<td><button type='submit' class='small'>Speichern</button></td>";
    h += "</form></tr>";
    h += "<tr><td colspan='4'>";
    h += "<form class='inline' method='POST' action='/wifi/connect'><input type='hidden' name='idx' value='" + String(i) + "'><button type='submit' class='small'>Verbinden</button></form>";
    h += "<form class='inline' method='POST' action='/wifi/delete' onsubmit=\"return confirm('Profil wirklich löschen?')\"><input type='hidden' name='idx' value='" + String(i) + "'><button type='submit' class='danger small'>Löschen</button></form>";
    h += "</td></tr>";
  }
  h += "</table></div>";
  if (
    wifiProfileCount() >= WIFI_PROFILE_MAX) {
    h += "<div class='hint'>Alle " + String(WIFI_PROFILE_MAX) + " Profil-Plätze belegt — erst eines löschen, um ein neues anzulegen.</div>";
  }
  h += "</div>";

  if (
    wifiProfileCount() < WIFI_PROFILE_MAX) {
    h += "<div class='card'><h2>Neues Profil</h2>";
    h += "<form method='POST' action='/wifi/add'>";
    h += "<label>Name<input name='name' maxlength='" + String(WIFI_NAME_MAX_LEN) + "'></label>";
    h += "<label>SSID<input name='ssid' maxlength='" + String(WIFI_MAX_LEN) + "' required></label>";
    h += "<label>Passwort</label>" + pwField("newpass","pass","",WIFI_MAX_LEN);
    h += "<div class='hint'>Leeres Passwort = offenes Netzwerk. Speichert nur, verbindet nicht automatisch.</div>";
    h += "<button type='submit'>Anlegen</button></form></div>";
  }

  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

void handleWifiAdd() {
  int freeSlot =
    findFreeWifiProfileSlot();
  String ssid =
    server.arg(
      "ssid");
  if (
    freeSlot != -1 && ssid.length() > 0) {
    strncpy(
      wifiProfiles[freeSlot].name,server.arg("name").c_str(),WIFI_NAME_MAX_LEN);
    wifiProfiles[freeSlot].name[WIFI_NAME_MAX_LEN] = '\0';
    strncpy(
      wifiProfiles[freeSlot].ssid,ssid.c_str(),WIFI_MAX_LEN);
    wifiProfiles[freeSlot].ssid[WIFI_MAX_LEN] = '\0';
    strncpy(
      wifiProfiles[freeSlot].pass,server.arg("pass").c_str(),WIFI_MAX_LEN);
    wifiProfiles[freeSlot].pass[WIFI_MAX_LEN] = '\0';
    wifiProfiles[freeSlot].used = true;
    saveWifiProfiles();
    Serial.println(
      "WEB: WIFI PROFILE ADDED");
  }
  sendRedirect(
    "/wifi");
}

void handleWifiEdit() {
  int idx =
    server.arg(
        "idx")
      .toInt();
  String ssid =
    server.arg(
      "ssid");
  if (
    idx >= 0 && idx < WIFI_PROFILE_MAX && wifiProfiles[idx].used && ssid.length() > 0) {
    strncpy(
      wifiProfiles[idx].name,server.arg("name").c_str(),WIFI_NAME_MAX_LEN);
    wifiProfiles[idx].name[WIFI_NAME_MAX_LEN] = '\0';
    strncpy(
      wifiProfiles[idx].ssid,ssid.c_str(),WIFI_MAX_LEN);
    wifiProfiles[idx].ssid[WIFI_MAX_LEN] = '\0';
    strncpy(
      wifiProfiles[idx].pass,server.arg("pass").c_str(),WIFI_MAX_LEN);
    wifiProfiles[idx].pass[WIFI_MAX_LEN] = '\0';
    saveWifiProfiles();
    Serial.println(
      "WEB: WIFI PROFILE EDITED");
  }
  sendRedirect(
    "/wifi");
}

void handleWifiConnect() {
  int idx =
    server.arg(
        "idx")
      .toInt();
  sendRedirect(
    "/wifi");
  if (
    idx >= 0 && idx < WIFI_PROFILE_MAX && wifiProfiles[idx].used) {
    connectToWifiProfile(
      idx);
    Serial.println(
      "WEB: WIFI PROFILE CONNECT");
  }
}

void handleWifiDelete() {
  int idx =
    server.arg(
        "idx")
      .toInt();
  if (
    idx >= 0 && idx < WIFI_PROFILE_MAX) {
    deleteWifiProfile(
      idx);
    Serial.println(
      "WEB: WIFI PROFILE DELETED");
  }
  sendRedirect(
    "/wifi");
}

// =================================================
// /dm3 — DM3-IP EINSTELLEN
// =================================================
void handleDm3Page() {
  String h =
    htmlHeader(
      "DM3-Verbindung","/dm3");
  h += "<div class='card'>";
  h += "<div class='row'>Status: " + statusBadge(dm3.connected(),"verbunden","offline") + "</div>";
  h += "<form method='POST' action='/dm3/set'>";
  h += "<label>DM3 IP-Adresse<input name='ip' value='" + dm3IP.toString() + "' pattern='^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$' required></label>";
  h += "<button type='submit'>Speichern</button></form></div>";
  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

void handleDm3Set() {
  String ip =
    server.arg(
      "ip");
  IPAddress parsed;
  if (
    parsed.fromString(
      ip)) {
    ipOctets[0] = parsed[0];
    ipOctets[1] = parsed[1];
    ipOctets[2] = parsed[2];
    ipOctets[3] = parsed[3];
    saveIPConfig();
    Serial.println(
      "WEB: DM3 IP SET");
  }
  sendRedirect(
    "/dm3");
}

// =================================================
// /channels — KANAL-SLOTS AKTIVIEREN/DEAKTIVIEREN + BELEGEN
// =================================================
void handleChannelsPage() {
  String h =
    htmlHeader(
      "Kanäle","/channels");
  h += "<div class='card'><form method='POST' action='/channels/set'><div class='table-wrap'>";
  h += "<table><tr><th>Typ</th><th>Aktiv</th><th>Kanalnummer</th><th>Name</th></tr>";
  for (
    int i = 0; i < EXTRA_SLOT_COUNT; i++) {
    h += "<tr><td>" + String(chTypeLabel[chSlots[i].type]) + "</td>";
    h += "<td><input type='checkbox' name='en" + String(i) + "'" + (chSlots[i].enabled ? " checked" : "") + "></td>";
    h += "<td><input type='number' name='num" + String(i) + "' value='" + String(chSlots[i].num) + "' min='1' max='" + String(chTypeMax[chSlots[i].type]) + "'></td>";
    h += "<td>" + channelSlotLabel(i) + "</td></tr>";
  }
  h += "</table></div><button type='submit'>Speichern</button></form></div>";
  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

void handleChannelsSet() {
  for (
    int i = 0; i < EXTRA_SLOT_COUNT; i++) {
    chSlots[i].enabled =
      server.hasArg(
        "en" + String(i));
    int num =
      server.arg(
          "num" + String(i))
        .toInt();
    chSlots[i].num =
      constrain(
        num,1,chTypeMax[chSlots[i].type]);
  }
  saveChannelConfig();
  Serial.println(
    "WEB: CHANNELS SET");
  sendRedirect(
    "/channels");
}

// =================================================
// /settings — STEP SIZE + SLEEP TIMEOUT
// =================================================
void handleSettingsPage() {
  String h =
    htmlHeader(
      "Einstellungen","/settings");
  h += "<div class='card'><form method='POST' action='/settings/set'>";
  h += "<label>Schrittweite<select name='step'>";
  for (
    int i = 0; i < STEP_COUNT; i++) {
    h += "<option value='" + String(i) + "'" + (i == stepIndex ? " selected" : "") + ">" + String(stepValues[i],1) + " dB</option>";
  }
  h += "</select></label>";
  h += "<label>Ruhezustand nach (Sekunden)<input type='number' name='sleep' value='" + String(sleepTimeoutMs / 1000) + "' min='" + String(SLEEP_TIMEOUT_MIN / 1000) + "' max='" + String(SLEEP_TIMEOUT_MAX / 1000) + "'></label>";
  h += "<button type='submit'>Speichern</button></form></div>";
  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

void handleSettingsSet() {
  int step =
    server.arg(
        "step")
      .toInt();
  stepIndex =
    constrain(
      step,0,STEP_COUNT - 1);
  encoderStep =
    stepValues[stepIndex];
  saveConfig();

  long sleepSec =
    server.arg(
        "sleep")
      .toInt();
  sleepTimeoutMs =
    constrain(
      sleepSec * 1000UL,SLEEP_TIMEOUT_MIN,SLEEP_TIMEOUT_MAX);
  saveSleepTimeout();

  Serial.println(
    "WEB: SETTINGS SET");
  sendRedirect(
    "/settings");
}

// =================================================
// /webconfig — SERVER-FREIGABE + AP-PASSWORT
// =================================================
void handleWebConfigPage() {
  String h =
    htmlHeader(
      "Web Config","/webconfig");
  h += "<div class='card'>";
  h += "<div class='row'>AP-Name: " + String(apSSID) + "</div>";
  if (
    apFallbackActive) {
    h += "<div class='row'>" + statusBadge(true,"AP-Fallback aktiv (kein WLAN gefunden), IP: " + WiFi.softAPIP().toString(),"") + "</div>";
  }
  h += "<form method='POST' action='/webconfig/toggle'>";
  h += "<label><input type='checkbox' name='enabled'" + String(webConfigEnabled ? " checked" : "") + "> Weboberfläche auch im normalen WLAN-Betrieb aktiv halten</label>";
  h += "<button type='submit'>Speichern</button></form>";
  h += "</div>";

  h += "<div class='card'><h2>AP-Passwort</h2><form method='POST' action='/webconfig/appass'>";
  h += "<label>Passwort (mind. 8 Zeichen, leer = offenes Netz)</label>" + pwField("appass","pass",String(apPassword),WIFI_MAX_LEN);
  h += "<button type='submit'>Speichern</button></form></div>";

  h += htmlFooter();
  server.send(
    200,"text/html",h);
}

void handleWebConfigToggle() {
  webConfigEnabled =
    server.hasArg(
      "enabled");
  saveWebConfig();
  Serial.println(
    "WEB: WEB CONFIG TOGGLE SET");
  sendRedirect(
    "/webconfig");
}

void handleWebConfigAppass() {
  String pass =
    server.arg(
      "pass");
  if (
    pass.length() == 0 || pass.length() >= 8) {
    strncpy(
      apPassword,pass.c_str(),WIFI_MAX_LEN);
    apPassword[WIFI_MAX_LEN] = '\0';
    saveWebConfig();
    if (
      apFallbackActive) {
      WiFi.softAPdisconnect(
        true);
      WiFi.softAP(
        apSSID,apPassword);
    }
    Serial.println(
      "WEB: AP PASSWORD SET");
  } else {
    Serial.println(
      "WEB: AP PASSWORD TOO SHORT, IGNORED");
  }
  sendRedirect(
    "/webconfig");
}

void handleNotFound() {
  server.send(
    404,"text/plain","Not found");
}

// =================================================
// ROUTEN REGISTRIEREN (einmalig in setup(), unabhängig davon ob der
// Server gerade läuft — start/stop passiert separat über updateWebServer())
// =================================================
void setupWebServerRoutes() {
  server.on(
    "/",HTTP_GET,handleRoot);
  server.on(
    "/wifi",HTTP_GET,handleWifiPage);
  server.on(
    "/wifi/add",HTTP_POST,handleWifiAdd);
  server.on(
    "/wifi/edit",HTTP_POST,handleWifiEdit);
  server.on(
    "/wifi/connect",HTTP_POST,handleWifiConnect);
  server.on(
    "/wifi/delete",HTTP_POST,handleWifiDelete);
  server.on(
    "/dm3",HTTP_GET,handleDm3Page);
  server.on(
    "/dm3/set",HTTP_POST,handleDm3Set);
  server.on(
    "/channels",HTTP_GET,handleChannelsPage);
  server.on(
    "/channels/set",HTTP_POST,handleChannelsSet);
  server.on(
    "/settings",HTTP_GET,handleSettingsPage);
  server.on(
    "/settings/set",HTTP_POST,handleSettingsSet);
  server.on(
    "/webconfig",HTTP_GET,handleWebConfigPage);
  server.on(
    "/webconfig/toggle",HTTP_POST,handleWebConfigToggle);
  server.on(
    "/webconfig/appass",HTTP_POST,handleWebConfigAppass);
  server.onNotFound(
    handleNotFound);
}
