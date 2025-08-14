import sqlite3
import pandas as pd

conn = sqlite3.connect('data_Monitor_WTP.db')

query = f"SELECT * FROM DATABASE"

df = pd.read_sql_query(query,conn)

conn.close()

print(df)