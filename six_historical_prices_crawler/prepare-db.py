import sqlite3 as sql
import sys

"""
    id, short_name, isin (international securities identificaiton code), isc (industry sector code), currency 
"""

shares_reference_table = """
    CREATE TABLE shares_reference (
        id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
        short_name TEXT NOT NULL,
        isin TEXT UNIQUE NOT NULL,
        isc INTEGER NOT NULL,
        currency TEXT
    );
"""

"""
    id, isin,  date, closing_price, daily_low_price, daily_high_price
"""
daily_share_values_table = """
    CREATE TABLE daily_share_values (
        id INTEGER NOT NULL PRIMARY KEY AUTOINCREMENT,
        isin TEXT NOT NULL,
        date INTEGER NOT NULL,
        closing_price REAL NOT NULL,
        low_price REAL NOT NULL,
        high_price REAL NOT NULL,
        FOREIGN KEY(isin) REFERENCES shares_reference(isin)
    );
"""




def create_database(conn):
    with conn:
        conn.execute(shares_reference_table)
        conn.execute(daily_share_values_table)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python prepare-db.py <database>")
    else:
        conn = sql.connect(sys.argv[1])
        create_database(conn)
