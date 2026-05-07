# COM3505 IoT Assignment

This project contains an ESP32-S3 Feather thermistor and LED-pattern system with a Flask web dashboard.

The ESP32 reads the thermistor value, calculates the temperature, sends the latest reading to the Flask server, and checks which LED pattern has been selected from the browser dashboard.

## Project structure

```text
COM3505_IoT_Assignment/
├── esp32/
│   ├── platformio.ini
│   └── src/
│       └── main.cpp
└── flask_server/
    ├── app.py
    ├── requirements.txt
    ├── static/
    └── templates/
```

## Hardware used

- Adafruit ESP32-S3 Feather
- NTC thermistor circuit connected to `A2`
- Red LED connected to `D5`
- Yellow LED connected to `D6`
- Green LED connected to `D9`

Current pin mapping in `main.cpp`:

```cpp
const int thermistorPin = A2;
const int redLedPin = 5;
const int yellowLedPin = 6;
const int greenLedPin = 9;
```

## 1. Start the Flask server

Open a terminal in the project folder:

```bash
cd flask_server
```

Create and activate a Python virtual environment:

```bash
python3 -m venv venv
source venv/bin/activate
```

On Windows PowerShell, use:

```powershell
python -m venv venv
.\venv\Scripts\Activate.ps1
```

Install the required Python packages:

```bash
pip install -r requirements.txt
```

Run the Flask server:

```bash
python app.py
```

The server runs on:

```text
http://0.0.0.0:5001
```

To open the dashboard on the same computer, use:

```text
http://127.0.0.1:5001
```

## 2. Find your computer IP address

The ESP32 needs the IP address of the computer running the Flask server.

### macOS

Run:

```bash
ipconfig getifaddr en0
```

If you are using Ethernet instead of WiFi, try:

```bash
ipconfig getifaddr en1
```

### Windows

Run:

```powershell
ipconfig
```

Look for the `IPv4 Address` under your WiFi adapter.

Example IP address:

```text
192.168.1.23
```

## 3. Configure WiFi and server IP in `main.cpp`

Open this file:

```text
esp32/src/main.cpp
```

At the top of the file, change these three values:

```cpp
#define WIFI_SSID "ENTER_YOUR_WIFI_SSID_HERE"
#define WIFI_PASSWORD "ENTER_YOUR_WIFI_PASSWORD_HERE"
#define FLASK_SERVER_BASE_URL "http://ENTER_YOUR_COMPUTER_IP_HERE:5001"
```

Example:

```cpp
#define WIFI_SSID "MyPhoneHotspot"
#define WIFI_PASSWORD "my-password"
#define FLASK_SERVER_BASE_URL "http://192.168.1.23:5001"
```

Important: the ESP32 and the computer running Flask must be connected to the same WiFi network.

If you are using an open WiFi network, leave the password empty:

```cpp
#define WIFI_PASSWORD ""
```

## 4. Upload the ESP32 code

Open the `esp32` folder in VS Code with the PlatformIO extension installed.

Then use PlatformIO to:

1. Build the project.
2. Upload it to the ESP32-S3 Feather.
3. Open the Serial Monitor at `115200` baud.

The PlatformIO environment is defined in:

```text
esp32/platformio.ini
```

Expected environment:

```ini
[env:adafruit_feather_esp32s3]
platform = espressif32
board = adafruit_feather_esp32s3
framework = arduino
monitor_speed = 115200
```

## 5. Test the system

After uploading, open the Serial Monitor. You should see:

- the ESP32 MAC address
- WiFi connection status
- ESP32 IP address
- ADC reading
- voltage
- calculated resistance
- temperature
- current LED pattern
- POST and GET request status codes

Open the Flask dashboard in a browser:

```text
http://127.0.0.1:5001
```

From the dashboard, select an LED pattern. The ESP32 polls the Flask server every 2 seconds and should update the LEDs.

## Troubleshooting

### ESP32 cannot connect to WiFi

Check that:

- `WIFI_SSID` is exactly correct, including capital letters and spaces.
- `WIFI_PASSWORD` is correct.
- the network is 2.4 GHz compatible.
- the ESP32 and Flask server are on the same network.

### ESP32 connects to WiFi but cannot reach Flask

Check that:

- `FLASK_SERVER_BASE_URL` uses your computer IP address, not `127.0.0.1`.
- the Flask server is still running.
- the port is `5001`.
- your firewall allows incoming connections to Python/Flask.

Correct example:

```cpp
#define FLASK_SERVER_BASE_URL "http://192.168.1.23:5001"
```

Incorrect for the ESP32:

```cpp
#define FLASK_SERVER_BASE_URL "http://127.0.0.1:5001"
```

`127.0.0.1` would point to the ESP32 itself, not your computer.

### Temperature shows `nan`

Check that:

- the thermistor circuit is connected to `A2`.
- the fixed resistor value matches the value in `main.cpp`.
- the voltage divider wiring is correct.
- the ADC reading is not stuck near `0` or `4095`.

## Notes for marking/demo

Before demonstrating the project:

1. Start the Flask server first.
2. Confirm your computer IP address.
3. Update `FLASK_SERVER_BASE_URL` in `main.cpp` if your IP address has changed.
4. Upload the ESP32 code.
5. Open the Serial Monitor and confirm that POST/GET requests return successful status codes.
6. Open the browser dashboard and test each LED pattern.
