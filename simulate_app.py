import time
import threading
import json
import os
import cv2
import numpy as np
import serial
from flask import Flask, jsonify, request, Response

app = Flask(__name__, static_folder='.', static_url_path='')

# --- SERIAL COMMUNICATION SETUP ---
COM_PORT = 'COM3'
BAUD_RATE = 115200

latest_telemetry = {
    "metrics": {"total": 0, "good": 0, "defect": 0, "moisture_reject": 0, "fps": "SIM"},
    "hardware": {"motorA": "STOPPED", "hopper": "STOPPED", "ir1": "CLEAR"},
    "environment": {"moisture": 0}
}

serial_connected = False
arduino = None
next_cocoon = None # 'GOOD', 'BAD', 'MOISTURE'

def serial_reader():
    global latest_telemetry, serial_connected, arduino, next_cocoon
    
    while True:
        if not serial_connected:
            try:
                arduino = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
                serial_connected = True
                print(f"\n[INFO] Successfully connected to Arduino on {COM_PORT} (SIMULATION MODE)")
                time.sleep(2)
                arduino.write(b"RESET\n")
            except serial.SerialException:
                serial_connected = False
                time.sleep(2)
                continue

        try:
            if arduino and arduino.in_waiting > 0:
                line = arduino.readline().decode('utf-8', errors='replace').strip()
                
                # Check for instant trigger from firmware_sim.ino
                if line == "TRIG":
                    print("[INFO] Instant TRIG received from Arduino (simulated IR block)")
                    
                    if next_cocoon == 'BAD':
                        print("[YOLO SIM] Replying DEFECT:YES")
                        arduino.write(b"DEFECT:YES\n")
                    else:
                        # For GOOD and MOISTURE, YOLO says NO DEFECT
                        print("[YOLO SIM] Replying DEFECT:NO")
                        arduino.write(b"DEFECT:NO\n")
                    
                    # Consume the queued cocoon
                    next_cocoon = None
                    continue
                
                if line.startswith('[DEBUG]'):
                    print(line)
                
                if line.startswith('{') and line.endswith('}'):
                    try:
                        parsed_data = json.loads(line)
                        for key, value in parsed_data.items():
                            if isinstance(value, dict) and key in latest_telemetry:
                                latest_telemetry[key].update(value)
                            else:
                                latest_telemetry[key] = value
                    except json.JSONDecodeError:
                        pass
        except serial.SerialException:
            serial_connected = False
            if arduino:
                arduino.close()
            arduino = None
            time.sleep(2)
        except Exception as e:
            time.sleep(1)

# Start serial thread
threading.Thread(target=serial_reader, daemon=True).start()

# --- Flask Endpoints ---

@app.route('/')
def index():
    return app.send_static_file('index.html')

def generate_mock_frames():
    # Simulate a 10 FPS camera stream for the dashboard
    while True:
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        
        cv2.putText(frame, "HARDWARE-IN-THE-LOOP SIMULATION", (60, 200), cv2.FONT_HERSHEY_SIMPLEX, 0.9, (0, 0, 255), 2)
        
        if next_cocoon:
            cv2.putText(frame, f"Queued: {next_cocoon}", (150, 260), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        else:
            cv2.putText(frame, f"Waiting for input...", (150, 260), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1)
        
        # Show real physical state
        hw_state = latest_telemetry["hardware"]
        state_str = f"M1: {hw_state['motorA']} | M2: {hw_state['hopper']} | IR: {hw_state['ir1']}"
        cv2.putText(frame, state_str, (80, 320), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 255, 255), 2)
        
        ret, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
        time.sleep(0.1)

@app.route('/video_feed')
def video_feed():
    return Response(generate_mock_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/api/telemetry')
def telemetry():
    # Append SIM fps
    latest_telemetry["metrics"]["fps"] = "SIM"
    return jsonify(latest_telemetry)

@app.route('/api/command', methods=['POST'])
def command():
    action = request.json.get('action', '')
    if serial_connected and arduino:
        if action == 'start':
            arduino.write(b"START\n")
            return jsonify({"status": "success"})
        elif action == 'stop':
            arduino.write(b"STOP\n")
            return jsonify({"status": "success"})
        elif action.startswith('SPEED:'):
            arduino.write(f"{action}\n".encode('utf-8'))
            return jsonify({"status": "success"})
    return jsonify({"status": "error", "message": "Serial not connected"}), 503

# --- Interactive CLI Loop ---

def interactive_loop():
    time.sleep(1) # Wait for startup prints
    print("\n" + "="*60)
    print(" HARDWARE-IN-THE-LOOP SIMULATOR ")
    print("="*60)
    print("Ensure you open the Dashboard at http://localhost:5000")
    print("Make sure firmware_sim.ino is uploaded to the Arduino.")
    print("Commands:")
    print("  1 - Insert GOOD Cocoon (Tests normal flow)")
    print("  2 - Insert BAD (Defect) Cocoon (Tests Servo 1)")
    print("  3 - Insert HIGH MOISTURE Cocoon (Tests Servo 2)")
    print("  q - Quit Simulator")
    print("="*60)
    
    global next_cocoon
    
    while True:
        cmd = input("\nEnter command (1/2/3/q): ").strip().lower()
        if cmd == 'q':
            print("Exiting simulator...")
            os._exit(0)
            
        if not serial_connected or not arduino:
            print(">>> ERROR: Arduino is not connected!")
            continue
            
        if cmd == '1':
            next_cocoon = 'GOOD'
            arduino.write(b"SIM:MOISTURE:10.0\n")
            arduino.write(b"SIM:DROP\n")
            print(">>> Queued GOOD cocoon -> Sending SIM:DROP to physical Arduino.")
        elif cmd == '2':
            next_cocoon = 'BAD'
            arduino.write(b"SIM:MOISTURE:10.0\n")
            arduino.write(b"SIM:DROP\n")
            print(">>> Queued BAD cocoon -> Sending SIM:DROP to physical Arduino.")
        elif cmd == '3':
            next_cocoon = 'MOISTURE'
            arduino.write(b"SIM:MOISTURE:15.5\n")
            arduino.write(b"SIM:DROP\n")
            print(">>> Queued MOISTURE cocoon -> Sending SIM:DROP to physical Arduino.")
        else:
            print("Invalid command. Use 1, 2, 3, or q.")

if __name__ == "__main__":
    import logging
    log = logging.getLogger('werkzeug')
    log.setLevel(logging.ERROR)
    
    cli_thread = threading.Thread(target=interactive_loop, daemon=True)
    cli_thread.start()
    
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)
