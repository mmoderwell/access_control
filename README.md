# access_control
#### Microcontroller project with the ESP8266 and MF RC522 RFID card reader to control the locking and unlocking of a door.

### Hardware used:
- ESP8266 (NodeMCU v2.0)
- Mifare RC522 RFID card reader from [Banggood](https://www.banggood.com/RC522-Chip-IC-Card-Induction-Module-RFID-Reader-p-81067.html?cur_warehouse=CN)
- MG995 High Torgue Analog Servo from [Banggood](https://www.banggood.com/MG995-High-Torgue-Mental-Gear-Analog-Servo-p-73885.html?rmmds=myorder&cur_warehouse=CN)
- WS2812 RGB LED

### Wiring:

###### Wiring the MF RC522 to ESP8266 (ESP-12) or NodeMCU v2
RST     = GPIO5 (D1)
SDA(SS) = GPIO4 (D2)
MOSI    = GPIO13 (D7)
MISO    = GPIO12 (D6)
SCK     = GPIO14 (D5)
GND     = GND 
3.3V    = 3.3V

###### The data pin for the WS2812B LED
D8 on NodeMCU v2

###### The servo
D4 on NodeMCU v2 

