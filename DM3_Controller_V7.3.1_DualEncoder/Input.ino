// =================================================
// ENCODER 1 (Kanal 1 / Menü-Navigation)
// =================================================
void handleEncoder1() {
  long pos =
    encoder1.getCount();
  long movement =
    pos - lastEncoder1;
  if (
    abs(movement) < 2)
    return;
  lastEncoder1 = pos;
  lastActivity =
    millis();
  int delta =
    movement / 2;
  if (
    delta == 0)
    return;

  // Beschleunigung für schnelles Drehen (nur Kanal-Pegel):
  // je kürzer der Abstand zum letzten Encoder-Schritt, desto größer
  // der Multiplikator, damit man große Sprünge (z.B. aus -inf) schnell
  // erreicht statt hunderte Klicks zu brauchen.
  static unsigned long lastMoveTime1 = 0;
  unsigned long nowMove =
    millis();
  unsigned long moveInterval =
    nowMove - lastMoveTime1;
  lastMoveTime1 = nowMove;
  int accel = 1;
  if (
    moveInterval < 30) {
    accel = 20;
  } else if (
    moveInterval < 60) {
    accel = 10;
  } else if (
    moveInterval < 120) {
    accel = 4;
  } else if (
    moveInterval < 250) {
    accel = 2;
  }

  // =================================================
  // AUFWACHEN
  // =================================================
  if (
    menu == MENU_SLEEP) {
    menu = MENU_MASTER;
    oled.setContrast(
      NORMAL_CONTRAST);
    Serial.println(
      "WAKE");
    return;
  }

  // =================================================
  // SETTINGS MENU
  // =================================================
  if (
    menu == MENU_SETTINGS) {
    // Encoder-Richtung hier umgekehrt, damit "im Uhrzeigersinn" nach unten
    // durch die Liste läuft (fühlte sich sonst verkehrt an)
    settingsIndex -= delta;
    settingsIndex =
      ((settingsIndex % SETTINGS_COUNT) + SETTINGS_COUNT) % SETTINGS_COUNT;
    return;
  }

  // =================================================
  // RUHEZUSTAND-ZEIT MENU
  // =================================================
  if (
    menu == MENU_SLEEP_TIME) {
    long newTimeout =
      (long)sleepTimeoutMs + (delta * SLEEP_TIMEOUT_STEP);
    sleepTimeoutMs =
      constrain(
        newTimeout,
        SLEEP_TIMEOUT_MIN,
        SLEEP_TIMEOUT_MAX);
    sleepTimeLastChange =
      millis();
    return;
  }

  // =================================================
  // WLAN-HUB MENU (NEW/SAVED/BACK)
  // =================================================
  if (
    menu == MENU_WIFI) {
    wifiMenuIndex -= delta;
    wifiMenuIndex =
      ((wifiMenuIndex % WIFI_MENU_COUNT) + WIFI_MENU_COUNT) % WIFI_MENU_COUNT;
    return;
  }

  // =================================================
  // WLAN GESPEICHERTE PROFILE (Liste zum Verbinden, + EDIT/BACK)
  // =================================================
  if (
    menu == MENU_WIFI_SAVED_LIST) {
    int count =
      wifiProfileCount() + 2;
    wifiSavedListIndex -= delta;
    wifiSavedListIndex =
      ((wifiSavedListIndex % count) + count) % count;
    return;
  }

  // =================================================
  // WLAN PROFILE BEARBEITEN (Liste, + BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_LIST) {
    int count =
      wifiProfileCount() + 1;
    wifiEditListIndex -= delta;
    wifiEditListIndex =
      ((wifiEditListIndex % count) + count) % count;
    return;
  }

  // =================================================
  // WLAN PROFIL-UNTERMENÜ (EDIT NAME/SSID/PASSWORD, DELETE, BACK)
  // =================================================
  if (
    menu == MENU_WIFI_EDIT_ITEM) {
    wifiEditItemIndex -= delta;
    wifiEditItemIndex =
      ((wifiEditItemIndex % WIFI_EDIT_ITEM_COUNT) + WIFI_EDIT_ITEM_COUNT) % WIFI_EDIT_ITEM_COUNT;
    return;
  }

  // =================================================
  // WLAN PROFIL-FELD BEARBEITEN (Name/SSID/Passwort) — AP-Passwort teilt
  // sich denselben Zeichen-Editor
  // =================================================
  if (
    menu == MENU_WIFI_PROFILE_FIELD || menu == MENU_AP_PASS_EDIT) {
    int totalPositions =
      (int)WIFI_CHARSET_LEN + 1;
    wifiCharIndex += delta;
    wifiCharIndex =
      ((wifiCharIndex % totalPositions) + totalPositions) % totalPositions;
    return;
  }

  // =================================================
  // WEB CONFIG MENU (SERVER ON/OFF, AP PASSWORD, BACK)
  // =================================================
  if (
    menu == MENU_WEB_CONFIG) {
    webConfigMenuIndex -= delta;
    webConfigMenuIndex =
      ((webConfigMenuIndex % WEB_CONFIG_COUNT) + WEB_CONFIG_COUNT) % WEB_CONFIG_COUNT;
    return;
  }

  // =================================================
  // IP MENU
  // =================================================
  if (
    menu == MENU_IP) {
    int value =
      ipOctets[ipEditIndex] + delta;
    ipOctets[ipEditIndex] =
      constrain(
        value,
        0,
        255);
    return;
  }

  // =================================================
  // CHANNEL-TYP MENU (INPUT/MIX/MATRIX/BACK)
  // =================================================
  if (
    menu == MENU_CHANNEL_TYPE) {
    channelTypeMenuIndex -= delta;
    channelTypeMenuIndex =
      ((channelTypeMenuIndex % CHANNEL_TYPE_MENU_COUNT) + CHANNEL_TYPE_MENU_COUNT) % CHANNEL_TYPE_MENU_COUNT;
    return;
  }

  // =================================================
  // CHANNEL MENU (SLOT-LISTE FÜR GEWÄHLTEN TYP)
  // =================================================
  if (
    menu == MENU_CHANNEL) {
    int count =
      typeSlotCount(channelMenuType) + 1;
    channelMenuIndex -= delta;
    channelMenuIndex =
      ((channelMenuIndex % count) + count) % count;
    return;
  }

  // =================================================
  // CHANNEL-ITEM MENU (SLOT-UNTERMENÜ)
  // =================================================
  if (
    menu == MENU_CHANNEL_ITEM) {
    channelItemIndex -= delta;
    channelItemIndex =
      ((channelItemIndex % CHANNEL_ITEM_COUNT) + CHANNEL_ITEM_COUNT) % CHANNEL_ITEM_COUNT;
    return;
  }

  // =================================================
  // CHANNEL MENU (SLOT BEARBEITEN)
  // =================================================
  if (
    menu == MENU_CHANNEL_SLOT) {
    int maxNum =
      chTypeMax[chSlots[channelEditSlot].type];
    channelEditNum =
      constrain(
        channelEditNum + delta,
        1,
        maxNum);
    return;
  }

  // =================================================
  // STEP MENU
  // =================================================
  if (
    menu == MENU_STEP) {
    stepIndex += delta;
    stepIndex =
      constrain(
        stepIndex,
        0,
        STEP_COUNT - 1);
    encoderStep =
      stepValues[stepIndex];
    stepChanged = true;
    stepLastChange =
      millis();
    return;
  }

  // =================================================
  // MASTER (Kanal 1)
  // =================================================
  if (
    menu == MENU_MASTER) {
    if (
      level1 == -99999)
      return;
    // Beschleunigung nur unterhalb -30dB: im eigentlichen Arbeitsbereich
    // (-30dB bis +10dB) soll die eingestellte Schrittweite ohne
    // Beschleunigung gelten, nur im tiefen Dämpfungsbereich soll man
    // schnell durchspulen können.
    int effectiveAccel =
      (level1 < -3000) ? accel : 1;
    int change =
      encoderStep * 100 * effectiveAccel;
    int newValue =
      level1 + (delta * change);
    newValue =
      constrain(
        newValue,
        -13800,
        1000);
    sendDM3(
      "set MIXER:Current/" + channelPath(activeSlot1) + "/Fader/Level " + String(channelIdx(activeSlot1)) + " 0 "
      + String(newValue));
    // Lokal sofort übernehmen (optimistic update), statt auf die nächste
    // Poll-Antwort zu warten. Sonst rechnet ein schneller Folge-Tick noch
    // vom alten (evtl. mehrere Sekunden veralteten) Wert weiter und die
    // Bewegung "hängt" bei schnellem Drehen fest, statt sich zu addieren.
    level1 =
      newValue;
    levelOverrideUntil1 =
      millis() + LEVEL_OVERRIDE_TIME;
  }
}

