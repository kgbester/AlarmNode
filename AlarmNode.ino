// AlarmNode - to be used with AlarmHub and MQTT services
// Copyright Kurt Bester 2017

#include <avr/wdt.h>
#include <UnoWiFiDevEd.h>
#include <dht11.h>
#include <elapsedMillis.h>

dht11 DHT;
#define DHT11_PIN 3

elapsedMillis timer0;
#define interval 900000

int redLED = 13;    // Motion detected LED
int blueLED = 12;   // Sensing LED
int PIR = 2;        // PIR sensor
int pirState = LOW; // we start, assuming no motion detected
int val = 0;        // variable for reading the pin status

// MQTT definitions
#define CONNECTOR "mqtt"
#define TOPIC_ZONETRIGGER_PUB "home/alarm/zonetrigger"
#define TOPIC_ZONE_PUB "home/alarm/state/zone1"
#define TOPIC_ZONE_SUB "home/alarm/switch/zone1"
#define TOPIC_ALARM_PUB "home/alarm/state"
#define TOPIC_ALARM_SUB "home/alarm/switch"
#define TOPIC_TEMPERATURE_PUB "home/temperature/zone1"
#define TOPIC_HUMIDITY_PUB "home/humidity/zone1"
#define SWITCH_ON "ON"
#define SWITCH_OFF "OFF"

boolean alarm = false;
boolean zone = false;

int temperature = 0;
int humidity = 0;

void setup()
{
  Serial.begin(9600);

  // Set the PIN modes
  pinMode(redLED, OUTPUT);
  pinMode(blueLED, OUTPUT);
  pinMode(PIR, INPUT);

  // Enable the MQTT client
  Ciao.begin();

  // Initialise the timer
  timer0 = 0;

  // Enable the watchdog
  wdt_enable(WDTO_2S);
}

void loop()
{
  if (timer0 > interval)
  {
    timer0 -= interval; //reset the timer
    CheckDHT();
  }

  // Check alarm subscription
  CiaoData alarmData = Ciao.read(CONNECTOR, TOPIC_ALARM_SUB);
  if (!alarmData.isEmpty())
  {
    Serial.println(F("TOPIC_ALARM_SUB requested"));
    char *value = alarmData.get(2);
    if (strcmp(value, SWITCH_ON) == 0)
    {
      Serial.println(F("MQTT alarm on"));
      alarm = true;
      zone = true;
      Serial.println(F("LED on"));
      digitalWrite(blueLED, HIGH); // turn LED ON
      mqttSend(TOPIC_ALARM_PUB, SWITCH_ON);
      mqttSend(TOPIC_ZONE_PUB, SWITCH_ON);
      mqttSend(TOPIC_ZONETRIGGER_PUB, "");
    }
    else
    {
      Serial.println(F("MQTT alarm off"));
      alarm = false;
      zone = false;
      Serial.println(F("LED off"));
      digitalWrite(blueLED, LOW); // turn LED OFF
      digitalWrite(redLED, LOW);  // turn LED OFF
      mqttSend(TOPIC_ALARM_PUB, SWITCH_OFF);
      mqttSend(TOPIC_ZONE_PUB, SWITCH_OFF);
    }
  }

  // Check the zone subscription
  CiaoData zoneData = Ciao.read(CONNECTOR, TOPIC_ZONE_SUB);
  if (!zoneData.isEmpty())
  {
    Serial.println(F("TOPIC_ZONE_SUB requested"));
    char *value = zoneData.get(2);
    if (strcmp(value, SWITCH_ON) == 0)
    {
      if (zone == false)
      {
        Serial.println(F("MQTT zone on"));
        zone = true;
        Serial.println(F("LED on"));
        digitalWrite(blueLED, HIGH); // turn LED ON
        mqttSend(TOPIC_ALARM_PUB, SWITCH_ON);
        mqttSend(TOPIC_ZONE_PUB, SWITCH_ON);
      }
    }
    else if (zone == true)
    {
      Serial.println(F("MQTT zone off"));
      zone = false;
      Serial.println(F("LED off"));
      digitalWrite(blueLED, LOW); // turn LED OFF
      digitalWrite(redLED, LOW);  // turn LED OFF
      mqttSend(TOPIC_ZONE_PUB, SWITCH_OFF);
    }
  }

  // Check PIR sensor
  if (zone)
  {
    CheckPIR();
  }

  // Send the climate data
  if (temperature != 0 && humidity != 0)
  {
    char buffer[4];

    itoa(temperature, buffer, 10);
    Serial.print(F("Temperature:"));
    Serial.println(buffer);
    mqttSend(TOPIC_TEMPERATURE_PUB, buffer);

    itoa(humidity, buffer, 10);
    Serial.print(F("humidity:"));
    Serial.println(buffer);
    mqttSend(TOPIC_HUMIDITY_PUB, buffer);

    temperature = 0;
    humidity = 0;
  }

  // Reset the watchdog
  wdt_reset();
}

void CheckPIR()
{
  val = digitalRead(PIR); // read input value
  if (val == HIGH)
  {                             // check if the input is HIGH
    digitalWrite(redLED, HIGH); // turn LED ON
    if (pirState == LOW)
    {
      // we have just turned oπ
      Serial.println(F("PIR motion detected"));
      // We only want to print on the output change, not state
      pirState = HIGH;
      // Send a message to MQTT
      mqttSend(TOPIC_ZONETRIGGER_PUB, "1");
    }
  }
  else
  {
    digitalWrite(redLED, LOW); // turn LED OFF
    if (pirState == HIGH)
    {
      // we have just turned off
      Serial.println(F("PIR motion detection ended"));
      // We only want to print on the output change, not state
      pirState = LOW;
    }
  }
}

void CheckDHT()
{
  int chk;
  chk = DHT.read(DHT11_PIN);
  switch (chk)
  {
  case DHTLIB_OK:
    Serial.println(F("DHT read success"));
    temperature = DHT.temperature;
    humidity = DHT.humidity;
    break;
  default:
    Serial.println(F("DHT read failure"));
    temperature = 0;
    humidity = 0;
    break;
  }
}

void mqttSend(char *topic, char *message)
{
  Ciao.write(CONNECTOR, topic, message);
  Serial.print(F("MQTT message: "));
  Serial.println(message);
}