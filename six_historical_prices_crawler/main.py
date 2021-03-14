import sys
import requests as req
import sqlite3 as sql

"""
    TODO
        - insert daily share value from file
        - fetch data from six historical data -> insert into table and somehow save progress in case of failure
            - 1. Try using monthly trade data, if not found
            - 2. Use daily reports until another monthly trade data is found
            - Insert only all data from a day, if done properly, it can easily be queried when the program last stopped
"""
def print_help():
    print("Usage: python3 main.py <ReferenceIn | ReferencesOut> <optional...>")
    print("ReferenceIn: Read file and insert share references into db. Optionals needs to be a path to file.")
    print("ReferencesOut: Print all share references. Optionals are Offset and Limit")

def create_conn():
    return sql.connect('data/market_data.db')

def insert_share_reference(conn, short_name, isin, isc, currency):
    stmt = 'INSERT INTO shares_reference (short_name, isin, isc, currency) VALUES(?, ?, ?, ?);'
    cur = conn.cursor()
    cur.execute(stmt, [short_name, isin, isc, currency])
    conn.commit()
    return cur.lastrowid

def insert_share_daily_value(conn, isin, date, closing_price, low_price, high_price):
    stmt = 'INSERT INTO daily_share_values (isin, date, closing_price, low_price, high_price) VALUES (?, ?, ?, ?, ?);'
    cur = conn.cursor()
    cur.execute(stmt, [isin, date, closing_price, low_price, high_price])
    conn.commit()
    return cur.lastrowid

def get_share_reference(conn, isin):
    stmt = 'SELECT * FROM shares_reference WHERE isin=?;'
    cur = conn.cursor()
    cur.execute(stmt, [isin])
    return cur.fetchone()

def list_share_references(conn, offset=0, limit=20):
    stmt = 'SELECT * FROM shares_reference LIMIT ? OFFSET ?;'
    cur = conn.cursor()
    cur.execute(stmt, [limit, offset])
    return cur.fetchall()

def parse_share_reference_file(conn, file_path):
    with open(file_path, 'r', encoding='latin1') as f:
        content = f.read()
        content = content.split('\n')[1:]
        for line in content:
            values = line.split(';')
            if len(values) > 12 and get_share_reference(conn, values[4]) is None:
                insert_share_reference(conn, values[0], values[4], values[12], values[8]) 

def parse_share_daily_price_file(conn, file_path):
    with open(file_path, 'r', encoding='latin1') as f:
       content = f.read()
       content = content.split('\n')[1:]
       for line in content:
           # TODO split data and insert, differentiate between monthly and daily reports
           pass

headers = { 'accept': 'application/json, text/plain, */*', 
            'Referer': 'https://www.six-group.com/en/products-services/the-swiss-stock-exchange/market-data/statistics/historical-prices.html',
            'User-Agent': 'Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.152 Safari/537.36' 
            }

# response = req.get('https://www.six-group.com/itf/sheldon/sido/ajax/historical_prices/v1/download/swiss_blue_chip_shares_2021-03-10.csv')

# if response.status_code == 200:
#   print("Response successful")
# else:
#    print("Error fetching document")

if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] == "ReferenceIn" and len(sys.argv) > 2:
            conn = create_conn()
            parse_share_reference_file(conn, sys.argv[2]) 
        elif sys.argv[1] == "ReferencesOut" and len(sys.argv) > 3:
            conn = create_conn()
            data = list_share_references(conn, int(sys.argv[2]), int(sys.argv[3]))
            print('\t|\t'.join(["id", "ShortName", "ISIN", "ISC", "Currency"]))
            for row in data:
                print('\t|\t'.join([str(x) for x in row]))
        else:
            print_help()
    else:
        print_help() 
        
