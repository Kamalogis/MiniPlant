from flask import Flask, render_template, request
from flask_socketio import SocketIO, emit
import sqlite3

app = Flask(__name__)
socketio = SocketIO(app)

def ambil_data():
    conn = sqlite3.connect('data_wtp.db')
    conn.row_factory = sqlite3.Row
    c = conn.cursor()
    c.execute("SELECT * FROM monitor_wtp ORDER BY id DESC LIMIT 1")
    row = c.fetchone()
    conn.close()
    return dict(row) if row else None

def poll_data():
    Last_id = None
    while True:
        Last = ambil_data()
        if Last_id != Last["id"]:
            Last_id = Last['id']
            Last["level_1"] = round((Last["level_1"]/100) * 100) #Asumsi tinggi tangki 100cm
            Last["level_2"] = round((Last["level_2"]/80) * 100) # Asumsi tinggi tanki 80cm
            Data = {
                "id" : Last["id"],
                "timestamp" : Last["timestamp"],
                "level1" : Last["level_1"],
                "level2" : Last["level_2"],
                "tdsValue" : Last["tds_1"],
                "flowRate" : Last["flow_1"],
                "pressureValue" : Last["pressure_1"],
                "levelSwitch" : Last["level_switch"],
                "mode_standby" : Last["mode_standby"],
                "mode_filtering" : Last["mode_filtering"],
                "mode_backwash" : Last["mode_backwash"],
                "mode_drain" : Last["mode_drain"],
                "mode_override" : Last["mode_override"],
                "emergency_stop" : Last["emergency_stop"],
                "solenoid1" : Last["solenoid_1"],
                "solenoid2" : Last["solenoid_2"],
                "solenoid3" : Last["solenoid_3"],
                "solenoid4" : Last["solenoid_4"],
                "solenoid5" : Last["solenoid_5"],
                "solenoid6" : Last["solenoid_6"],
                "pump1" : Last["pompa_1"],
                "pump2" : Last["pompa_2"],
                "pump3" : Last["pompa_3"]
            }
            socketio.emit("data_monitor", Data)
            print("Berhasil Poll")
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
    socketio.run(app, debug=True)
