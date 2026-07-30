// =================================================
// LABEL SCROLLEN LASSEN, WENN ES NICHT IN SEINE SPALTE PASST
// =================================================
// Läuft einmal nach links raus und beginnt dann wieder von rechts —
// kein nahtloser Loop, reicht aber für die kurzen Kanalnamen hier.
void drawScrollingLabel(
  U8G2 &d,int x0,int x1,int y,String text,int &offset,unsigned long &lastMove) {
  int colWidth =
    x1 - x0;
  int textWidth =
    d.getStrWidth(
      text.c_str());
  d.setClipWindow(
    x0,0,x1,64);
  if (
    textWidth <= colWidth) {
    d.drawStr(
      x0,y,
      text.c_str());
    offset = 0;
  } else {
    if (
      millis() - lastMove >= 120) {
      offset -= 2;
      if (
        offset < -(textWidth + 12)) {
        offset = colWidth;
      }
      lastMove = millis();
    }
    d.drawStr(
      x0 + offset,y,
      text.c_str());
  }
  d.setMaxClipWindow();
}

// =================================================
// WEB-SYMBOL (kleiner Globus: Kreis + Meridian/Äquator) — zeigt an, dass
// der Web-Konfigurationsserver gerade unter der davor stehenden IP
// erreichbar ist.
// =================================================
void drawWebIcon(
  U8G2 &d,int x,int y) {
  int cx =
    x + 4;
  int cy =
    y + 4;
  d.drawCircle(
    cx,cy,4);
  d.drawLine(
    cx,cy - 4,cx,cy + 4);
  d.drawLine(
    cx - 4,cy,cx + 4,cy);
}

// =================================================
// KANAL-ANSICHT (Label + Pegel) für die 128x32-Displays 1 und 2 — beide
// zeigen IMMER ihren Kanal, unabhängig vom Menüzustand.
// =================================================
void drawChannelView(
  U8G2 &d,int slot,int level,bool mute,bool connected,int &scrollOffset,unsigned long &lastScrollMove) {
  d.setFont(
    u8g2_font_6x12_tf);
  String label =
    (slot == SLOT_MASTER) ? masterLabel() : channelSlotLabel(slot);
  String line =
    label + (mute ? " MUTE" : "");
  drawScrollingLabel(
    d,0,128,10,line,scrollOffset,lastScrollMove);

  // DM3-Verbindungsstatus (WAIT DM3/OFFLINE) wird nur auf dem Menü-
  // Display (oledMenu) gezeigt, nicht hier — hier bleibt bei fehlender
  // Verbindung einfach nur der Kanalname sichtbar, keine Pegelzahl.
  if (
    !connected || level == -99999) {
    return;
  }
  d.setFont(
    u8g2_font_logisoso16_tf);
  d.drawStr(
    0,30,
    String(level / 100.0,1).c_str());
  d.setFont(
    u8g2_font_6x12_tf);
}

// =================================================
// DISPLAY 1 (Kanal 1) — immer aktiv, unabhängig vom Menüzustand.
// Die Menü-Navigation läuft komplett über oledMenu (drawMenuScreen).
// =================================================
void drawChannel1Screen() {
  oled1.clearBuffer();
  // Im Ruhezustand bleibt Display 1 einfach leer (dunkel) — "SLEEP"
  // erscheint nur auf dem Menü-Display (oledMenu).
  if (
    menu == MENU_SLEEP) {
    oled1.sendBuffer();
    return;
  }
  static int scrollOffset1 = 0;
  static unsigned long lastScrollMove1 = 0;
  drawChannelView(
    oled1,activeSlot1,level1,mute1,dm3.connected(),scrollOffset1,lastScrollMove1);
  oled1.sendBuffer();
}

// =================================================
// DISPLAY 2 (Kanal 2) — immer aktiv, unabhängig vom Menüzustand
// =================================================
void drawChannel2Screen() {
  oled2.clearBuffer();
  // Im Ruhezustand bleibt Display 2 einfach leer (dunkel) — "SLEEP"
  // erscheint nur auf dem Menü-Display (oledMenu).
  if (
    menu == MENU_SLEEP) {
    oled2.sendBuffer();
    return;
  }
  static int scrollOffset2 = 0;
  static unsigned long lastScrollMove2 = 0;
  drawChannelView(
    oled2,activeSlot2,level2,mute2,dm3.connected(),scrollOffset2,lastScrollMove2);
  oled2.sendBuffer();
}