// =================================================
// ENCODER 2 (Kanal 2) — komplett unabhängig vom Menüzustand, steuert
// immer nur seinen eigenen Kanal (activeSlot2). Nimmt nicht an der
// Menü-Navigation teil.
// =================================================
void handleEncoder2() {
  long pos =
    encoder2.getCount();
  long movement =
    pos - lastEncoder2;
  if (
    abs(movement) < 2)
    return;
  lastEncoder2 = pos;
  lastActivity =
    millis();
  int delta =
    movement / 2;
  if (
    delta == 0)
    return;

  static unsigned long lastMoveTime2 = 0;
  unsigned long nowMove =
    millis();
  unsigned long moveInterval =
    nowMove - lastMoveTime2;
  lastMoveTime2 = nowMove;
  int accel = 1;
  if (
    moveInterval < 30) {
    accel = 20;
  } else if (
    moveInterval < 60) {
    accel = 10;
  } else if (
    moveInterval < 120) {
    accel = 4;
  } else if (
    moveInterval < 250) {
    accel = 2;
  }

  // =================================================
  // AUFWACHEN
  // =================================================
  if (
    menu == MENU_SLEEP) {
    menu = MENU_MASTER;
    oled.setContrast(
      NORMAL_CONTRAST);
    Serial.println(
      "WAKE");
    return;
  }

  if (
    level2 == -99999)
    return;
  int effectiveAccel =
    (level2 < -3000) ? accel : 1;
  int change =
    encoderStep * 100 * effectiveAccel;
  int newValue =
    level2 + (delta * change);
  newValue =
    constrain(
      newValue,
      -13800,
      1000);
  sendDM3(
    "set MIXER:Current/" + channelPath(activeSlot2) + "/Fader/Level " + String(channelIdx(activeSlot2)) + " 0 "
    + String(newValue));
  level2 =
    newValue;
  levelOverrideUntil2 =
    millis() + LEVEL_OVERRIDE_TIME;
}

