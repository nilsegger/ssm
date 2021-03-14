import sqlite3 as sql

"""
    id, short_name, isin (international securities identificaiton code), isc (industry sector code), currency 
"""

table_create = """
    CREATE TABLE shares_reference (
        id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
        short_name TEXT NOT NULL,
        isin TEXT UNIQUE NOT NULL,
        isc INTEGER NOT NULL,
        currency TEXT
    );
"""

conn = sql.connect('data/market_data.db')

with conn:
    conn.execute(table_create)
