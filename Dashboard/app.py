from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
import sqlite3

app = Flask(__name__)
socketio = SocketIO(app)

def ambil_data():
    conn = sqlite3.connect('data_Monitor_WTP.db')
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    c.execute("SELECT * FROM Database ORDER BY id DESC LIMIT 1")
    row = c.fetchone()
    conn.close()
    return dict(row) if row else None

def poll_data():
    Last_id = None
    while True:
        Last = ambil_data()
        if Last_id != Last["id"]:
            Last_id = Last['id']
            Last["level1"] = round((Last["level1"]/100) * 100) #Asumsi tinggi tangki 100cm
            Last["level2"] = round((Last["level2"]/80) * 100) # Asumsi tinggi tanki 80cm
            Data = {
                "id" : Last["id"],
                "timestamp" : Last["timestamp"],
                "level1" : Last["level1"],
                "level2" : Last["level2"],
                "tdsValue" : Last["tdsValue"],
                "flowRate" : Last["flowRate"],
                "pressureValue" : Last["pressureValue"],
                "levelSwitch" : Last["levelSwitch"],
                "mode_standby" : Last["mode_standby"],
                "mode_filtering" : Last["mode_filtering"],
                "mode_backwash" : Last["mode_backwash"],
                "mode_drain" : Last["mode_drain"],
                "mode_override" : Last["mode_override"],
                "emergency_stop" : Last["emergency_stop"],
                "solenoid1" : Last["solenoid1"],
                "solenoid2" : Last["solenoid2"],
                "solenoid3" : Last["solenoid3"],
                "solenoid4" : Last["solenoid4"],
                "solenoid5" : Last["solenoid5"],
                "solenoid6" : Last["solenoid6"],
                "pump1" : Last["pump1"],
                "pump2" : Last["pump2"],
                "pump3" : Last["pump3"]
            }
            socketio.emit("data_monitor", Data)
        socketio.sleep(0.5)


@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_monitor():
    socketio.start_background_task(poll_data)

@socketio.on('emergency')
def monitor_WTP(data):
    if data.get('status') == 'emergency':
        print("Emergency Ditekan") # Ganti Logika Emergency
    else:
        None

if __name__ == '__main__':
    socketio.run(app, debug=True, host="0.0.0.0")
