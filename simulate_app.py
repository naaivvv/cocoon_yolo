import time
import threading
import json
import os
import cv2
import numpy as np
from flask import Flask, jsonify, request, Response

app = Flask(__name__, static_folder='.', static_url_path='')

class MockHardware:
    def __init__(self):
        self.state = 'IDLE'
        self.system_active = False
        self.conveyor_running = False
        self.hopper_running = False
        self.ir1_state = "CLEAR" # CLEAR or BLOCKED
        
        self.total = 0
        self.good = 0
        self.defect = 0
        self.moisture_reject = 0
        
        self.moisture = 10.0
        self.servo1_angle = 180
        self.servo2_angle = 90
        
        self.state_timer = 0
        self.next_cocoon = None # 'GOOD', 'BAD', 'MOISTURE'
        
    def change_state(self, new_state):
        self.state = new_state
        self.state_timer = time.time()
        print(f"\n[HARDWARE SIM] State changed to: {new_state}")
        
    def process(self):
        if not self.system_active:
            return
            
        current_time = time.time()
        elapsed = current_time - self.state_timer
        
        if self.state == 'IDLE':
            # Waiting for START command (handled by /api/command)
            pass
            
        elif self.state == 'FEEDING':
            if not self.hopper_running and not self.conveyor_running:
                print("[HARDWARE SIM] Activating Hopper and Conveyor...")
            self.hopper_running = True
            self.conveyor_running = True
            
            # Note: We delay the change_state to MOVING_TO_CAM to give it some time in FEEDING visually
            if elapsed > 0.5:
                self.total += 1
                self.ir1_state = "CLEAR"
                self.change_state('MOVING_TO_CAM')
            
        elif self.state == 'MOVING_TO_CAM':
            if self.hopper_running and elapsed > 1.0:
                print("[HARDWARE SIM] Stopping Hopper...")
                self.hopper_running = False
                
            # If the user has queued a cocoon, wait until 0.25s then trigger IR1 (simulating new 250ms guard)
            if elapsed > 0.25 and self.ir1_state == "CLEAR":
                if self.next_cocoon:
                    print(f"[HARDWARE SIM] Cocoon detected by IR sensor! (Type: {self.next_cocoon})")
                    self.ir1_state = "BLOCKED"
                    self.hopper_running = False
                    self.conveyor_running = False
                    print("[HARDWARE SIM] Stopping Conveyor...")
                    self.change_state('WAITING_CAM_RESULT')
                
        elif self.state == 'WAITING_CAM_RESULT':
            # In simulation, we bypass YOLO and immediately resolve it instantly (simulating the new TRIG command)
            if elapsed > 0.05:
                if self.next_cocoon == 'BAD':
                    print("[YOLO SIM] Detected class: 'bad' -> Sending DEFECT:YES")
                    self.defect += 1
                    self.change_state('SORT_DEFECT')
                elif self.next_cocoon == 'MOISTURE':
                    print("[YOLO SIM] Detected class: 'good' -> Sending DEFECT:NO")
                    print("[HARDWARE SIM] High moisture detected!")
                    self.moisture = 15.5 # Simulate high moisture
                    self.moisture_reject += 1
                    self.change_state('SORT_MOISTURE')
                elif self.next_cocoon == 'GOOD':
                    print("[YOLO SIM] Detected class: 'good' -> Sending DEFECT:NO")
                    print("[HARDWARE SIM] Moisture OK.")
                    self.moisture = 10.0 # Simulate normal moisture
                    self.good += 1
                    self.change_state('MOVE_TO_END')
                
                self.next_cocoon = None # Consumed
                self.ir1_state = "CLEAR"
                
        elif self.state == 'SORT_DEFECT':
            if elapsed < 0.1:
                print("[HARDWARE SIM] Sweeping Servo 1: 180 -> 0 -> 180 (Rejecting Defect)")
            if elapsed > 1.5:
                self.change_state('FEEDING')
            
        elif self.state == 'SORT_MOISTURE':
            if elapsed < 0.1:
                print("[HARDWARE SIM] Servo 2 moving to 150 degrees (Blocking Moisture)")
                self.conveyor_running = True
            
            if elapsed > 2.0:
                print("[HARDWARE SIM] Servo 2 moving back to 180 degrees")
                self.conveyor_running = False
                self.change_state('FEEDING')
                
        elif self.state == 'MOVE_TO_END':
            if elapsed < 0.1:
                self.conveyor_running = True
                print("[HARDWARE SIM] Conveyor running to drop good cocoon at end...")
            if elapsed > 2.0:
                self.conveyor_running = False
                self.change_state('FEEDING')

