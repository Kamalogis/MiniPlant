import sqlite3

conn = sqlite3.connect('data_Monitor_WTP.db')
c = conn.cursor()
c.execute("SELECT * FROM DATABASE")
data = c.fetchall()
conn.close()
print(data)