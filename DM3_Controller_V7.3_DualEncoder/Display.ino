// =================================================
// LABEL SCROLLEN LASSEN, WENN ES NICHT IN SEINE SPALTE PASST
// =================================================
// Läuft einmal nach links raus und beginnt dann wieder von rechts —
// kein nahtloser Loop, reicht aber für die kurzen Kanalnamen hier.
void drawScrollingLabel(
  int x0,int x1,int y,String text,int &offset,unsigned long &lastMove) {
  int colWidth =
    x1 - x0;
  int textWidth =
    oled.getStrWidth(
      text.c_str());
  oled.setClipWindow(
    x0,0,x1,64);
  if (
    textWidth <= colWidth) {
    oled.drawStr(
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
    oled.drawStr(
      x0 + offset,y,
      text.c_str());
  }
  oled.setMaxClipWindow();
}

// =================================================
// WEB-SYMBOL (kleiner Globus: Kreis + Meridian/Äquator) — zeigt an, dass
// der Web-Konfigurationsserver gerade erreichbar ist. Steht direkt vor
// dem Akku-Symbol.
// =================================================
void drawWebIcon(
  int x,int y) {
  int cx =
    x + 4;
  int cy =
    y + 4;
  oled.drawCircle(
    cx,cy,4);
  oled.drawLine(
    cx,cy - 4,cx,cy + 4);
  oled.drawLine(
    cx - 4,cy,cx + 4,cy);
}

// =================================================
// OLED
// =================================================
void drawScreen() {
  oled.clearBuffer();

  // =================================================
  // RUHEZUSTAND
  // =================================================
  if (
    menu == MENU_SLEEP) {
    oled.setFont(
      u8g2_font_logisoso20_tf);
    oled.drawStr(
      25,
      40,
      "SLEEP");
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  // =================================================
  // SETTINGS
  // =================================================
  if (
    menu == MENU_SETTINGS) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "SETTINGS");
    drawBatteryIndicator(
      108,2);

    const char* items[SETTINGS_COUNT] = {
      "CHANNELS","STEP SIZE","SLEEP TIME","DM3 IP","WIFI","WEB CONFIG","BACK"
    };
    // Fenster statt fester Liste: 7 Einträge à 8px ab y=22 würden bis
    // y=70 reichen, das Display ist aber nur 64px hoch — BACK wäre
    // unsichtbar und der Marker beim Weiterscrollen verschwunden.
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
      oled.drawStr(
        0,
        22 + row * 8,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL-TYP LISTE (INPUT/MIX/MATRIX/BACK)
  // =================================================
  if (
    menu == MENU_CHANNEL_TYPE) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "CHANNELS");
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
        0,
        24 + i * 10,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL LISTE (SLOTS FÜR GEWÄHLTEN TYP)
  // =================================================
  if (
    menu == MENU_CHANNEL) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      chTypeLabel[channelMenuType]);
    drawBatteryIndicator(
      108,2);

    int base =
      typeSlotBase(channelMenuType);
    int count =
      typeSlotCount(channelMenuType);
    for (
      int i = 0; i < count; i++) {
      int slotIdx = base + i;
      String line =
        (i == channelMenuIndex) ? "> " : "  ";
      line +=
        "SLOT " + String(i + 1) + ": " + channelSlotLabel(slotIdx)
        + (chSlots[slotIdx].enabled ? " *" : "");
      oled.drawStr(
        0,
        24 + i * 10,
        line.c_str());
    }
    String backLine =
      (channelMenuIndex == count) ? "> BACK" : "  BACK";
    oled.drawStr(
      0,
      24 + count * 10,
      backLine.c_str());
    oled.sendBuffer();
    return;
  }

  // =================================================
  // CHANNEL-ITEM (PRO SLOT: ACTIVATE/SET CHANNEL/BACK)
  // =================================================
  if (
    menu == MENU_CHANNEL_ITEM) {
    oled.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(chTypeLabel[chSlots[channelItemSlot].type]) + " SLOT";
    oled.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
        0,
        26 + i * 12,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // KANAL-SLOT BEARBEITEN
  // =================================================
  if (
    menu == MENU_CHANNEL_SLOT) {
    oled.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(chTypeLabel[chSlots[channelEditSlot].type]) + " CHANNEL";
    oled.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      108,2);

    int type =
      chSlots[channelEditSlot].type;
    String preview =
      (type == CT_INCH) ? inputChannelLabel(channelEditNum) : (String(chTypeLabel[type]) + " " + String(channelEditNum));
    oled.drawStr(
      0,
      30,
      preview.c_str());

    oled.setFont(
      u8g2_font_logisoso20_tf);
    oled.drawStr(
      40,
      58,
      String(channelEditNum).c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  // =================================================
  // RUHEZUSTAND-ZEIT
  // =================================================
  if (
    menu == MENU_SLEEP_TIME) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "SLEEP TIME");
    drawBatteryIndicator(
      108,2);

    oled.setFont(
      u8g2_font_logisoso20_tf);
    String val =
      String(sleepTimeoutMs / 1000) + "s";
    oled.drawStr(
      20,
      50,
      val.c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WLAN-HUB (SSID/PASSWORD/BACK)
  // =================================================
  if (
    menu == MENU_WIFI) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "WIFI");
    drawBatteryIndicator(
      108,2);

    const char* items[WIFI_MENU_COUNT] = {
      "NEW","SAVED","BACK"
    };
    for (
      int i = 0; i < WIFI_MENU_COUNT; i++) {
      String line =
        (i == wifiMenuIndex) ? "> " : "  ";
      line += items[i];
      oled.drawStr(
        0,
        24 + i * 10,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WLAN GESPEICHERTE PROFILE (zum schnellen Verbinden, + EDIT/BACK)
  // =================================================
  if (
    menu == MENU_WIFI_SAVED_LIST) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "SAVED WIFI");
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
        0,
        24 + row * 10,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFILE BEARBEITEN (Liste, + BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_LIST) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "EDIT WIFI");
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
        0,
        24 + row * 10,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFIL-UNTERMENÜ (EDIT NAME/SSID/PASSWORD, DELETE, BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_ITEM) {
    oled.setFont(
      u8g2_font_6x12_tf);
    String header =
      String(wifiProfiles[wifiEditProfileIndex].name);
    if (
      header.length() == 0) {
      header =
        String(wifiProfiles[wifiEditProfileIndex].ssid);
    }
    oled.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      108,2);

    const char* items[WIFI_EDIT_ITEM_COUNT] = {
      "EDIT NAME","EDIT SSID","EDIT PASSWORD","DELETE","BACK"
    };
    for (
      int i = 0; i < WIFI_EDIT_ITEM_COUNT; i++) {
      String line =
        (i == wifiEditItemIndex) ? "> " : "  ";
      line += items[i];
      oled.drawStr(
        0,
        24 + i * 8,
        line.c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WLAN PROFIL-FELD BEARBEITEN (Name/SSID/Passwort)
  // =================================================
  if (
    menu == MENU_WIFI_PROFILE_FIELD) {
    oled.setFont(
      u8g2_font_6x12_tf);
    String header =
      (wifiEditField == WEF_NAME) ? "SET NAME" : (wifiEditField == WEF_SSID) ? "SET SSID" : "SET PASSWORD";
    if (
      wifiEditIsNew) {
      header = "NEW: " + header;
    }
    oled.drawStr(
      0,
      12,
      header.c_str());
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
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
    oled.drawStr(
      0,
      34,
      line.c_str());

    oled.setFont(
      u8g2_font_logisoso20_tf);
    String big =
      (wifiCharIndex == (int)WIFI_CHARSET_LEN) ? "END" : String(wifiCharset[wifiCharIndex]);
    oled.drawStr(
      20,
      58,
      big.c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  // =================================================
  // WEB CONFIG (Server AN/AUS, AP-Passwort, BACK)
  // =================================================
  if (
    menu == MENU_WEB_CONFIG) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "WEB CONFIG");
    drawBatteryIndicator(
      108,2);

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
      oled.drawStr(
        0,
        24 + i * 8,
        line.c_str());
    }

    // Erreichbarkeit direkt darunter anzeigen, je nach aktuellem Zustand —
    // Web-Adresse als eine Zeile statt "http://" und IP getrennt
    if (
      apFallbackActive) {
      oled.drawStr(
        0,
        52,
        ("AP: " + String(apSSID)).c_str());
      oled.drawStr(
        0,
        62,
        ("http://" + WiFi.softAPIP().toString()).c_str());
    } else if (
      webServerRunning && WiFi.status() == WL_CONNECTED) {
      oled.drawStr(
        0,
        52,
        ("http://" + WiFi.localIP().toString()).c_str());
    }
    oled.sendBuffer();
    return;
  }

  // =================================================
  // AP-PASSWORT BEARBEITEN
  // =================================================
  if (
    menu == MENU_AP_PASS_EDIT) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "SET AP PASSWORD");
    drawBatteryIndicator(
      108,2);

    String current =
      String(apPassword);
    if (
      current.length() > 21) {
      current =
        current.substring(0,21);
    }
    oled.drawStr(
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
    oled.drawStr(
      0,
      34,
      line.c_str());

    oled.setFont(
      u8g2_font_logisoso20_tf);
    String big =
      (wifiCharIndex == (int)WIFI_CHARSET_LEN) ? "END" : String(wifiCharset[wifiCharIndex]);
    oled.drawStr(
      20,
      58,
      big.c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  // =================================================
  // IP KONFIGURATION
  // =================================================
  if (
    menu == MENU_IP) {
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.drawStr(
      0,
      12,
      "SET DM3 IP");
    drawBatteryIndicator(
      108,2);

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
    oled.drawStr(
      0,
      30,
      ipStr.c_str());

    oled.setFont(
      u8g2_font_logisoso20_tf);
    oled.drawStr(
      40,
      58,
      String(ipOctets[ipEditIndex]).c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
    oled.sendBuffer();
    return;
  }

  oled.setFont(
    u8g2_font_6x12_tf);
  oled.drawStr(
    0,
    12,
    "DM3 REMOTE V7.3");
  // Web-Symbol direkt vor dem Akku-Symbol, wenn der Konfigurationsserver
  // gerade erreichbar ist (AP-Fallback oder manuell aktiviert im WLAN)
  if (
    apFallbackActive || (webServerRunning && WiFi.status() == WL_CONNECTED)) {
    drawWebIcon(
      94,2);
  }
  drawBatteryIndicator(
    108,2);

  // =================================================
  // MASTER MENU (zwei Kanäle gleichzeitig, vertikal geteilt: Encoder 1
  // links, Encoder 2 rechts)
  // =================================================
  if (
    menu == MENU_MASTER) {
    bool connected =
      dm3.connected();

    // Trennlinie, nur im mittleren Bereich (lässt oben Platz für die
    // Kopfzeile und unten für die Statuszeile frei)
    oled.drawVLine(
      64,14,40);

    // Kanal 1 (Encoder 1), linke Hälfte
    static int scrollOffset1 = 0;
    static unsigned long lastScrollMove1 = 0;
    String label1 =
      (activeSlot1 == SLOT_MASTER) ? masterLabel() : channelSlotLabel(activeSlot1);
    String line1 =
      label1 + (mute1 ? " MUTE" : "");
    drawScrollingLabel(
      0,62,26,line1,scrollOffset1,lastScrollMove1);
    if (
      connected && level1 != -99999) {
      oled.setFont(
        u8g2_font_logisoso16_tf);
      oled.drawStr(
        0,
        52,
        String(level1 / 100.0,1).c_str());
      oled.setFont(
        u8g2_font_6x12_tf);
    }

    // Kanal 2 (Encoder 2), rechte Hälfte
    static int scrollOffset2 = 0;
    static unsigned long lastScrollMove2 = 0;
    String label2 =
      (activeSlot2 == SLOT_MASTER) ? masterLabel() : channelSlotLabel(activeSlot2);
    String line2 =
      label2 + (mute2 ? " MUTE" : "");
    drawScrollingLabel(
      66,127,26,line2,scrollOffset2,lastScrollMove2);
    if (
      connected && level2 != -99999) {
      oled.setFont(
        u8g2_font_logisoso16_tf);
      oled.drawStr(
        66,
        52,
        String(level2 / 100.0,1).c_str());
      oled.setFont(
        u8g2_font_6x12_tf);
    }

    // =================================================
    // Statuszeile unten: AP-Fallback-Info > DM3-Verbindungsstatus >
    // DM3-Antwortzeit (Reihenfolge nach Relevanz — nur eine passt gerade).
    // Scrollt automatisch, falls der Text (v.a. "AP: <SSID> <IP>") nicht
    // in die Zeile passt.
    // =================================================
    static int scrollOffsetStatus = 0;
    static unsigned long lastScrollMoveStatus = 0;
    String statusLine =
      "";
    if (
      apFallbackActive) {
      statusLine =
        "AP: " + String(apSSID) + " " + WiFi.softAPIP().toString();
    } else if (
      !connected) {
      statusLine =
        (millis() - bootTime < DM3_WAIT_TIME) ? "WAIT DM3" : "DM3 OFFLINE";
    } else if (
      dm3LatencySamples > 0) {
      statusLine =
        "DM3: " + String(dm3AvgLatencyMs) + "ms";
    }
    if (
      statusLine.length() > 0) {
      drawScrollingLabel(
        0,128,62,statusLine,scrollOffsetStatus,lastScrollMoveStatus);
    }
  }

  // =================================================
  // STEP MENU
  // =================================================
  else {
    oled.drawStr(
      0,
      30,
      "STEP SIZE");
    oled.setFont(
      u8g2_font_logisoso20_tf);
    String step =
      String(
        encoderStep,
        1)
      + "dB";
    oled.drawStr(
      25,
      55,
      step.c_str());
    oled.setFont(
      u8g2_font_6x12_tf);
  }
  oled.sendBuffer();
}