// =================================================
// TASTER ENCODER 1 (Kanal 1 Mute/Wechsel + gesamte Menü-Bedienung)
// =================================================
void handleButton1() {
  static bool lastState = HIGH;
  static bool pressed = false;
  static bool wakeConsumed = false;
  static unsigned long debounceTime = 0;
  bool state =
    digitalRead(
      ENC1_SW);
  if (
    state != lastState) {
    debounceTime = millis();
    lastState = state;
  }
  if (
    millis() - debounceTime < 50)
    return;
  // gedrückt
  if (
    state == LOW && !pressed) {
    pressed = true;
    buttonStart1 = millis();
    lastActivity = millis();

    // =================================================
    // AUFWACHEN
    // =================================================
    if (
      menu == MENU_SLEEP) {
      menu = MENU_MASTER;
      oled.setContrast(
        NORMAL_CONTRAST);
      wakeConsumed = true;
      Serial.println(
        "WAKE");
    }
  }
  // losgelassen
  if (
    state == HIGH && pressed) {
    pressed = false;
    unsigned long pressTime =
      millis()
      - buttonStart1;

    if (
      wakeConsumed) {
      wakeConsumed = false;
      return;
    }

    // =================================================
    // LANGER DRUCK
    // =================================================
    if (
      pressTime >= LONG_PRESS_TIME) {
      if (
        menu == MENU_MASTER) {
        // Kanal wechseln (mit Wraparound, überspringt den von Encoder 2
        // belegten Kanal) — öffnet NICHT mehr SETTINGS, das übernimmt
        // der eigene Menü-Taster (GPIO0)
        int next =
          nextEnabledSlotWrapExcl(
            activeSlot1,activeSlot2);
        if (
          next != activeSlot1) {
          activeSlot1 = next;
          resetChannelState(
            1);
          Serial.println(
            "VIEW CHANNEL1 " + String(next));
        }
      } else if (
        menu == MENU_SETTINGS) {
        // Listen-Menü: langer Druck geht immer eine Ebene zurück,
        // Auswählen passiert jetzt per kurzem Druck (siehe unten)
        menu = MENU_MASTER;
        buttonStart1 = 0;
        Serial.println(
          "RETURN MASTER");
      } else if (
        menu == MENU_CHANNEL_TYPE) {
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_CHANNEL) {
        menu = MENU_CHANNEL_TYPE;
        Serial.println(
          "RETURN CHANNEL TYPE");
      } else if (
        menu == MENU_CHANNEL_ITEM) {
        menu = MENU_CHANNEL;
        Serial.println(
          "RETURN CHANNEL");
      } else if (
        menu == MENU_CHANNEL_SLOT) {
        saveChannelSlotEdit();
        menu = MENU_CHANNEL_ITEM;
        Serial.println(
          "RETURN CHANNEL ITEM");
      } else if (
        menu == MENU_STEP) {
        if (
          stepChanged) {
          saveConfig();
          stepChanged = false;
        }
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_IP) {
        saveIPConfig();
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_SLEEP_TIME) {
        saveSleepTimeout();
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_WIFI) {
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_WEB_CONFIG) {
        menu = MENU_SETTINGS;
        Serial.println(
          "RETURN SETTINGS");
      } else if (
        menu == MENU_AP_PASS_EDIT) {
        if (
          wifiEditLength == 0) {
          Serial.println(
            "AP PASSWORD EMPTY, KEEPING OLD VALUE");
        } else if (
          wifiEditLength < 8) {
          Serial.println(
            "AP PASSWORD TOO SHORT (MIN 8), KEEPING OLD VALUE");
        } else {
          strncpy(
            apPassword,wifiEditBuffer,WIFI_MAX_LEN);
          apPassword[WIFI_MAX_LEN] = '\0';
          saveWebConfig();
          if (
            apFallbackActive) {
            // AP sofort mit neuem Passwort neu aufbauen, sonst gilt das
            // alte Passwort bis zum nächsten Neustart des Fallbacks
            WiFi.softAPdisconnect(
              true);
            WiFi.softAP(
              apSSID,apPassword);
          }
          Serial.println(
            "AP PASSWORD SAVED");
        }
        menu = MENU_WEB_CONFIG;
      } else if (
        menu == MENU_WIFI_SAVED_LIST) {
        menu = MENU_WIFI;
        Serial.println(
          "RETURN WIFI MENU");
      } else if (
        menu == MENU_WIFI_EDIT_LIST) {
        menu = MENU_WIFI_SAVED_LIST;
        Serial.println(
          "RETURN SAVED LIST");
      } else if (
        menu == MENU_WIFI_EDIT_ITEM) {
        menu = MENU_WIFI_EDIT_LIST;
        Serial.println(
          "RETURN EDIT LIST");
      } else if (
        menu == MENU_WIFI_PROFILE_FIELD) {
        if (
          wifiEditIsNew) {
          if (
            wifiEditField == WEF_NAME) {
            // Name darf leer bleiben (Anzeige fällt dann auf die SSID
            // zurück) — trotzdem speichern und mit SSID weitermachen.
            saveWifiProfileField();
            loadWifiProfileFieldEdit(
              wifiEditProfileIndex,WEF_SSID,true);
            Serial.println(
              "PROFILE NAME SET, EDIT SSID");
          } else if (
            wifiEditField == WEF_SSID) {
            if (
              wifiEditLength == 0) {
              // Ohne SSID kein funktionierendes Profil -> abbrechen,
              // nichts wird gespeichert.
              Serial.println(
                "PROFILE SSID EMPTY, CANCELLED");
              menu = MENU_WIFI;
            } else {
              saveWifiProfileField();
              loadWifiProfileFieldEdit(
                wifiEditProfileIndex,WEF_PASS,true);
              Serial.println(
                "PROFILE SSID SET, EDIT PASSWORD");
            }
          } else {
            // WEF_PASS, letzter Schritt — leeres Passwort ist erlaubt
            // (offenes Netzwerk), Profil ist danach fertig und wird
            // gleich aktiviert.
            saveWifiProfileField();
            connectToWifiProfile(
              wifiEditProfileIndex);
            menu = MENU_MASTER;
            buttonStart1 = 0;
            Serial.println(
              "PROFILE CREATED, CONNECTING");
          }
        } else {
          // Einzelnes Feld eines bestehenden Profils bearbeiten
          if (
            wifiEditLength > 0) {
            saveWifiProfileField();
            Serial.println(
              "PROFILE FIELD SAVED");
          } else {
            // Leere Eingabe = Abbrechen, alten Wert NICHT überschreiben
            Serial.println(
              "PROFILE FIELD EMPTY, KEEPING OLD VALUE");
          }
          menu = MENU_WIFI_EDIT_ITEM;
        }
      }
    }

    // =================================================
    // KURZER DRUCK
    // =================================================
    else {
      if (
        menu == MENU_MASTER) {
        mute1 =
          !mute1;
        muteOverrideUntil1 =
          millis() + MUTE_OVERRIDE_TIME;
        sendDM3(
          "set MIXER:Current/" + channelPath(activeSlot1) + "/Fader/On " + String(channelIdx(activeSlot1)) + " 0 "
          + String(
            mute1 ? 0 : 1));
        Serial.println(
          "MUTE1");
      } else if (
        menu == MENU_SETTINGS) {
        // Listen-Menü: kurzer Druck wählt den markierten Eintrag
        if (
          settingsIndex == 0) {
          channelTypeMenuIndex = 0;
          menu = MENU_CHANNEL_TYPE;
          Serial.println(
            "OPEN CHANNEL TYPE");
        } else if (
          settingsIndex == 1) {
          menu = MENU_STEP;
          stepChanged = false;
          stepLastChange =
            millis();
          Serial.println(
            "OPEN STEP");
        } else if (
          settingsIndex == 2) {
          menu = MENU_SLEEP_TIME;
          sleepTimeLastChange =
            millis();
          Serial.println(
            "OPEN SLEEP TIME");
        } else if (
          settingsIndex == 3) {
          loadIPEdit();
          menu = MENU_IP;
          Serial.println(
            "OPEN IP CONFIG");
        } else if (
          settingsIndex == 4) {
          wifiMenuIndex = 0;
          menu = MENU_WIFI;
          Serial.println(
            "OPEN WIFI MENU");
        } else if (
          settingsIndex == 5) {
          webConfigMenuIndex = 0;
          menu = MENU_WEB_CONFIG;
          Serial.println(
            "OPEN WEB CONFIG");
        } else {
          menu = MENU_MASTER;
          buttonStart1 = 0;
          Serial.println(
            "RETURN MASTER");
        }
      } else if (
        menu == MENU_WEB_CONFIG) {
        if (
          webConfigMenuIndex == 0) {
          webConfigEnabled =
            !webConfigEnabled;
          saveWebConfig();
          Serial.println(
            webConfigEnabled ? "WEB SERVER ENABLED" : "WEB SERVER DISABLED");
        } else if (
          webConfigMenuIndex == 1) {
          loadApPassEdit();
          menu = MENU_AP_PASS_EDIT;
          Serial.println(
            "EDIT AP PASSWORD");
        } else {
          menu = MENU_SETTINGS;
          Serial.println(
            "RETURN SETTINGS");
        }
      } else if (
        menu == MENU_WIFI) {
        if (
          wifiMenuIndex == 0) {
          // NEW: freien Profil-Slot suchen und mit dem Namen anfangen
          int freeSlot =
            findFreeWifiProfileSlot();
          if (
            freeSlot == -1) {
            Serial.println(
              "WIFI PROFILES FULL");
          } else {
            loadWifiProfileFieldEdit(
              freeSlot,WEF_NAME,true);
            menu = MENU_WIFI_PROFILE_FIELD;
            Serial.println(
              "OPEN NEW WIFI PROFILE");
          }
        } else if (
          wifiMenuIndex == 1) {
          wifiSavedListIndex = 0;
          menu = MENU_WIFI_SAVED_LIST;
          Serial.println(
            "OPEN SAVED WIFI LIST");
        } else {
          menu = MENU_SETTINGS;
          Serial.println(
            "RETURN SETTINGS");
        }
      } else if (
        menu == MENU_WIFI_SAVED_LIST) {
        int count =
          wifiProfileCount();
        if (
          wifiSavedListIndex < count) {
          int idx =
            wifiProfileIndexAtListPosition(
              wifiSavedListIndex);
          connectToWifiProfile(
            idx);
          menu = MENU_MASTER;
          buttonStart1 = 0;
          Serial.println(
            "CONNECTING TO SAVED PROFILE");
        } else if (
          wifiSavedListIndex == count) {
          wifiEditListIndex = 0;
          menu = MENU_WIFI_EDIT_LIST;
          Serial.println(
            "OPEN EDIT LIST");
        } else {
          menu = MENU_WIFI;
          Serial.println(
            "RETURN WIFI MENU");
        }
      } else if (
        menu == MENU_WIFI_EDIT_LIST) {
        int count =
          wifiProfileCount();
        if (
          wifiEditListIndex < count) {
          wifiEditProfileIndex =
            wifiProfileIndexAtListPosition(
              wifiEditListIndex);
          wifiEditItemIndex = 0;
          menu = MENU_WIFI_EDIT_ITEM;
          Serial.println(
            "OPEN PROFILE EDIT ITEM");
        } else {
          menu = MENU_WIFI_SAVED_LIST;
          Serial.println(
            "RETURN SAVED LIST");
        }
      } else if (
        menu == MENU_WIFI_EDIT_ITEM) {
        if (
          wifiEditItemIndex == 0) {
          loadWifiProfileFieldEdit(
            wifiEditProfileIndex,WEF_NAME,false);
          menu = MENU_WIFI_PROFILE_FIELD;
          Serial.println(
            "EDIT PROFILE NAME");
        } else if (
          wifiEditItemIndex == 1) {
          loadWifiProfileFieldEdit(
            wifiEditProfileIndex,WEF_SSID,false);
          menu = MENU_WIFI_PROFILE_FIELD;
          Serial.println(
            "EDIT PROFILE SSID");
        } else if (
          wifiEditItemIndex == 2) {
          loadWifiProfileFieldEdit(
            wifiEditProfileIndex,WEF_PASS,false);
          menu = MENU_WIFI_PROFILE_FIELD;
          Serial.println(
            "EDIT PROFILE PASSWORD");
        } else if (
          wifiEditItemIndex == 3) {
          deleteWifiProfile(
            wifiEditProfileIndex);
          menu = MENU_WIFI_EDIT_LIST;
          if (
            wifiEditListIndex > 0)
            wifiEditListIndex--;
          Serial.println(
            "RETURN EDIT LIST");
        } else {
          menu = MENU_WIFI_EDIT_LIST;
          Serial.println(
            "RETURN EDIT LIST");
        }
      } else if (
        menu == MENU_CHANNEL_TYPE) {
        if (
          channelTypeMenuIndex >= 0 && channelTypeMenuIndex <= 2) {
          channelMenuType = channelTypeMenuIndex;
          channelMenuIndex = 0;
          menu = MENU_CHANNEL;
          Serial.println(
            "OPEN CHANNEL LIST");
        } else {
          menu = MENU_SETTINGS;
          Serial.println(
            "RETURN SETTINGS");
        }
      } else if (
        menu == MENU_CHANNEL) {
        int count =
          typeSlotCount(channelMenuType);
        if (
          channelMenuIndex >= 0 && channelMenuIndex < count) {
          channelItemSlot =
            typeSlotBase(channelMenuType) + channelMenuIndex;
          channelItemIndex = 0;
          menu = MENU_CHANNEL_ITEM;
          Serial.println(
            "OPEN CHANNEL ITEM");
        } else {
          menu = MENU_CHANNEL_TYPE;
          Serial.println(
            "RETURN CHANNEL TYPE");
        }
      } else if (
        menu == MENU_CHANNEL_ITEM) {
        if (
          channelItemIndex == 0) {
          // AKTIVIEREN/DEAKTIVIEREN, bleibt im Untermenü
          toggleChannelEnabled();
        } else if (
          channelItemIndex == 1) {
          // SET CHANNEL
          loadChannelSlotEdit(
            channelItemSlot);
          menu = MENU_CHANNEL_SLOT;
          Serial.println(
            "OPEN CHANNEL SLOT");
        } else {
          menu = MENU_CHANNEL;
          Serial.println(
            "RETURN CHANNEL");
        }
      } else if (
        menu == MENU_IP) {
        ipEditIndex =
          (ipEditIndex + 1) % 4;
        Serial.println(
          "NEXT IP BLOCK");
      } else if (
        menu == MENU_WIFI_PROFILE_FIELD || menu == MENU_AP_PASS_EDIT) {
        int fieldMaxLen =
          (wifiEditField == WEF_NAME) ? WIFI_NAME_MAX_LEN : WIFI_MAX_LEN;
        if (
          wifiCharIndex == (int)WIFI_CHARSET_LEN) {
          // END gewählt: Eingabe hier abschließen
          wifiEditLength = wifiEditCursor;
          wifiEditBuffer[wifiEditLength] = '\0';
        } else {
          wifiEditBuffer[wifiEditCursor] =
            wifiCharset[wifiCharIndex];
          if (
            wifiEditCursor == wifiEditLength && wifiEditLength < fieldMaxLen) {
            wifiEditLength++;
          }
          wifiEditBuffer[wifiEditLength] = '\0';
          wifiEditCursor++;
          if (
            wifiEditCursor > wifiEditLength)
            wifiEditCursor = wifiEditLength;
          wifiCharIndex =
            wifiCharsetIndexFor(
              wifiEditCursor < wifiEditLength ? wifiEditBuffer[wifiEditCursor] : ' ');
        }
        Serial.println(
          "WIFI CHAR SET");
      }
    }
  }
}

