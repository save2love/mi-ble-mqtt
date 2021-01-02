/*
  Code based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleScan.cpp
  Ported to Arduino ESP32 by Evandro Copercini and based on https://github.com/turlvo/KuKuMi/
  Additional improvements by Alexander Savychev https://github.com/save2love/mi-ble-mqtt
*/
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFi.h>
#include <WiFiMulti.h>
#include <PubSubClient.h>

#include "soc/rtc_cntl_reg.h"
#include "esp_system.h"
#include "const.h"

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
    uint8_t getServiceData(uint8_t uuid_len, uint8_t* payload, uint8_t length, uint8_t* data) {
      if (length < uuid_len)
        return 0;
      if (length > uuid_len) {
        memcpy(data, payload + uuid_len, length - uuid_len);
        return length - uuid_len;
      }
      return 0;
    }

    uint8_t tryParsePayload(uint8_t* payload, size_t payload_len, uint8_t* service_data) {
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
                return getServiceData(2, payload, length, service_data);
              }

            case ESP_BLE_AD_TYPE_32SERVICE_DATA: { // 4 byte UUID
                return getServiceData(4, payload, length, service_data);
              }

            case ESP_BLE_AD_TYPE_128SERVICE_DATA: { // 16 byte UUID
                return getServiceData(16, payload, length, service_data);
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
      Serial.print(F(" = "));
      Serial.print(value);
      Serial.println();
    }

    void processData(XiaomiDevice device, uint8_t* payload, size_t len, char* addr) {
      uint8_t data[30];
      uint8_t raw_data_length = tryParsePayload(payload, len, data);
      if (raw_data_length == 0) {
        Serial.println(F("Skip! No service date"));
        return;
      }

      bool has_data = data[0] & 0x40;
      bool has_capability = data[0] & 0x20;
      bool has_encryption = data[0] & 0x08;

      if (!has_data) {
        Serial.println(F("Skip! Service data has no DATA flag"));
        return;
      }

      if (has_encryption) {
        Serial.println(F("Skip! Service data is encrypted"));
        return;
      }

      /*static uint8_t last_frame_count = 0;
        if (last_frame_count == data[4]) {
        Serial.println(F("Skip! Duplicate data packet received"));
        return;
        }
        last_frame_count = data[4];*/

      // Check device signature
      if (data[2] != device.sign1 || data[3] != device.sign2) {
        Serial.println(F("Skip! Wrong device signature"));
        return;
      }

      uint16_t temp, humd;
      uint8_t bat;
      char value[10];

      uint8_t data_offset = has_capability ? 12 : 11;
      raw_data_length = raw_data_length - data_offset;

      while (raw_data_length > 0) {
        if (data[data_offset + 1] != 0x10) {
          Serial.println(F("Skip! Fixed byte (0x10) not found"));
          break;
        }

        const uint8_t value_length = data[data_offset + 2];
        if ((value_length < 1) || (value_length > 4) || (raw_data_length < (3 + value_length))) {
          Serial.printf("Skip! Value has wrong size (%d)!", value_length);
          break;
        }

        const uint8_t value_type = data[data_offset + 0];
        //const uint8_t *data = &data[data_offset + 3];

        switch (data[data_offset]) {
          case 0x04:
            temp = readValue(data, data_offset + 4, data_offset + 3);
            sprintf(value, "%.1f", temp / 10.0);
            publish(addr, "temp", value);
            break;

          case 0x06:
            humd = readValue(data, data_offset + 4, data_offset + 3);
            sprintf(value, "%.1f", humd / 10.0);
            publish(addr, "humd", value);
            break;

          case 0x0A:
            bat = data[data_offset + 3];
            sprintf(value, "%d", bat);
            publish(addr, "battery", value);
            break;

          case 0x0D:
            temp = readValue(data, data_offset + 4, data_offset + 3);
            sprintf(value, "%.1f", temp / 10.0);
            publish(addr, "temp", value);
            humd = readValue(data, data_offset + 6, data_offset + 5);
            sprintf(value, "%.1f", humd / 10.0);
            publish(addr, "humd", value);
            break;

          default:
            Serial.print(F("Unknown value type: "));
            Serial.print(data[data_offset]);
            break;
        }

        raw_data_length -= 3 + value_length;
        data_offset += 3 + value_length;
      }
    }

    uint8_t getDeviceIndexByName(std::string name) {
      for (int i = 0; i < xiaomiDevicesSize; i++) {
        if (!name.compare(xiaomiDevices[i].name)) {
          return i;
        }
      }
      return 0xFF;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice)
    {
      if (!advertisedDevice.haveName() || !advertisedDevice.haveServiceData())
        return;

      std::string dname = advertisedDevice.getName();
      Serial.print(F("Device: "));
      Serial.print(dname.c_str());

      uint8_t dIndex = getDeviceIndexByName(dname);
      if (dIndex >= xiaomiDevicesSize) {
        Serial.println(F(" Not supported!"));
        return;
      }
      Serial.println(F(" OK!"));

      XiaomiDevice device = xiaomiDevices[dIndex];

      size_t len = advertisedDevice.getPayloadLength();
      uint8_t* payload = advertisedDevice.getPayload();
      char addr[20];
      strcpy(addr, advertisedDevice.getAddress().toString().c_str());

      digitalWrite(PIN_LED, HIGH);
      processData(device, payload, len, addr);
      digitalWrite(PIN_LED, LOW);
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
    Serial.print(F("Attempting MQTT connection..."));

    if (client.connect(MQTT_GW_NAME)) {
      Serial.println(F("connected"));
    } else {
      Serial.print(F("failed, rc="));
      Serial.print(client.state());
      Serial.println(F(" try again in 5 seconds"));
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
  Serial.println(F("After deep sleep"));
#endif
}
