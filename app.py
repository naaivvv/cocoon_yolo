import cv2
import serial
import threading
import json
import time
import argparse
from flask import Flask, render_template, Response, jsonify, send_from_directory, request
from ultralytics import YOLO

parser = argparse.ArgumentParser(description="Cocoon YOLO Backend")
parser.add_argument('--debug', action='store_true', help='Enable verbose serial and system debugging')
args = parser.parse_args()
DEBUG_MODE = args.debug

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
BAUD_RATE = 115200

latest_telemetry = {
    "metrics": {"total": 0, "good": 0, "defect": 0, "moisture_reject": 0, "fps": "0.0"},
    "hardware": {"motorA": "STOPPED", "hopper": "STOPPED", "ir1": "CLEAR"},
    "environment": {"moisture": 0}
}
serial_connected = False
arduino = None
defect_eval_sent = False
last_eval_time = 0
instant_yolo_trigger = False

def serial_reader():
    global latest_telemetry, serial_connected, arduino
    
    while True:
        if not serial_connected:
            try:
                arduino = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
                serial_connected = True
                print(f"Successfully connected to Arduino on {COM_PORT}")
                time.sleep(2)  # Wait for Arduino to finish booting after DTR reset
                arduino.write(b"RESET\n")  # Zero all hardware counters on connect
                print("[INFO] Sent RESET to Arduino - counters zeroed")
            except serial.SerialException as e:
                # Suppress constant printing, only print once per disconnect if needed
                serial_connected = False
                time.sleep(2)
                continue

        try:
            if arduino and arduino.in_waiting > 0:
                line = arduino.readline().decode('utf-8', errors='replace').strip()
                if DEBUG_MODE and line:
                    print(f"[SERIAL RAW] {line}")
                elif line.startswith('[DEBUG]'):
                    print(line)
                elif line == "TRIG":
                    print("[INFO] Instant TRIG received from Arduino")
                    global instant_yolo_trigger
                    instant_yolo_trigger = True
                
                if line.startswith('{') and line.endswith('}'):
                    try:
                        parsed_data = json.loads(line)
                        # Deep merge: update nested dicts instead of replacing them
                        # This preserves Python-side keys like 'fps' inside metrics
                        for key, value in parsed_data.items():
                            if isinstance(value, dict) and key in latest_telemetry:
                                latest_telemetry[key].update(value)
                            else:
                                latest_telemetry[key] = value
                    except json.JSONDecodeError:
                        pass # Ignore malformed json
        except serial.SerialException as e:
            print(f"Serial connection lost: {e}")
            serial_connected = False
            if arduino:
                arduino.close()
            arduino = None
            time.sleep(2)
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
                # Use agnostic_nms=True to prevent overlapping bounding boxes
                # of different classes (e.g. good and bad) on the same object.
                results = model(frame, verbose=False, agnostic_nms=True)
                annotated_frame = results[0].plot()
                
                global defect_eval_sent, last_eval_time, serial_connected, arduino, instant_yolo_trigger
                hardware_state = latest_telemetry.get("hardware", {})
                if serial_connected and arduino is not None:
                    current_time = time.time()
                    
                    is_blocked = instant_yolo_trigger or (hardware_state.get("ir1") == "BLOCKED")
                    
                    if is_blocked:
                        if not defect_eval_sent or (current_time - last_eval_time > 0.5):
                            # Check if any detected box is classified as 'bad'
                            has_defect = False
                            boxes = results[0].boxes
                            if boxes is not None and len(boxes) > 0:
                                for box in boxes:
                                    class_id = int(box.cls[0])
                                    class_name = model.names[class_id].lower()
                                    print(f"[YOLO] Detected class: '{class_name}'")
                                    if class_name == "bad":
                                        has_defect = True
                                        break
                            
                            cmd = b"DEFECT:YES\n" if has_defect else b"DEFECT:NO\n"
                            try:
                                arduino.write(cmd)
                                if DEBUG_MODE:
                                    print(f"[DEBUG] Sent YOLO result to Arduino: {cmd.strip().decode('utf-8')} for IR1=BLOCKED")
                                else:
                                    print(f"Sent YOLO result to Arduino: {cmd.strip().decode('utf-8')}")
                                defect_eval_sent = True
                                instant_yolo_trigger = False
                                last_eval_time = current_time
                            except Exception as e:
                                print(f"Failed to send command to Arduino: {e}")
                    
                    elif hardware_state.get("ir1") == "CLEAR":
                        if defect_eval_sent and DEBUG_MODE:
                            print("[DEBUG] IR1 is now CLEAR. Resetting defect_eval_sent to False.")
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

@app.route('/api/command', methods=['POST'])
def command():
    global serial_connected, arduino
    data = request.json
    action = data.get('action')
    if action == 'start':
        if serial_connected and arduino:
            try:
                arduino.write(b"START\n")
                return jsonify({"status": "success", "message": "START sent"})
            except Exception as e:
                return jsonify({"status": "error", "message": str(e)}), 500
        return jsonify({"status": "error", "message": "Serial not connected"}), 503
    elif action == 'stop':
        if serial_connected and arduino:
            try:
                arduino.write(b"STOP\n")
                return jsonify({"status": "success", "message": "STOP sent"})
            except Exception as e:
                return jsonify({"status": "error", "message": str(e)}), 500
        return jsonify({"status": "error", "message": "Serial not connected"}), 503
    elif action.startswith('SPEED:'):
        if serial_connected and arduino:
            try:
                arduino.write(f"{action}\n".encode('utf-8'))
                return jsonify({"status": "success", "message": f"{action} sent"})
            except Exception as e:
                return jsonify({"status": "error", "message": str(e)}), 500
        return jsonify({"status": "error", "message": "Serial not connected"}), 503
    return jsonify({"status": "error", "message": "Invalid action"}), 400

if __name__ == "__main__":
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)
