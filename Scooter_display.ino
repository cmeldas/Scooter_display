

// CAN
#include <CAN.h>
// RTC
#include <Wire.h>
#include "RTClib.h"
RTC_DS3231 rtc;

// Dalas
#include <OneWire.h>
#include <DallasTemperature.h>
#define ONE_WIRE_BUS 6
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress tempDeviceAddress;
int resolution = 12;
unsigned long lastTempRequest = 0;
int delayInMillis = 0;
int temperature = 0;

// OLED
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#define FONT_1 u8g2_font_fub17_tf
#define FONT_2 FONT_1
#define FONT_3 FONT_1
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE);

// Read from controler

int32_t trip;
int16_t phase_curr, v_speed, power, limit, error;
float bat_volt, bat_curr;
int8_t soc, soh, pmap, app_mode;

char *temp = malloc(sizeof(char) * 30);

void setup()
{
  Serial.begin(9600);

  sensors.begin();
  sensors.getAddress(tempDeviceAddress, 0);
  sensors.setResolution(tempDeviceAddress, resolution);
  sensors.setWaitForConversion(false);
  sensors.requestTemperatures();
  delayInMillis = 750 / (1 << (12 - resolution));
  lastTempRequest = millis();

  while (!Serial)
    ;
  Serial.println("Scooter Display begin");
  Serial.println(__DATE__);
  u8g2.begin();
  do
  {
    u8g2.setFont(FONT_1);
    u8g2.drawStr(0, 17, "Starting");
    u8g2.drawStr(0, 55, __DATE__);

  } while (u8g2.nextPage());

  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    u8g2.firstPage();
    do
    {
      u8g2.setFont(FONT_1);
      u8g2.drawStr(0, 24, "Couldn't find RTC");
    } while (u8g2.nextPage());
    while (1)
      ;
  }
  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, lets set the time!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 1, 0)); //dorovná zpoždění při nahrávání
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
  DateTime now = rtc.now();
  Serial.println("Actual time");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);

  // start the CAN bus at 500 kbps
  if (!CAN.begin(1000E3))
  {
    Serial.println("Starting CAN failed!");
    u8g2.firstPage();
    do
    {
      u8g2.setFont(FONT_1);
      u8g2.drawStr(0, 24, "Starting CAN failed!");
    } while (u8g2.nextPage());
    while (1)
      ;
  }
  //wait for data
  for (int i = 0; i < 100; i++)
  {
    can_read();
    get_time();
    delay(20);
  }
}

void can_read()
{

  // try to parse packet
  static unsigned long last_receive_T = 0;
  int packetSize = CAN.parsePacket();
  int battery_volt, battery_curr;

  if (last_receive_T + 600 < millis())
  {
    // reset to zero
    trip = 0;
    phase_curr = 0;
    v_speed = 0;
    power = 0;
    limit = 0;
    //error = 0; // hold error
    bat_volt = 0;
    bat_curr = 0;
    soc = 0;
    soh = 0;
    pmap = 0;
    app_mode = 0;
  }

  if (packetSize == 8)
  {
    // LYNX send only 8 byte packed size
    int canid = CAN.packetId();
    last_receive_T = millis();
    uint8_t read_buff[8];
    for (int i = 0; i < 8; i++)
    {
      read_buff[i] = CAN.read();
    }

    switch (canid)
    {
    case 0x600:
      app_mode = read_buff[1];
      pmap = read_buff[2];
      limit = read_buff[4];
      limit = limit | read_buff[5] << 8;
      error = read_buff[6];
      error = error | read_buff[7] << 8;
      break;

    case 0x610:
      phase_curr = read_buff[0];
      phase_curr = phase_curr | read_buff[1] << 8;
      v_speed = read_buff[4];
      v_speed = v_speed | read_buff[5] << 8;
      power = read_buff[6];
      power = power | read_buff[7] << 8;
      break;

    case 0x618:
      battery_volt = read_buff[4];
      battery_volt = battery_volt | read_buff[5] << 8;
      battery_curr = read_buff[6];
      battery_curr = battery_curr | read_buff[7] << 8;
      bat_curr = battery_curr;
      bat_curr = bat_curr / 100;
      bat_volt = battery_volt;
      bat_volt = bat_volt / 100;
      soc = read_buff[2];
      soh = read_buff[3];
      break;

    case 0x620:
      trip = read_buff[0];
      trip = trip | read_buff[1] << 8;
      trip = trip | read_buff[2] << 16;
      trip = trip | read_buff[3] << 24;
      break;
    }
  }
}

