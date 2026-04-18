import cv2
import serial
import threading
import json
import time
from flask import Flask, render_template, Response, jsonify, send_from_directory
from ultralytics import YOLO

app = Flask(__name__, static_folder='.', static_url_path='')

try:
    model = YOLO('cocoon_model.pt')
except Exception as e:
    print(f"Failed to load cocoon_model.pt: {e}")
    model = None

# '0' is the standard ID for a built-in laptop webcam
camera = cv2.VideoCapture(1)

# --- SERIAL COMMUNICATION SETUP ---
COM_PORT = 'COM3'  # Windows usually uses COM ports
BAUD_RATE = 9600

latest_telemetry = {
    "metrics": {"total": 0, "good": 0, "bad": 0, "fps": "0.0"},
    "hardware": {"motorA": "STOPPED", "hopper": "STOPPED", "ir1": "CLEAR"},
    "environment": {"moisture": 0}
}
serial_connected = False
arduino = None
defect_eval_sent = False

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
                if line.startswith('[DEBUG]'):
                    print(line)
                elif line.startswith('{') and line.endswith('}'):
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

            # YOLO object detection
            if model is not None:
                results = model(frame, verbose=False)
                annotated_frame = results[0].plot()
                
                global defect_eval_sent, serial_connected, arduino
                hardware_state = latest_telemetry.get("hardware", {})
                if serial_connected and arduino is not None:
                    if hardware_state.get("ir1") == "BLOCKED" and not defect_eval_sent:
                        has_defect = len(results[0].boxes) > 0 # Assuming any bounding box is a defect
                        cmd = b"DEFECT:YES\n" if has_defect else b"DEFECT:NO\n"
                        try:
                            arduino.write(cmd)
                            print(f"Sent YOLO result to Arduino: {cmd.strip().decode('utf-8')}")
                            defect_eval_sent = True
                        except Exception as e:
                            print(f"Failed to send command to Arduino: {e}")
                    
                    elif hardware_state.get("ir1") == "CLEAR":
                        defect_eval_sent = False

            else:
                annotated_frame = frame

            ret, buffer = cv2.imencode('.jpg', annotated_frame)
            frame_bytes = buffer.tobytes()

            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/')
def index():
    # Serve the local index.html as the primary dashboard frontend
    return app.send_static_file('index.html')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(), 
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/api/telemetry')
def telemetry():
    return jsonify(latest_telemetry)

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