// =================================================
// ONBOARD-OLED (128x64) — dediziertes Menü-/SETTINGS-Display, komplett
// von den beiden Kanal-Displays getrennt. Zeigt außerhalb des Menüs
// (MENU_MASTER) ein einfaches Status-Dashboard.
// =================================================
void drawMenuScreen() {
  oledMenu.clearBuffer();

  // =================================================
  // RUHEZUSTAND
  // =================================================
  if (
    menu == MENU_SLEEP) {
    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    oledMenu.drawStr(
      25,
      40,
      "SLEEP");
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // SETTINGS
  // =================================================
  if (
    menu == MENU_SETTINGS) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "SETTINGS");
    drawBatteryIndicator(
      oledMenu,108,2);

    const char* items[SETTINGS_COUNT] = {
      "CHANNELS","STEP SIZE","SLEEP TIME","DM3 IP","WIFI","WEB CONFIG","BACK"
    };
    // Fenster statt fester Liste: passt nicht mehr komplett auf den
    // Bildschirm, seit WEB CONFIG dazugekommen ist (7 Einträge à 8px ab
    // y=22 würden bis y=70 reichen, das Display ist aber nur 64px hoch —
    // BACK wäre unsichtbar und der Marker beim Weiterscrollen verschwunden).
    int visible = 6;
    int start =
      settingsIndex;
    if (
      start > SETTINGS_COUNT - visible) {
      start = SETTINGS_COUNT - visible;
    }
    if (
      start < 0) {
      start = 0;
    }
    for (
      int row = 0; row < visible && (start + row) < SETTINGS_COUNT; row++) {
      int i =
        start + row;
      String line =
        (i == settingsIndex) ? "> " : "  ";
      line += items[i];
      oledMenu.drawStr(
        0,
        22 + row * 8,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL-TYP LISTE (INPUT/MIX/MATRIX/BACK)
  // =================================================
  if (
    menu == MENU_CHANNEL_TYPE) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "CHANNELS");
    drawBatteryIndicator(
      oledMenu,108,2);

    String labels[CHANNEL_TYPE_MENU_COUNT];
    labels[0] = "INPUT";
    labels[1] = "MIX";
    labels[2] = "MATRIX";
    labels[3] = "BACK";
    for (
      int i = 0; i < CHANNEL_TYPE_MENU_COUNT; i++) {
      String line =
        (i == channelTypeMenuIndex) ? "> " : "  ";
      line += labels[i];
      oledMenu.drawStr(
        0,
        24 + i * 10,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL LISTE (SLOTS FÜR GEWÄHLTEN TYP)
  // =================================================
  if (
    menu == MENU_CHANNEL) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      chTypeLabel[channelMenuType]);
    drawBatteryIndicator(
      oledMenu,108,2);

    int base =
      typeSlotBase(channelMenuType);
    int count =
      typeSlotCount(channelMenuType);
    // Bis zu 6 Input-Slots + BACK passen nicht mehr alle gleichzeitig auf
    // die 64px Höhe (nur 4 Zeilen Platz nach der Kopfzeile) — deshalb ein
    // Scroll-Fenster, das der Auswahl folgt, statt alles auf einmal zu zeigen.
    int totalItems =
      count + 1;
    int visible = 4;
    int start =
      channelMenuIndex;
    if (
      start > totalItems - visible) {
      start = totalItems - visible;
    }
    if (
      start < 0) {
      start = 0;
    }
    for (
      int row = 0; row < visible && (start + row) < totalItems; row++) {
      int i =
        start + row;
      String line =
        (i == channelMenuIndex) ? "> " : "  ";
      if (
        i == count) {
        line += "BACK";
      } else {
        int slotIdx = base + i;
        line +=
          "SLOT " + String(i + 1) + ": " + channelSlotLabel(slotIdx)
          + (chSlots[slotIdx].enabled ? " *" : "");
      }
      oledMenu.drawStr(
        0,
        24 + row * 10,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL-ITEM (PRO SLOT: ACTIVATE/SET CHANNEL/BACK)
  // =================================================
  if (
    menu == MENU_CHANNEL_ITEM) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(chTypeLabel[chSlots[channelItemSlot].type]) + " SLOT";
    oledMenu.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      oledMenu,108,2);

    String items[CHANNEL_ITEM_COUNT];
    items[0] =
      chSlots[channelItemSlot].enabled ? "DEACTIVATE" : "ACTIVATE";
    items[1] =
      "SET CHANNEL: " + channelSlotLabel(channelItemSlot);
    items[2] =
      "BACK";
    for (
      int i = 0; i < CHANNEL_ITEM_COUNT; i++) {
      String line =
        (i == channelItemIndex) ? "> " : "  ";
      line += items[i];
      oledMenu.drawStr(
        0,
        26 + i * 12,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // KANAL-SLOT BEARBEITEN
  // =================================================
  if (
    menu == MENU_CHANNEL_SLOT) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(chTypeLabel[chSlots[channelEditSlot].type]) + " CHANNEL";
    oledMenu.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      oledMenu,108,2);

    int type =
      chSlots[channelEditSlot].type;
    String preview =
      (type == CT_INCH) ? inputChannelLabel(channelEditNum) : (String(chTypeLabel[type]) + " " + String(channelEditNum));
    oledMenu.drawStr(
      0,
      30,
      preview.c_str());

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    oledMenu.drawStr(
      40,
      58,
      String(channelEditNum).c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // RUHEZUSTAND-ZEIT
  // =================================================
  if (
    menu == MENU_SLEEP_TIME) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "SLEEP TIME");
    drawBatteryIndicator(
      oledMenu,108,2);

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    String val =
      String(sleepTimeoutMs / 1000) + "s";
    oledMenu.drawStr(
      20,
      50,
      val.c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WLAN-HUB (NEW/SAVED/BACK)
  // =================================================
  if (
    menu == MENU_WIFI) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "WIFI");
    drawBatteryIndicator(
      oledMenu,108,2);

    const char* items[WIFI_MENU_COUNT] = {
      "NEW","SAVED","BACK"
    };
    for (
      int i = 0; i < WIFI_MENU_COUNT; i++) {
      String line =
        (i == wifiMenuIndex) ? "> " : "  ";
      line += items[i];
      oledMenu.drawStr(
        0,
        24 + i * 10,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WLAN GESPEICHERTE PROFILE (zum schnellen Verbinden, + EDIT/BACK)
  // =================================================
  if (
    menu == MENU_WIFI_SAVED_LIST) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "SAVED WIFI");
    drawBatteryIndicator(
      oledMenu,108,2);

    int count =
      wifiProfileCount();
    int totalItems =
      count + 2;
    int visible = 4;
    int start =
      wifiSavedListIndex;
    if (
      start > totalItems - visible) {
      start = totalItems - visible;
    }
    if (
      start < 0) {
      start = 0;
    }
    for (
      int row = 0; row < visible && (start + row) < totalItems; row++) {
      int i =
        start + row;
      String line =
        (i == wifiSavedListIndex) ? "> " : "  ";
      if (
        i < count) {
        int idx =
          wifiProfileIndexAtListPosition(i);
        String name =
          String(wifiProfiles[idx].name);
        line +=
          (name.length() > 0) ? name : String(wifiProfiles[idx].ssid);
      } else if (
        i == count) {
        line += "EDIT";
      } else {
        line += "BACK";
      }
      oledMenu.drawStr(
        0,
        24 + row * 10,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFILE BEARBEITEN (Liste, + BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_LIST) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "EDIT WIFI");
    drawBatteryIndicator(
      oledMenu,108,2);

    int count =
      wifiProfileCount();
    int totalItems =
      count + 1;
    int visible = 4;
    int start =
      wifiEditListIndex;
    if (
      start > totalItems - visible) {
      start = totalItems - visible;
    }
    if (
      start < 0) {
      start = 0;
    }
    for (
      int row = 0; row < visible && (start + row) < totalItems; row++) {
      int i =
        start + row;
      String line =
        (i == wifiEditListIndex) ? "> " : "  ";
      if (
        i < count) {
        int idx =
          wifiProfileIndexAtListPosition(i);
        String name =
          String(wifiProfiles[idx].name);
        line +=
          (name.length() > 0) ? name : String(wifiProfiles[idx].ssid);
      } else {
        line += "BACK";
      }
      oledMenu.drawStr(
        0,
        24 + row * 10,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFIL-UNTERMENÜ (EDIT NAME/SSID/PASSWORD, DELETE, BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_ITEM) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(wifiProfiles[wifiEditProfileIndex].name);
    if (
      header.length() == 0) {
      header =
        String(wifiProfiles[wifiEditProfileIndex].ssid);
    }
    oledMenu.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      oledMenu,108,2);

    const char* items[WIFI_EDIT_ITEM_COUNT] = {
      "EDIT NAME","EDIT SSID","EDIT PASSWORD","DELETE","BACK"
    };
    for (
      int i = 0; i < WIFI_EDIT_ITEM_COUNT; i++) {
      String line =
        (i == wifiEditItemIndex) ? "> " : "  ";
      line += items[i];
      oledMenu.drawStr(
        0,
        24 + i * 8,
        line.c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFIL-FELD BEARBEITEN (Name/SSID/Passwort)
  // =================================================
  if (
    menu == MENU_WIFI_PROFILE_FIELD) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    String header =
      (wifiEditField == WEF_NAME) ? "SET NAME" : (wifiEditField == WEF_SSID) ? "SET SSID" : "SET PASSWORD";
    if (
      wifiEditIsNew) {
      header = "NEW: " + header;
    }
    oledMenu.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      oledMenu,108,2);

    // Aktuell gespeicherten Wert nur zur Anzeige (nicht editierbar) zeigen,
    // damit man ihn beim erneuten Eingeben zum Vergleich sieht. Der
    // Eingabepuffer selbst startet bewusst leer (siehe
    // loadWifiProfileFieldEdit), um das alte Puffer-Reste-Problem zu
    // vermeiden. Bei einem neuen Profil gibt es keinen alten Wert.
    if (
      !wifiEditIsNew) {
      String current =
        (wifiEditField == WEF_NAME) ? String(wifiProfiles[wifiEditProfileIndex].name) : (wifiEditField == WEF_SSID) ? String(wifiProfiles[wifiEditProfileIndex].ssid) : String(wifiProfiles[wifiEditProfileIndex].pass);
      if (
        current.length() > 21) {
        current =
          current.substring(0,21);
      }
      oledMenu.drawStr(
        0,
        22,
        ("was: " + current).c_str());
    }

    int start = 0;
    if (
      wifiEditCursor >= WIFI_VISIBLE_CHARS) {
      start = wifiEditCursor - WIFI_VISIBLE_CHARS + 1;
    }
    int endIdx =
      wifiEditLength < (start + WIFI_VISIBLE_CHARS) ? wifiEditLength : (start + WIFI_VISIBLE_CHARS);
    String line =
      "";
    for (
      int i = start; i < endIdx; i++) {
      if (
        i == wifiEditCursor) {
        line += "[";
        line += wifiEditBuffer[i];
        line += "]";
      } else {
        line += wifiEditBuffer[i];
      }
    }
    if (
      wifiEditCursor == wifiEditLength) {
      line += "_";
    }
    oledMenu.drawStr(
      0,
      34,
      line.c_str());

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    String big =
      (wifiCharIndex == (int)WIFI_CHARSET_LEN) ? "END" : String(wifiCharset[wifiCharIndex]);
    oledMenu.drawStr(
      20,
      58,
      big.c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // WEB CONFIG (Server AN/AUS, AP-Passwort, BACK)
  // =================================================
  if (
    menu == MENU_WEB_CONFIG) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "WEB CONFIG");
    drawBatteryIndicator(
      oledMenu,108,2);

    String serverLabel =
      webConfigEnabled ? "SERVER: ON" : "SERVER: OFF";
    const char* items[WEB_CONFIG_COUNT] = {
      serverLabel.c_str(),"AP PASSWORD","BACK"
    };
    for (
      int i = 0; i < WEB_CONFIG_COUNT; i++) {
      String line =
        (i == webConfigMenuIndex) ? "> " : "  ";
      line += items[i];
      oledMenu.drawStr(
        0,
        24 + i * 8,
        line.c_str());
    }

    // Erreichbarkeit direkt darunter anzeigen, je nach aktuellem Zustand —
    // Web-Adresse als eine Zeile statt "http://" und IP getrennt
    if (
      apFallbackActive) {
      oledMenu.drawStr(
        0,
        52,
        ("AP: " + String(apSSID)).c_str());
      oledMenu.drawStr(
        0,
        62,
        ("http://" + WiFi.softAPIP().toString()).c_str());
    } else if (
      webServerRunning && WiFi.status() == WL_CONNECTED) {
      oledMenu.drawStr(
        0,
        52,
        ("http://" + WiFi.localIP().toString()).c_str());
    }
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // AP-PASSWORT BEARBEITEN
  // =================================================
  if (
    menu == MENU_AP_PASS_EDIT) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "SET AP PASSWORD");
    drawBatteryIndicator(
      oledMenu,108,2);

    String current =
      String(apPassword);
    if (
      current.length() > 21) {
      current =
        current.substring(0,21);
    }
    oledMenu.drawStr(
      0,
      22,
      ("was: " + current).c_str());

    int start = 0;
    if (
      wifiEditCursor >= WIFI_VISIBLE_CHARS) {
      start = wifiEditCursor - WIFI_VISIBLE_CHARS + 1;
    }
    int endIdx =
      wifiEditLength < (start + WIFI_VISIBLE_CHARS) ? wifiEditLength : (start + WIFI_VISIBLE_CHARS);
    String line =
      "";
    for (
      int i = start; i < endIdx; i++) {
      if (
        i == wifiEditCursor) {
        line += "[";
        line += wifiEditBuffer[i];
        line += "]";
      } else {
        line += wifiEditBuffer[i];
      }
    }
    if (
      wifiEditCursor == wifiEditLength) {
      line += "_";
    }
    oledMenu.drawStr(
      0,
      34,
      line.c_str());

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    String big =
      (wifiCharIndex == (int)WIFI_CHARSET_LEN) ? "END" : String(wifiCharset[wifiCharIndex]);
    oledMenu.drawStr(
      20,
      58,
      big.c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // IP KONFIGURATION
  // =================================================
  if (
    menu == MENU_IP) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "SET DM3 IP");
    drawBatteryIndicator(
      oledMenu,108,2);

    String ipStr =
      "";
    for (
      int i = 0; i < 4; i++) {
      if (
        i == ipEditIndex) {
        ipStr +=
          "[" + String(ipOctets[i]) + "]";
      } else {
        ipStr +=
          String(ipOctets[i]);
      }
      if (
        i < 3)
        ipStr += ".";
    }
    oledMenu.drawStr(
      0,
      30,
      ipStr.c_str());

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    oledMenu.drawStr(
      40,
      58,
      String(ipOctets[ipEditIndex]).c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // STEP MENU (gleiches Layout wie SLEEP TIME: Kopfzeile + Akku oben,
  // große Zahl darunter)
  // =================================================
  if (
    menu == MENU_STEP) {
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.drawStr(
      0,
      12,
      "STEP SIZE");
    drawBatteryIndicator(
      oledMenu,108,2);

    oledMenu.setFont(
      u8g2_font_logisoso20_tf);
    String step =
      String(
        encoderStep,
        1)
      + "dB";
    oledMenu.drawStr(
      20,
      50,
      step.c_str());
    oledMenu.setFont(
      u8g2_font_6x12_tf);
    oledMenu.sendBuffer();
    return;
  }

  // =================================================
  // MENU_MASTER (kein Untermenü offen) — Status-Dashboard, da Kanal 1
  // und Kanal 2 schon permanent auf den eigenen Displays laufen
  // =================================================
  oledMenu.setFont(
    u8g2_font_6x12_tf);
  oledMenu.drawStr(
    0,
    12,
    "DM3 CONTROLLER");
  drawBatteryIndicator(
    oledMenu,108,2);

  bool connected =
    dm3.connected();
  String dm3Line =
    connected ? "DM3: CONNECTED" : ((millis() - bootTime < DM3_WAIT_TIME) ? "DM3: WAIT" : "DM3: OFFLINE");
  if (
    connected && dm3LatencySamples > 0) {
    dm3Line +=
      " " + String(dm3AvgLatencyMs) + "ms";
  }
  oledMenu.drawStr(
    0,
    30,
    dm3Line.c_str());
  // Im AP-Fallback ersetzt "APMODE: <AP-IP>" die normale WIFI-Zeile
  // komplett, statt beides gleichzeitig anzuzeigen — die AP-IP ist in dem
  // Moment die einzige, unter der überhaupt etwas erreichbar ist.
  String wifiLine =
    apFallbackActive ? ("APMODE: " + WiFi.softAPIP().toString()) : (WiFi.status() == WL_CONNECTED ? ("WIFI: " + WiFi.localIP().toString()) : "WIFI: --");
  oledMenu.drawStr(
    0,
    44,
    wifiLine.c_str());
  // Kleiner Globus direkt hinter der IP, wenn der Web-Konfigurationsserver
  // gerade unter genau dieser Adresse erreichbar ist
  if (
    apFallbackActive || (webServerRunning && WiFi.status() == WL_CONNECTED)) {
    drawWebIcon(
      oledMenu,oledMenu.getStrWidth(wifiLine.c_str()) + 4,36);
  }
  oledMenu.drawStr(
    0,
    58,
    "MENU-TASTER: SETTINGS");
  oledMenu.sendBuffer();
}
