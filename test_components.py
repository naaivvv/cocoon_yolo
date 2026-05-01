import cv2
import serial
import threading
import json
import time
from flask import Flask, request, jsonify, Response
from ultralytics import YOLO

app = Flask("Cocoon Manual Test Server")

COM_PORT = 'COM3'
BAUD_RATE = 9600

latest_telemetry = {
    "metrics": {"fps": "0.0"},
    "hardware": {"motorA": "STOPPED", "hopper": "STOPPED", "servo1": 90, "servo2": 90, "ir1": "CLEAR"},
    "environment": {"moisture": 0}
}
serial_connected = False
arduino = None

try:
    model = YOLO('cocoon_model.pt')
except Exception as e:
    print(f"Failed to load cocoon_model.pt: {e}")
    model = None

camera = cv2.VideoCapture(1)

def serial_reader():
    global latest_telemetry, serial_connected, arduino
    while True:
        if not serial_connected:
            try:
                arduino = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
                serial_connected = True
                print(f"Connected to Arduino on {COM_PORT}")
                time.sleep(2)
            except Exception:
                serial_connected = False
                time.sleep(2)
                continue

        try:
            if arduino and arduino.in_waiting > 0:
                line = arduino.readline().decode('utf-8', errors='replace').strip()
                if line.startswith('{') and line.endswith('}'):
                    try:
                        parsed = json.loads(line)
                        for key, value in parsed.items():
                            if isinstance(value, dict) and key in latest_telemetry:
                                latest_telemetry[key].update(value)
                            else:
                                latest_telemetry[key] = value
                    except:
                        pass
        except Exception:
            serial_connected = False
            if arduino:
                arduino.close()
            arduino = None
            time.sleep(2)

threading.Thread(target=serial_reader, daemon=True).start()

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

            # YOLO object detection (Visual only, no auto-commands)
            if model is not None:
                results = model(frame, verbose=False, agnostic_nms=True)
                annotated_frame = results[0].plot()
            else:
                annotated_frame = frame

            ret, buffer = cv2.imencode('.jpg', annotated_frame)
            frame_bytes = buffer.tobytes()

            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route("/")
def index():
    html_content = """
    <!DOCTYPE html>
    <html>
    <head>
        <title>Manual Hardware Tester</title>
        <style>
            body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #121212; color: #fff; padding: 20px; }
            .container { max-width: 1000px; margin: 0 auto; display: flex; gap: 20px; flex-wrap: wrap; }
            .main-content { flex: 1; min-width: 600px; }
            .side-content { flex: 0 0 350px; }
            h1 { text-align: center; color: #00e676; width: 100%; }
            .panel { background: #1e1e1e; padding: 20px; border-radius: 10px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
            button { background: #333; color: #fff; border: 1px solid #555; padding: 8px 15px; margin: 5px; border-radius: 5px; cursor: pointer; transition: background 0.3s; }
            button:hover { background: #00e676; color: #000; }
            .btn-danger:hover { background: #f44336; color: #fff; }
            .btn-primary { background: #2196f3; color: #fff; }
            .btn-primary:hover { background: #64b5f6; }
            input[type="number"] { padding: 8px; border-radius: 5px; border: 1px solid #555; background: #333; color: #fff; }
            pre { background: #000; padding: 15px; border-radius: 5px; color: #00e676; overflow-x: auto; font-size: 14px; }
            h3 { margin-top: 0; color: #ddd; }
            h4 { margin-bottom: 5px; color: #bbb; }
        </style>
    </head>
    <body>
        <h1>Cocoon Granular Tester</h1>
        <div class="container">
            <div class="main-content">
                <div class="panel">
                    <h3>YOLO Camera Feed</h3>
                    <img src="/video_feed" width="100%" style="border: 2px solid #555; border-radius: 5px;"/>
                </div>
            </div>
            <div class="side-content">
                <div class="panel">
                    <h3>Hardware Controls</h3>
                    <div style="margin-bottom: 15px;">
                        <h4>Hopper Motor</h4>
                        <button class="btn-primary" onclick="sendCommand('HOPPER:1')">ON</button>
                        <button class="btn-danger" onclick="sendCommand('HOPPER:0')">OFF</button>
                    </div>
                    <div style="margin-bottom: 15px;">
                        <h4>Conveyor Motor</h4>
                        <button class="btn-primary" onclick="sendCommand('CONV:1')">ON</button>
                        <button class="btn-danger" onclick="sendCommand('CONV:0')">OFF</button>
                    </div>
                    <div style="margin-bottom: 15px;">
                        <h4>Servo 1 (Defect Sort)</h4>
                        <input type="number" id="s1_pos" value="90" style="width: 60px;">
                        <button onclick="sendCommand('S1:' + document.getElementById('s1_pos').value)">Set Angle</button>
                    </div>
                    <div style="margin-bottom: 15px;">
                        <h4>Servo 2 (Moisture Sort)</h4>
                        <input type="number" id="s2_pos" value="90" style="width: 60px;">
                        <button onclick="sendCommand('S2:' + document.getElementById('s2_pos').value)">Set Angle</button>
                    </div>
                </div>
                <div class="panel">
                    <h3>Live Telemetry</h3>
                    <pre id="telemetry">Waiting for data...</pre>
                </div>
            </div>
        </div>
        <script>
            function sendCommand(action) {
                fetch('/api/command', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ action: action })
                }).then(r => r.json()).then(data => console.log(data));
            }
            setInterval(() => {
                fetch('/api/telemetry')
                    .then(r => r.json())
                    .then(data => {
                        document.getElementById('telemetry').innerText = JSON.stringify(data, null, 2);
                    }).catch(e => console.error(e));
            }, 500);
        </script>
    </body>
    </html>
    """
    return html_content

@app.route("/video_feed")
def video_feed():
    return Response(generate_frames(), 
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route("/api/telemetry")
def get_telemetry():
    return jsonify(latest_telemetry)

@app.route("/api/command", methods=["POST"])
def post_command():
    global serial_connected, arduino
    if not serial_connected or not arduino:
        return jsonify({"status": "error", "message": "Serial not connected"}), 503
    
    try:
        data = request.json
        command_str = data.get("action", "")
        if command_str:
            arduino.write((command_str + "\n").encode('utf-8'))
            print(f"Sent manual command: {command_str}")
            return jsonify({"status": "success", "message": f"Sent {command_str}"})
        return jsonify({"status": "error", "message": "No action provided"}), 400
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=8000, use_reloader=False)
