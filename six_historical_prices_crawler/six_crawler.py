from sys import argv
from enum import Enum
import os
import requests as req
from datetime import datetime, timedelta
import calendar
import random
import time

print_debug = True

user_agents = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.152 Safari/537.36 Edge/12.246",
    "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:15.0) Gecko/20100101 Firefox/15.0.1",
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/88.0.4324.152 Safari/537.36",
    "Mozilla/5.0 (iPhone; CPU iPhone OS 12_0 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/12.0 Mobile/15E148 Safari/604.1",
    "Mozilla/5.0 (Linux; Android 8.0.0; SM-G960F Build/R16NW) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/62.0.3202.84 Mobile Safari/537.36"
]

def debug(*args):
    global print_debug
    if print_debug:
        print(*args)

def print_help():
    print("Usage: python six_crawler.py <folder of data> <finish date(dd-mm-yyyy)>")

def find_last_date(folder_list: list, file_prefix:str, file_len: int, date_start:int, date_end:int, date_format:str) -> datetime:
    last_date = None
    for file in folder_list:
        if file_prefix in file and len(file) == file_len:
            file_date = file[date_start:date_end]
            parsed_date = datetime.strptime(file_date, date_format)
            if last_date is None or parsed_date < last_date:
                last_date = parsed_date
    if last_date is None:
        last_date = datetime.today()
    return last_date


def get_progress(folder: str):
    folder = [f for f in os.listdir(folder) if os.path.isfile(os.path.join(folder, f))]
    if len(folder) == 0:
        debug("Starting with empty directory.")
        return datetime.today()
    else:
        folder.sort()
        last_month = find_last_date(folder, 'monthly-trade-data-', 29, 19, 25, '%Y%m')
        last_blue_chip = find_last_date(folder, 'swiss_blue_chip_shares_', 37, 23, 33, '%Y-%m-%d')
        last_small_and_mid = find_last_date(folder, 'mid_and_small_caps_swiss_shares_', 46, 32, 42, '%Y-%m-%d')
        if last_month < last_blue_chip and last_month < last_small_and_mid:
            return last_month
        else:
            return last_blue_chip if last_blue_chip >= last_small_and_mid else last_small_and_mid

def download(url: str, referer: str, file:str, folder: str):
    time.sleep(float(random.randint(1, 9999)) / 10000.0)
    headers = {"Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9",
                "Accept-Encoding": "gzip, deflate, br",
                "Accept-Language": "en-GB,en-US;q=0.9,en;q=0.8",
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "Host": "www.six-group.com",
                "Pragma": "no-cache",
                "Referer": referer,
                "Sec-Fetch-Dest": "document",
                "Sec-Fetch-Mode": "navigate",
                "Sec-Fetch-Site": "same-origin",
                "Sec-Fetch-User": "?1",
                "Sec-GPC": "1",
                "Upgrade-Insecure-Requests": "1",
                "User-Agent": random.choice(user_agents)
                } 
    r = req.get(url)
    debug(url, "->", r.status_code)
    if r.status_code == 200:
        with open(os.path.join(folder, file), "w+") as f:
            f.write(r.text)
    return r.status_code

def download_monthly_trade_data(date: datetime, folder: str):
    file = "monthly-trade-data-" + str(date.year) + str(date.month).zfill(2)+ ".csv"
    url = "https://www.six-group.com/dam/download/market-data/statistics/monthly-report/mtd/" + str(date.year) + "/" + file
    return download(url, "https://www.six-group.com/en/products-services/the-swiss-stock-exchange/market-data/statistics/monthly-reports.html", file, folder)

def download_daily(date_start: datetime, date_finish:datetime, folder: str):
    date = date_start
    while date >= date_finish:
        if date.weekday() < 5:
            blue_chip_file = "swiss_blue_chip_shares_" + str(date.year) + "-" + str(date.month).zfill(2) + "-" + str(date.day).zfill(2) + ".csv"
            url_blue_chip = "https://www.six-group.com/sheldon/historical_prices/v1/download/" + blue_chip_file
            blue_chip_res = download(url_blue_chip, "https://www.six-group.com/en/products-services/the-swiss-stock-exchange/market-data/statistics/historical-prices.html", blue_chip_file, folder)

            sm_file = "mid_and_small_caps_swiss_shares_" + str(date.year) + "-" + str(date.month).zfill(2) + "-" + str(date.day).zfill(2) + ".csv"
            url_sm = "https://www.six-group.com/sheldon/historical_prices/v1/download/" + sm_file 
            sm_res = download(url_sm, "https://www.six-group.com/en/products-services/the-swiss-stock-exchange/market-data/statistics/historical-prices.html", sm_file, folder)

            if sm_res != 200 or blue_chip_res != 200:
                debug("Failed to download", date)
        date = date - timedelta(days=1) 

def start_download_process(finish: datetime, folder: str):
    date = get_progress(folder)
    while date is not None and  date > finish:
        debug("Last date was:", date)
        if date.day > 1:
            download_daily(date - timedelta(days=1), datetime(date.year, date.month, 1), folder)
        if date.month > 1:
            date = datetime(date.year, date.month - 1, 1)
        else:
            date = datetime(date.year - 1, 12, 1)
        debug("Next date is:", date)
        if download_monthly_trade_data(date, folder) != 200:
            download_daily(datetime(date.year, date.month, calendar.monthrange(date.year, date.month)[1]), date, folder)
    debug("Done downloading data.")
        
if __name__ == '__main__':
    if len(argv) < 3:
        print_help()
    else:
        folder = argv[1]
        finish_date = datetime.strptime(argv[2], "%d-%m-%Y")
        start_download_process(finish_date, folder)
        




