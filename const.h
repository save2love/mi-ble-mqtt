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

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

typedef struct {
  char* name;
  uint8_t sign1;
  uint8_t sign2;
} XiaomiDevice;

const XiaomiDevice xiaomiDevices[] {
  { "MJ_HT_V1",             0xAA, 0x01 },
  { "LYWSD02",              0x5B, 0x04 },
  { "LYWSD03MMC",           0x5B, 0x05 },
  { "CGG1",                 0x47, 0x03 },
  { "CGD1",                 0x76, 0x05 }, // Qingping Alarm Clock
  { "MHO-C401",             0x87, 0x03 },
  { "MHO-C303",             0xd3, 0x06 },
  { "JQJCY01YM",            0xDF, 0x02 }
};

uint8_t xiaomiDevicesSize = ARRAY_SIZE(xiaomiDevices);
