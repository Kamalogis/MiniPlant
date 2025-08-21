import sqlite3

# Buat Database
conn = sqlite3.connect("data_Monitor_WTP.db")
c = conn.cursor()
c.execute("""
CREATE TABLE IF NOT EXISTS Database (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT,
    level1 INTEGER,
    level2 INTEGER,
    tdsValue INTEGER,
    flowRate INTEGER,
    pressureValue INTEGER,
    levelSwitch INTEGER,
    mode_standby INTEGER,
    mode_filtering INTEGER,
    mode_backwash INTEGER,
    mode_drain INTEGER,
    mode_override INTEGER,
    emergency_stop INTEGER,
    solenoid1 INTEGER,
    solenoid2 INTEGER,
    solenoid3 INTEGER,
    solenoid4 INTEGER,
    solenoid5 INTEGER,
    solenoid6 INTEGER,
    pump1 INTEGER,
    pump2 INTEGER,
    pump3 INTEGER
);
""")
conn.commit()
conn.close()