import time
import sqlite3
from datetime import datetime
import random

DB_FILE = "data_wtp.db"

def simulator():
    """Simulasikan penambahan data baru ke DB setiap detik"""
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    while True:
        status = random.choice([True, False])
        status2 = random.choice([True, False])
        status3 = random.choice([True, False])
        status4 = random.choice([True, False])
        value1 = random.uniform(0,100)
        value2 = random.uniform(0,80)
        c.execute("""INSERT INTO monitor_wtp (
            timestamp,
            level_1,
            level_2,
            tds_1,
            flow_1,
            pressure_1,
            level_switch,
            mode_standby,
            mode_filtering,
            mode_backwash,
            mode_drain,
            mode_override,
            emergency_stop,
            solenoid_1,
            solenoid_2,
            solenoid_3,
            solenoid_4,
            solenoid_5,
            solenoid_6,
            pompa_1,
            pompa_2,
            pompa_3,
            stepper
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
        (
        datetime.now().strftime("%Y-%m-%d %H:%M:%S"), 
        value1, 
        value2, 
        value1, 
        value1, 
        value1, 
        status, 
        status2, 
        status3, 
        status4, 
        status, 
        status2, 
        status3, 
        status4, 
        status, 
        status2, 
        status3, 
        status4, 
        status, 
        status2, 
        status3, 
        status4,
        status4))
        conn.commit()
        print(f"[Simulator] Data baru: {value1} {value2} , Bool: {status}")
        time.sleep(0.5)

if __name__ == "__main__":
    simulator()
