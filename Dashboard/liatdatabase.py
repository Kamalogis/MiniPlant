import sqlite3
import pandas as pd
conn = sqlite3.connect('data_wtp.db')
query = f"SELECT * FROM monitor_wtp"
df = pd.read_sql_query(query, conn)
df.to_csv("monitor_wtp.csv", index=False)
conn.close()
print(df.tail())