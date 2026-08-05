// =================================================
// AKKU LESEN
// =================================================
void readBattery() {
  pinMode(
    VBAT_CTRL,OUTPUT);
  digitalWrite(
    VBAT_CTRL,HIGH);
  delay(
    5);
  long sum = 0;
  for (
    int i = 0; i < VBAT_SAMPLES; i++) {
    sum += analogRead(VBAT_ADC);
  }
  int raw =
    sum / VBAT_SAMPLES;
  pinMode(
    VBAT_CTRL,INPUT);

  batteryConnected =
    (raw > VBAT_MIN_RAW);

  if (
    !batteryConnected) {
    batteryVoltage = 0;
    batteryPercent = 0;
    Serial.println(
      "BATTERY NOT CONNECTED");
    return;
  }

  batteryVoltage =
    raw / VBAT_DIVISOR;
  int percent =
    (int)((batteryVoltage - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100.0);
  batteryPercent =
    constrain(
      percent,0,100);

  Serial.print(
    "BATTERY raw=");
  Serial.print(
    raw);
  Serial.print(
    " volt=");
  Serial.print(
    batteryVoltage);
  Serial.print(
    " percent=");
  Serial.println(
    batteryPercent);
}

// =================================================
// AKKU SYMBOL
// =================================================
void drawBattery(
  int x,int y,int percent) {
  oled.drawFrame(
    x,y,16,8);
  oled.drawBox(
    x + 16,y + 2,2,4);
  int fillWidth =
    map(
      percent,0,100,0,14);
  if (
    fillWidth > 0) {
    oled.drawBox(
      x + 1,y + 1,fillWidth,6);
  }
}

// =================================================
// AKKU NICHT VERBUNDEN
// =================================================
void drawBatteryEmpty(
  int x,int y) {
  oled.drawFrame(
    x,y,16,8);
  oled.drawBox(
    x + 16,y + 2,2,4);
  oled.drawLine(
    x,y,x + 16,y + 8);
  oled.drawLine(
    x,y + 8,x + 16,y);
}

// =================================================
// AKKU ANZEIGE (KOMBINIERT)
// =================================================
void drawBatteryIndicator(
  int x,int y) {
  if (
    !batteryConnected) {
    drawBatteryEmpty(
      x,y);
    return;
  }
  drawBattery(
    x,y,batteryPercent);
}