// =================================================
// TASTER ENCODER 2 (Kanal 2 Mute/Wechsel) — unabhängig vom Menüzustand,
// nimmt nicht an der Menü-Navigation teil
// =================================================
void handleButton2() {
  static bool lastState = HIGH;
  static bool pressed = false;
  static bool wakeConsumed = false;
  static unsigned long debounceTime = 0;
  bool state =
    digitalRead(
      ENC2_SW);
  if (
    state != lastState) {
    debounceTime = millis();
    lastState = state;
  }
  if (
    millis() - debounceTime < 50)
    return;
  // gedrückt
  if (
    state == LOW && !pressed) {
    pressed = true;
    buttonStart2 = millis();
    lastActivity = millis();

    if (
      menu == MENU_SLEEP) {
      menu = MENU_MASTER;
      oled.setContrast(
        NORMAL_CONTRAST);
      wakeConsumed = true;
      Serial.println(
        "WAKE");
    }
  }
  // losgelassen
  if (
    state == HIGH && pressed) {
    pressed = false;
    unsigned long pressTime =
      millis()
      - buttonStart2;

    if (
      wakeConsumed) {
      wakeConsumed = false;
      return;
    }

    if (
      pressTime >= LONG_PRESS_TIME) {
      // Langer Druck: Kanal 2 wechseln (mit Wraparound, überspringt den
      // von Encoder 1 belegten Kanal)
      int next =
        nextEnabledSlotWrapExcl(
          activeSlot2,activeSlot1);
      if (
        next != activeSlot2) {
        activeSlot2 = next;
        resetChannelState(
          2);
        Serial.println(
          "VIEW CHANNEL2 " + String(next));
      }
    } else {
      // Kurzer Druck: Mute Kanal 2
      mute2 =
        !mute2;
      muteOverrideUntil2 =
        millis() + MUTE_OVERRIDE_TIME;
      sendDM3(
        "set MIXER:Current/" + channelPath(activeSlot2) + "/Fader/On " + String(channelIdx(activeSlot2)) + " 0 "
        + String(
          mute2 ? 0 : 1));
      Serial.println(
        "MUTE2");
    }
  }
}