hardware = MockHardware()

def hardware_loop():
    while True:
        hardware.process()
        time.sleep(0.1)

# Start hardware simulation thread
threading.Thread(target=hardware_loop, daemon=True).start()

# --- Flask Endpoints ---

@app.route('/')
def index():
    return app.send_static_file('index.html')

def generate_mock_frames():
    # Simulate a 10 FPS camera stream
    while True:
        frame = np.zeros((480, 640, 3), dtype=np.uint8)
        
        # Draw some text overlay
        cv2.putText(frame, "SIMULATION MODE", (150, 200), cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 2)
        
        if hardware.next_cocoon:
            cv2.putText(frame, f"Queued: {hardware.next_cocoon}", (150, 260), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
        
        cv2.putText(frame, f"State: {hardware.state}", (150, 320), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 255), 2)
        
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
    return jsonify({
        "metrics": {
            "total": hardware.total,
            "good": hardware.good,
            "defect": hardware.defect,
            "moisture_reject": hardware.moisture_reject,
            "fps": "10.0"
        },
        "hardware": {
            "motorA": "RUNNING" if hardware.conveyor_running else "STOPPED",
            "hopper": "RUNNING" if hardware.hopper_running else "STOPPED",
            "ir1": hardware.ir1_state
        },
        "environment": {
            "moisture": hardware.moisture
        }
    })

@app.route('/api/command', methods=['POST'])
def command():
    action = request.json.get('action', '')
    if action == 'start':
        hardware.system_active = True
        print("\n[API] Received START command")
        if hardware.state == 'IDLE':
            hardware.change_state('FEEDING')
        return jsonify({"status": "success", "message": "START sent"})
    elif action == 'stop':
        hardware.system_active = False
        hardware.conveyor_running = False
        hardware.hopper_running = False
        print("\n[API] Received STOP command")
        hardware.change_state('IDLE')
        return jsonify({"status": "success", "message": "STOP sent"})
    elif action.startswith('SPEED:'):
        print(f"\n[API] Received {action} command")
        return jsonify({"status": "success", "message": f"{action} sent"})
    return jsonify({"status": "error", "message": "Invalid action"}), 400

# --- Interactive CLI Loop ---

def interactive_loop():
    time.sleep(1) # Give Flask a moment to print its startup messages
    print("\n" + "="*50)
    print(" COCOON SORTING INTERACTIVE SIMULATOR ")
    print("="*50)
    print("Ensure you open the Dashboard at http://localhost:5000")
    print("Commands:")
    print("  1 - Insert GOOD Cocoon")
    print("  2 - Insert BAD (Defect) Cocoon")
    print("  3 - Insert HIGH MOISTURE Cocoon")
    print("  q - Quit Simulator")
    print("="*50)
    
    while True:
        cmd = input("\nEnter command (1/2/3/q): ").strip().lower()
        if cmd == 'q':
            print("Exiting simulator...")
            os._exit(0)
        elif cmd == '1':
            if not hardware.system_active:
                print(">>> System is STOPPED. Start from the dashboard first!")
            else:
                hardware.next_cocoon = 'GOOD'
                print(">>> Queued GOOD cocoon.")
        elif cmd == '2':
            if not hardware.system_active:
                print(">>> System is STOPPED. Start from the dashboard first!")
            else:
                hardware.next_cocoon = 'BAD'
                print(">>> Queued BAD cocoon.")
        elif cmd == '3':
            if not hardware.system_active:
                print(">>> System is STOPPED. Start from the dashboard first!")
            else:
                hardware.next_cocoon = 'MOISTURE'
                print(">>> Queued MOISTURE cocoon.")
        else:
            print("Invalid command. Use 1, 2, 3, or q.")

if __name__ == "__main__":
    import logging
    # Suppress werkzeug logging for cleaner interactive CLI
    log = logging.getLogger('werkzeug')
    log.setLevel(logging.ERROR)
    
    # Run interactive loop in a background thread so Flask can run on main thread
    cli_thread = threading.Thread(target=interactive_loop, daemon=True)
    cli_thread.start()
    
    # Run Flask
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)
