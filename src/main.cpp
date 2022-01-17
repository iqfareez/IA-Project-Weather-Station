#define BLYNK_TEMPLATE_ID "TMPL5qPt6-2S"
#define BLYNK_DEVICE_NAME "Weather Station"
#define BLYNK_AUTH_TOKEN "BchRthK9R3FKLfIUCeLMZmgPJfz2z9Dc"

#define BLYNK_PRINT Serial

#include <Adafruit_BMP085.h>
#include <SPI.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <SH1106Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>

#define DHTPIN 5      // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11 // DHT 11
#define LED_PIN 2

Adafruit_BMP085 bmp;
BH1750 lightMeter(0x23);
DHT_Unified dht(DHTPIN, DHTTYPE);
SH1106Wire display(0x3c, SDA, SCL); // ADDRESS, SDA, SCL

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = "YES4";
char pass[] = "haha2021a";
char auth[] = BLYNK_AUTH_TOKEN;

BlynkTimer timer;
HTTPClient httpClient;

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 8 * 60 * 60;
const int daylightOffset_sec = 3600;

float temperature;
int pressure;
float altitude;
int32_t sealevelPressure;
float reallAltitude;
float lux;
float dhtTemperature;
int relativeHumidity;

int lastSendNotif = 0;

void updateSensors()
{
  Serial.print("Temperature = ");
  temperature = bmp.readTemperature();
  Serial.print(temperature);
  Serial.println(" *C");

  Serial.print("Pressure = ");
  pressure = bmp.readPressure();
  Serial.print(pressure);
  Serial.println(" Pa");

  // Calculate altitude assuming 'standard' barometric
  // pressure of 1013.25 millibar = 101325 Pascal
  Serial.print("Altitude = ");
  altitude = bmp.readAltitude();
  Serial.print(altitude);
  Serial.println(" meters");

  Serial.print("Pressure at sealevel (calculated) = ");
  sealevelPressure = bmp.readSealevelPressure();
  Serial.print(sealevelPressure);
  Serial.println(" Pa");

  // you can get a more precise measurement of altitude
  // if you know the current sea level pressure which will
  // vary with weather and such. If it is 1015 millibars
  // that is equal to 101500 Pascals.
  Serial.print("Real altitude = ");
  reallAltitude = bmp.readAltitude(101500);
  Serial.print(reallAltitude);
  Serial.println(" meters");

  // lux meter
  if (lightMeter.measurementReady())
  {
    lux = lightMeter.readLightLevel();
    Serial.print("Light: ");
    Serial.print(lux);
    Serial.println(" lx");
  }

  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature))
  {
    Serial.println(F("Error reading temperature!"));
  }
  else
  {
    Serial.print(F("Temperature: "));
    dhtTemperature = event.temperature;
    Serial.print(dhtTemperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity))
  {
    Serial.println(F("Error reading humidity!"));
  }
  else
  {
    Serial.print(F("Humidity: "));
    relativeHumidity = event.relative_humidity;
    Serial.print(relativeHumidity);
    Serial.println(F("%"));
  }

  // get time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }
  char timeStringBuff[50]; //50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%a, %I:%M %p", &timeinfo);

  // display all sensors value to the OLED display
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.normalDisplay();
  display.drawString(0, 0, "Temperature: " + String(temperature) + " *C");
  display.drawString(0, 10, "Pressure: " + String(pressure) + " Pa");
  // display.drawString(0, 20, "Altitude: " + String(altitude) + " m");
  display.drawString(0, 20, "Light: " + String(lux) + " lx");
  display.drawString(0, 30, "Humidity: " + String(relativeHumidity) + " %");
  display.drawString(0, 50, timeStringBuff);
  display.display();
  Serial.println();

  // Upload sensors value to Blynk
  Blynk.virtualWrite(V10, dhtTemperature);
  Blynk.virtualWrite(V12, lux);
  Blynk.virtualWrite(V11, relativeHumidity);
  Blynk.virtualWrite(V8, altitude);
  Blynk.virtualWrite(V14, pressure);

  // Check if raining is coming by the light intensity
  if ((lux < 10 && lux > 0) && (timeinfo.tm_hour > 8 && timeinfo.tm_hour < 18))
  {
    Serial.println("Nak hujan.");

    // To avoid spamming the user, tele notification
    // will be send once an hour only
    if ((timeinfo.tm_hour - lastSendNotif) != 0)
    {
      Serial.println("Sending tele notif now");
      lastSendNotif = timeinfo.tm_hour;
      int statusCode = httpClient.GET();
      Serial.println(statusCode); // expect 200
    }
  }
}

BLYNK_CONNECTED()
{
  Serial.println("Connected yuhhuu");
  // On-board LED will turn on if Blynk successfully connected
  digitalWrite(LED_PIN, HIGH);
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  // Initialize OLED display
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(0, 50, "Initializing...");
  display.display();

  // iniitlize blynk
  Blynk.begin(auth, ssid, pass);

  //init and get the time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  Wire.begin();
  // Start BMP180 Pressure sensor
  if (!bmp.begin())
  {
    Serial.println("Could not find a valid BMP085 sensor, check wiring!");
    while (1)
    {
    }
  }
  Wire.begin();
  //Start BH1750 Lux sensor
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE))
  {
    Serial.println(F("BH1750 Advanced begin"));
  }
  else
  {
    Serial.println(F("Error initialising BH1750"));
  }

  // Initialize DHT11 Sensor
  dht.begin();
  Serial.println(F("DHT11 Unified Sensor ON"));

  // Display weather station text
  display.clear();
  display.setFont(ArialMT_Plain_24);
  display.invertDisplay();
  display.drawString(0, 5, " Weather");
  display.drawString(0, 30, " Station");
  display.display();
  display.clear();

  httpClient.begin("https://api.telegram.org/bot2098150036:AAFKH9JNTefiljqUX59oS47S487jin2VQWc/sendMessage?chat_id=530851302&text=Rain\%20is\%20coming.\%20Pick\%20up\%20the\%20clothes\%20now.");

  // Sensor updating and cloud synchronize
  // will occur every 1 sec
  // Avoid using delay() function!
  timer.setInterval(1000L, updateSensors);
}

void loop()
{
  Blynk.run();
  timer.run();
}