char *get_time()
{
  static unsigned long lastT = 0;
  static char *str = malloc(sizeof(char) * 6);
  if (millis() > lastT + 2000)
  {

    DateTime now = rtc.now();
    sprintf(str, "%02u:%02u", now.hour(), now.minute());
    Serial.println(str);
    lastT = millis();
  }
  return str;
}

void loop()
{
  can_read();

  switch (app_mode)
  {
  case 70:
    // Charging mode
    u8g2.firstPage();
    do
    {
      u8g2.setFont(FONT_1);
      u8g2.setCursor(65, 17);
      u8g2.println(get_time());
      sprintf(temp, "%3u%%", soc);
      u8g2.setCursor(0, 17);
      u8g2.println(temp);
      sprintf(temp, "%3uW", abs(power));
      u8g2.setCursor(0, 40);
      u8g2.println(temp);
      sprintf(temp, "%3uV", int(bat_volt));
      u8g2.setCursor(75, 40);
      u8g2.println(temp);
      if (!error || error == 65534)
      {
        sprintf(temp, "Lim: %4u", limit);
      }
      else
      {
        sprintf(temp, "Err: %4u", error);
      }

      u8g2.setCursor(0, 64);
      u8g2.println(temp);
    } while (u8g2.nextPage());
    break;

  default:
    if (!error)
    {
      u8g2.firstPage();
      do
      {
        u8g2.setFont(FONT_3);
        sprintf(temp, "%3u%%", soc);
        u8g2.drawStr(0, 17, temp);
        u8g2.setFont(FONT_2);
        u8g2.drawStr(65, 17, get_time());
        sprintf(temp, "%d%cC", temperature, char(176));
        u8g2.drawStr(80, 50, temp);
        u8g2.setFont(FONT_1);
        sprintf(temp, " %1u", pmap);
        u8g2.drawStr(0, 50, temp);
      } while (u8g2.nextPage());
    }
    else    // some error happends, print it
    {
      u8g2.firstPage();
    do
    {
      u8g2.setFont(FONT_3);
      sprintf(temp, "%3u%%", soc);
      u8g2.drawStr(0, 17, temp);
      u8g2.setFont(FONT_2);
      u8g2.drawStr(65, 17, get_time());
    //  sprintf(temp, "%d%cC", temperature, char(176));
     // u8g2.drawStr(80, 50, temp);
      u8g2.setFont(FONT_1);
      sprintf(temp, "Err: %4u", error);
      u8g2.drawStr(0, 50, temp);
    } while (u8g2.nextPage());
      
    }
  }

  // temp read

  if (millis() - lastTempRequest >= delayInMillis) // waited long enough??
  {
    Serial.print(" Temperature: ");
    temperature = sensors.getTempCByIndex(0);
    Serial.println(temperature);
    sensors.requestTemperatures();
    delayInMillis = 750 / (1 << (12 - resolution));
    lastTempRequest = millis();
  }

  // Serial debug print:
  static unsigned long lastT = 0;
  int interval = 1000;
  if (millis() > lastT + interval)
  {

    Serial.println("Volt:");
    Serial.println(bat_volt);
    Serial.println("Curr:");
    Serial.println(bat_curr);
    Serial.println("SOC:");
    Serial.println(soc);
    Serial.println("Trip:");
    Serial.println(trip);
    Serial.println("Pmap:");
    Serial.println(pmap);
    Serial.println("Power");
    Serial.println(power);
    Serial.println();

    lastT = millis();
  }
}
