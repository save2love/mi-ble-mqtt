/*
Code based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
Ported to Arduino ESP32 by Evandro Copercini and based on https://github.com/turlvo/KuKuMi/
Additional work by Alexander Savychev https://github.com/save2love/mi-ble-mqtt
*/
#define WIFI_SSID     "SSID"
#define WIFI_PASSWORD "qwerty12345"

#define MQTT_SERVER   "192.168.0.1"
#define MQTT_PORT     1883
#define MQTT_GW_NAME  "BLE_Mi_to_MQTT"
#define MQTT_PREFIX   "MI"
// MQTT topics
// /MI/12:34:56:78:90:ab/temp = XX.X
// /MI/12:34:56:78:90:ab/humd = XX.X
// /MI/12:34:56:78:90:ab/battery = XX

#define PIN_LED   2

#define SCAN_TIME   60 // seconds
#define SLEEP_TIME  60 // seconds

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>

#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"

// WiFi & BT Instance
WiFiMulti wifiMulti;
WiFiClient espClient;
PubSubClient client(espClient);
BLEScan *pBLEScan;

void IRAM_ATTR resetModule() {
  ets_printf("reboot\n");
  //esp_restart_noos();
  //ESP.restart();
  esp_restart();
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
    bool tryGetServiceData(uint8_t uuid_len, uint8_t* payload, uint8_t length, uint8_t* data) {
      if (length < uuid_len)
        return false;
      if (length > uuid_len) {
        memcpy(data, payload + uuid_len, length - uuid_len);
        return data[0] == 0x50 && data[1] == 0x20; // Signature
      }
      return false;
    }

    bool tryParsePayload(uint8_t* payload, size_t payload_len, uint8_t* service_data) {
      int8_t length;
      uint8_t ad_type;
      uint8_t sizeConsumed = 0;
      bool finished = false;

      while (!finished) {
        length = *payload;          // Retrieve the length of the record.
        payload++;                  // Skip to type
        sizeConsumed += 1 + length; // increase the size consumed.

        if (length != 0) { // A length of 0 indicates that we have reached the end.
          ad_type = *payload;
          payload++;
          length--;

          switch (ad_type) {
            case ESP_BLE_AD_TYPE_SERVICE_DATA: {  // 2 byte UUID
                if (tryGetServiceData(2, payload, length, service_data))
                  return true;
                break;
              }

            case ESP_BLE_AD_TYPE_32SERVICE_DATA: { // 4 byte UUID
                if (tryGetServiceData(4, payload, length, service_data))
                  return true;
                break;
              }

            case ESP_BLE_AD_TYPE_128SERVICE_DATA: { // 16 byte UUID
                if (tryGetServiceData(16, payload, length, service_data))
                  return true;
                break;
              }

          }
          payload += length;
        }

        if (sizeConsumed >= payload_len)
          finished = true;
      }
    }

    uint16_t readValue(uint8_t* data, uint8_t i1, uint8_t i2) {
      uint16_t value = 0;
      value |= data[i1] << 8;
      value |= data[i2];
      return value;
    }

    void publish(const char* addr, char* valueName, char* value) {
      char topic[50];
      sprintf(topic, "%s/%s/%s", MQTT_PREFIX, addr, valueName);
      if (client.connected()) {
        client.publish(topic, value);
        client.loop();
      }
      Serial.print(topic);
      Serial.print(" = ");
      Serial.print(value);
      Serial.println();
    }

    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      if (advertisedDevice.haveName() && advertisedDevice.haveServiceData() && !advertisedDevice.getName().compare("MJ_HT_V1")) {
        size_t len = advertisedDevice.getPayloadLength();
        uint8_t* payload = advertisedDevice.getPayload();
        uint8_t data[30];

        digitalWrite(PIN_LED, HIGH);

        if (!tryParsePayload(payload, len, data)) {
          digitalWrite(PIN_LED, LOW);
          return;
        }

        char addr[20];
        strcpy(addr, advertisedDevice.getAddress().toString().c_str());

        uint16_t temp, humd;
        uint8_t bat;
        uint8_t flags = 0; // 1 - temperature, 2 - humidity, 4 - battery
        char value[10];

        switch (data[11]) {
          case 0x04:
            temp = readValue(data, 15, 14);
            sprintf(value, "%.1f", temp / 10.0);
            publish(addr, "temp", value);
            break;
            
          case 0x06:
            humd = readValue(data, 15, 14);
            sprintf(value, "%.1f", humd / 10.0);
            publish(addr, "humd", value);
            break;
            
          case 0x0A:
            bat = data[14];
            sprintf(value, "%d", bat);
            publish(addr, "battery", value);
            break;
            
          case 0x0D:
            temp = readValue(data, 15, 14);
            sprintf(value, "%.1f", temp / 10.0);
            publish(addr, "temp", value);
            humd = readValue(data, 17, 16);
            sprintf(value, "%.1f", humd / 10.0);
            publish(addr, "humd", value);
            break;
        }
        digitalWrite(PIN_LED, LOW);
      }
    }
};

void WiFiEvent(WiFiEvent_t event)
{
  digitalWrite(PIN_LED, HIGH);
  switch (event) {
    case SYSTEM_EVENT_WIFI_READY: Serial.println(F("WiFi ready")); break;
    case SYSTEM_EVENT_SCAN_DONE: Serial.println(F("WiFi finish scanning")); break;
    case SYSTEM_EVENT_STA_CONNECTED: Serial.println(F("WiFi connected")); break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print(F("WiFi got IP: "));
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println(F("WiFi lost connection"));
      blinkLed(5, 250);
      resetModule();
      break;
    default:
      Serial.print(F("WiFi "));
      Serial.println(event);
      break;
  }
  delay(10);
  digitalWrite(PIN_LED, LOW);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");

    if (client.connect(MQTT_GW_NAME)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      blinkLed(10, 500);
    }
  }
}

void blinkLed(uint8_t repeats, uint16_t delayTime) {
  for (uint i = 0; i < repeats; i++) {
    digitalWrite(PIN_LED, HIGH);
    delay(delayTime);
    digitalWrite(PIN_LED, LOW);
    delay(delayTime);
  }
}

void setup()
{
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);

  // BLE init and setting
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(0x50);
  pBLEScan->setWindow(0x30);

  // WiFi Setting and Connecting
  WiFi.onEvent(WiFiEvent);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.println();

  int wifi_retry = 10;
  while (--wifi_retry >= 0) {
    if (wifiMulti.run() == WL_CONNECTED)
      break;

    if (wifi_retry == 0)
      resetModule();

    delay(3000);
  }

  client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
  if (!client.connected())
    reconnect();
  client.loop();

  Serial.printf("Start BLE scan for %d seconds...\n", SCAN_TIME);

  BLEScanResults foundDevices = pBLEScan->start(SCAN_TIME);
  int count = foundDevices.getCount();
  printf("Found device count : %d\n", count);

#if SLEEP_TIME > 0
  //esp_sleep_enable_timer_wakeup(SLEEP_TIME * 1000000); // translate second to micro second
  Serial.printf("Enter deep sleep for %d seconds...\n\r", (SLEEP_TIME));
  delay(10);
  //esp_deep_sleep_start();
  esp_deep_sleep(1000000LL * SLEEP_TIME);
  Serial.println("After deep sleep");
#endif
}
