# SILK_OS v1.0 — Cocoon YOLO Sorting Machine Dashboard

A real-time IoT sorting machine dashboard powered by YOLOv11 object detection, Arduino hardware control, and a Flask backend. The system classifies silk cocoons via a live webcam feed and routes them through a conveyor + servo sorting mechanism.

---

## Project Structure

```
cocoon_yolo/
├── app.py                 # Flask backend (webcam feed, YOLO inference, serial, API)
├── app.js                 # Frontend polling logic & DOM updaters
├── index.html             # Dashboard UI (Tailwind CSS)
├── cocoon_yolo.ipynb      # Jupyter notebook (YOLO training/testing)
├── cocoon_model.pt        # Trained YOLOv11 model file
├── firmware/
│   └── firmware.ino       # Arduino firmware (non-blocking, millis-based)
└── README.md              # This file
```

---

## Prerequisites

### Software

| Tool | Version | Purpose |
|------|---------|---------|
| **Python** | 3.8+ | Backend server runtime |
| **pip** | Latest | Python package manager |
| **Arduino IDE** | 2.x | Flashing firmware to microcontroller |
| **Web Browser** | Chrome / Edge / Firefox | Viewing the dashboard |
| **Git** *(optional)* | Latest | Version control |

### Hardware

| Component | Description |
|-----------|-------------|
| **Arduino Uno/Mega** | Microcontroller board |
| **L298N Motor Driver** | Controls the RC-370 DC conveyor motor |
| **RC-370 DC Motor** | Conveyor belt drive |
| **IR Sensor 1** (Digital) | Sorting sensor (controls conveyor stop/go) |
| **IR Sensor 2** (Analog) | Moisture sensor (detects wet/dry cocoons) |
| **2× Servo Motors** | Sorting gate mechanism (Good vs Bad routing) |
| **HC385XLG 16V Pump** | Water/air pump (via MOSFET or Relay) |
| **USB Cable** | Arduino ↔ Laptop serial connection |
| **Webcam** | Built-in or USB webcam for YOLO feed |

---

## Installation

### 1. Clone the Repository

```bash
git clone https://github.com/your-username/cocoon_yolo.git
cd cocoon_yolo
```

### 2. Install Python Dependencies

```bash
pip install flask opencv-python pyserial ultralytics
```

| Package | Purpose |
|---------|---------|
| `flask` | Web server serving the dashboard and API |
| `opencv-python` | Webcam capture and image processing |
| `pyserial` | Serial communication with Arduino |
| `ultralytics` | YOLOv11 engine for real-time inference |

### 3. Flash the Arduino Firmware

1. Open `firmware/firmware.ino` in the **Arduino IDE**.
2. Install the **Servo** library if not already present (comes built-in with most Arduino IDE installs).
3. Select your board (e.g., **Arduino Uno**) and the correct **COM port** from `Tools > Port`.
4. Click **Upload** (→ button).

#### Default Pin Wiring

| Component | Arduino Pin |
|-----------|-------------|
| L298N `ENA` (PWM) | `3` |
| L298N `IN1` | `4` |
| L298N `IN2` | `5` |
| Pump (MOSFET gate) | `6` |
| IR Sensor 1 (Digital) | `2` |
| IR Sensor 2 (Analog) | `A0` |
| Servo 1 | `9` |
| Servo 2 | `10` |

> **Note:** Modify the pin constants at the top of `firmware.ino` if your wiring differs.

---

## Configuration

### Set the COM Port (Python)

Open `app.py` and change the `COM_PORT` variable to match your Arduino's serial port:

```python
# app.py — Line 10
COM_PORT = 'COM3'  # Windows: COM3, COM4, etc.
                   # macOS:   /dev/tty.usbmodem14101
                   # Linux:   /dev/ttyUSB0 or /dev/ttyACM0
```

To find your port:
- **Windows:** Open Device Manager → Ports (COM & LPT)
- **macOS/Linux:** Run `ls /dev/tty*` in a terminal

### Real-time Telemetry
The dashboard is now strictly connected to hardware telemetry. Mock data generation has been removed to ensure production accuracy. Ensure the Flask server is running and the Arduino is connected to see live updates.

---

## Running the Project

### Step 1: Connect your Arduino via USB

Make sure the firmware is already flashed and the board is powered.

### Step 2: Start the Flask Server

```bash
python app.py
```

You should see:

```
Successfully connected to Arduino on COM3
 * Running on http://0.0.0.0:5000
```

> If the Arduino is not connected, the server will print a warning but **still start** — the webcam feed and YOLO inference will work fine.

### Step 3: Open the Dashboard

Open your browser and navigate to:

```
http://localhost:5000
```

Or open `index.html` directly for the full dashboard UI (the webcam feed and API calls will work as long as the Flask server is running on port 5000).

---

## API Reference

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Basic camera feed page |
| `/video_feed` | GET | MJPEG webcam stream |
| `/api/telemetry` | GET | Latest hardware telemetry as JSON |

### Example `/api/telemetry` Response

```json
{
  "metrics": {
    "total": 142,
    "good": 128,
    "bad": 14,
    "fps": "29.8"
  },
  "hardware": {
    "motorA": "RUNNING",
    "pump": "STOPPED",
    "ir1": "CLEAR"
  },
  "environment": {
    "moisture": 512
  }
}
```

---

## Serial Commands (Python → Arduino)

The Flask backend (or any serial terminal) can send these commands to the Arduino:

| Command | Action |
|---------|--------|
| `START` | Start the conveyor motor |
| `STOP` | Stop the conveyor motor |
| `PUMP_ON` | Activate the water/air pump |
| `PUMP_OFF` | Deactivate the pump |
| `SERVO1:90` | Move Servo 1 to 90° (0–180) |
| `SERVO2:45` | Move Servo 2 to 45° (0–180) |
| `ADD_GOOD` | Increment the good counter |
| `ADD_BAD` | Increment the bad counter |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `ModuleNotFoundError: No module named 'serial'` | Run `pip install pyserial` (not `pip install serial`) |
| `Could not connect to Arduino on COM3` | Check the correct COM port in Device Manager and update `COM_PORT` in `app.py` |
| Camera feed shows "STREAM OFFLINE" | Ensure your webcam is connected and not used by another app |
| Dashboard shows `CONN_LOST` | Verify the Flask server is running on port 5000 |
| Arduino serial output is garbled | Confirm baud rate is `115200` in both `firmware.ino` and `app.py` |
| `cv2` import error | Run `pip install opencv-python` |
| `Failed to load cocoon_model.pt` | Ensure `cocoon_model.pt` is in the project root directory |
| Low FPS / Laggy stream | YOLO inference is CPU/GPU intensive; ensure high-performance mode or a dedicated GPU |

---

## License

This project is for educational and research purposes.
