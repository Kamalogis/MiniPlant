import sqlite3

# Buat Database
conn = sqlite3.connect("data_wtp.db")
c = conn.cursor()
c.execute("""
CREATE TABLE IF NOT EXISTS monitor_wtp (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT,
        level_1 INTEGER,
        level_2 INTEGER,
        tds_1 INTEGER,
        flow_1 INTEGER,
        pressure_1 INTEGER,
        level_switch INTEGER,
        mode_standby INTEGER,
        mode_filtering INTEGER,
        mode_backwash INTEGER,
        mode_drain INTEGER,
        mode_override INTEGER,
        emergency_stop INTEGER,
        solenoid_1 INTEGER,
        solenoid_2 INTEGER,
        solenoid_3 INTEGER,
        solenoid_4 INTEGER,
        solenoid_5 INTEGER,
        solenoid_6 INTEGER,
        pompa_1 INTEGER,
        pompa_2 INTEGER,
        pompa_3 INTEGER,
        stepper INTEGER
);
""")
conn.commit()
conn.close()