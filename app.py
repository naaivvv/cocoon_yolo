import cv2
import serial
import threading
import json
import time
from flask import Flask, render_template, Response, jsonify

app = Flask(__name__)

# '0' is the standard ID for a built-in laptop webcam
camera = cv2.VideoCapture(0)

# --- SERIAL COMMUNICATION SETUP ---
COM_PORT = 'COM3'  # Windows usually uses COM ports
BAUD_RATE = 115200

# Global variable to store latest serial data
latest_telemetry = {
    "metrics": {"total": 0, "good": 0, "bad": 0, "fps": "0.0"},
    "hardware": {"motorA": "STOPPED", "motorB": "STOPPED", "ir1": "CLEAR", "ir2": "CLEAR"},
    "environment": {"temp": 0.0, "humidity": 0.0, "moisture": 0}
}
serial_connected = False
arduino = None

def serial_reader():
    global latest_telemetry, serial_connected, arduino
    try:
        arduino = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
        serial_connected = True
        print(f"Successfully connected to Arduino on {COM_PORT}")
    except serial.SerialException as e:
        print(f"WARNING: Could not connect to Arduino on {COM_PORT}. Error: {e}")
        serial_connected = False

    while serial_connected:
        try:
            if arduino.in_waiting > 0:
                line = arduino.readline().decode('utf-8').strip()
                if line.startswith('{') and line.endswith('}'):
                    try:
                        parsed_data = json.loads(line)
                        latest_telemetry.update(parsed_data)
                    except json.JSONDecodeError:
                        pass # Ignore malformed json
        except Exception as e:
            print(f"Serial read error: {e}")
            time.sleep(1)

# Start background thread
reader_thread = threading.Thread(target=serial_reader, daemon=True)
reader_thread.start()

def generate_frames():
    fps = 0.0
    start_time = time.time()
    frames = 0
    while True:
        success, frame = camera.read()
        if not success:
            break
        else:
            frames += 1
            elapsed = time.time() - start_time
            if elapsed > 1.0:
                fps = frames / elapsed
                latest_telemetry["metrics"]["fps"] = f"{fps:.1f}"
                start_time = time.time()
                frames = 0

            ret, buffer = cv2.imencode('.jpg', frame)
            frame = buffer.tobytes()

            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame + b'\r\n')

@app.route('/')
def index():
    # Simple placeholder index. Typically JS/HTML handles the dashboard logic
    return """
    <html>
      <body style="background: #222; color: white; text-align: center;">
        <h1>Laptop Camera Live Feed & Telemetry API Active</h1>
        <img src="/video_feed" style="border: 5px solid #555; border-radius: 10px; width: 80%;">
      </body>
    </html>
    """

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), 
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/api/telemetry')
def telemetry():
    return jsonify(latest_telemetry)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True)