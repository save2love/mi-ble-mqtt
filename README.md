# Xiaomi MI Temperature and Humidity Sensor BLE to MQTT gateway
BLE to MQTT gateway for Xiaomi MI Temperature and Humidity Sensor with BLE and LCD

![Schema](/schema.png "Schema")

## Increasing partition size to upload sketch

### Generate new partition table

* Use my partition table from the file [default.csv](/partitions/default.csv)

| Name    | Type | SubType | Offset   | Size     | Flags |
| ------- | ---- | ------- | -------- | -------- | ----- |
| nvs     | data | nvs     | 0x9000   | 0x5000   |       |
| otadata | data | ota     | 0xe000   | 0x2000   |       |
| app0    | app  | ota_0   | 0x10000  | 0x190000 |       |
| app1    | app  | ota_1   | 0x200000 | 0x190000 |       |
| eeprom  | data | 0x99    | 0x390000 | 0x1000   |       |
| spiffs  | data | spiffs  | 0x391000 | 0x6F000  |       |

* You can use my [default.bin](/partitions/default.bin) or use command ```gen_esp32part.exe -q "partitions/default.csv" "partitions/default.bin"``` to generate your binary partition table file. You also can change file names default.csv and default.bin to yours.

### Increasing upload size and using new partition table

* Open file ```[ARDUINO_DIR]/hardware/espressif/esp32/boards.txt``` and find your board
* ```[YOUR_BOARD].upload.maximum_size=1310720``` : replace 1310720 to 1672864 for your board
* If you changed default partitions name to your name use it: ```[YOUR_BOARD].build.partitions=[YOUR_PARTITIONS]```

## Legal

*Xiaomi* and *Mi* are registered trademarks of BEIJING XIAOMI TECHNOLOGY CO., LTD.

This project is in no way affiliated with, authorized, maintained, sponsored or endorsed by *Xiaomi* or any of its affiliates or subsidiaries.

## Disclaimer

This software was designed to be used only for research purposes. This software comes with no warranties of any kind whatsoever,
and may not be useful for anything. Use it at your own risk! If these terms are not acceptable, you aren't allowed to use the code.

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details
