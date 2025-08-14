import time
import sqlite3
from datetime import datetime
import random

DB_FILE = "data_Monitor_WTP.db"
def generate_number_naik(n):
    while True:
        # Naik
        for i in range(0, n+1):
            yield i
            time.sleep(0.05)
        # Turun
        for i in range(n, -1, -1):
            yield i
            time.sleep(0.05)

def simulator():
    """Simulasikan penambahan data baru ke DB setiap detik"""
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    while True:
        status = random.choice([True, False])
        status2 = random.choice([True, False])
        status3 = random.choice([True, False])
        status4 = random.choice([True, False])
        value1 = random.uniform(0, 100)
        value2 = random.uniform(0,80)
        c.execute("""INSERT INTO database (
        timestamp,
        level1,
        level2,
        tdsValue,
        flowRate,
        pressureValue,
        levelSwitch,
        mode_standby,
        mode_filtering,
        mode_backwash,
        mode_drain,
        mode_override,
        emergency_stop,
        solenoid1,
        solenoid2,
        solenoid3,
        solenoid4,
        solenoid5,
        solenoid6,
        pump1,
        pump2,
        pump3
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,?, ?, ?, ?, ?, ?, ?, ?, ?, ?)""",
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
            status4))
        conn.commit()
        print(f"[Simulator] Data baru: {value1} {value2} , Bool: {status}")
        time.sleep(0.5)

if __name__ == "__main__":
    simulator()