// =================================================
// MENÜ-TASTER (GPIO0) — einziger Weg ins SETTINGS-Menü, langer Druck
// springt aus JEDEM Untermenü sofort zurück zu MASTER (Notausstieg)
// =================================================
void handleMenuButton() {
  static bool lastState = HIGH;
  static bool pressed = false;
  static bool wakeConsumed = false;
  static unsigned long debounceTime = 0;
  static unsigned long buttonStartMenu = 0;
  bool state =
    digitalRead(
      MENU_BTN);
  if (
    state != lastState) {
    debounceTime = millis();
    lastState = state;
  }
  if (
    millis() - debounceTime < 50)
    return;
  if (
    state == LOW && !pressed) {
    pressed = true;
    buttonStartMenu = millis();
    lastActivity = millis();

    if (
      menu == MENU_SLEEP) {
      menu = MENU_MASTER;
      oled.setContrast(
        NORMAL_CONTRAST);
      wakeConsumed = true;
      Serial.println(
        "WAKE");
    }
  }
  if (
    state == HIGH && pressed) {
    pressed = false;
    unsigned long pressTime =
      millis()
      - buttonStartMenu;

    if (
      wakeConsumed) {
      // Aufwecken zählt nicht zusätzlich als Tastendruck fürs Menü,
      // sonst würde direkt nach dem Aufwecken auch noch SETTINGS
      // aufgehen bzw. der Notausstieg ausgelöst.
      wakeConsumed = false;
      return;
    }

    if (
      pressTime >= LONG_PRESS_TIME) {
      // Notausstieg: aus jedem Untermenü sofort zurück zu MASTER
      if (
        menu != MENU_MASTER) {
        menu = MENU_MASTER;
        Serial.println(
          "MENU ESCAPE, RETURN MASTER");
      }
    } else if (
      menu == MENU_MASTER) {
      menu = MENU_SETTINGS;
      settingsIndex = 0;
      Serial.println(
        "OPEN SETTINGS");
    }
  }
}
