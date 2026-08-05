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
// AKKU SYMBOL (parametrisiert auf ein Display, da wir jetzt zwei haben)
// =================================================
void drawBattery(
  U8G2 &d,int x,int y,int percent) {
  d.drawFrame(
    x,y,10,6);
  d.drawBox(
    x + 10,y + 1,1,4);
  int fillWidth =
    map(
      percent,0,100,0,8);
  if (
    fillWidth > 0) {
    d.drawBox(
      x + 1,y + 1,fillWidth,4);
  }
}

// =================================================
// AKKU NICHT VERBUNDEN
// =================================================
void drawBatteryEmpty(
  U8G2 &d,int x,int y) {
  d.drawFrame(
    x,y,10,6);
  d.drawBox(
    x + 10,y + 1,1,4);
  d.drawLine(
    x,y,x + 10,y + 6);
  d.drawLine(
    x,y + 6,x + 10,y);
}

// =================================================
// AKKU ANZEIGE (KOMBINIERT)
// =================================================
void drawBatteryIndicator(
  U8G2 &d,int x,int y) {
  if (
    !batteryConnected) {
    drawBatteryEmpty(
      d,x,y);
    return;
  }
  drawBattery(
    d,x,y,batteryPercent);
}
