from sys import argv
import os
import requests as req
from datetime import datetime

print_debug = True

def debug(*args):
    global print_debug
    if print_debug:
        print(*args)


def print_help():
    print("Usage: python six_crawler.py <folder of data>")

def find_last_date(folder_list: list, file_prefix:str, file_len: int, date_start:int, date_end:int, date_format:str) -> datetime:
    last_date = None
    for file in folder_list:
        if file_prefix in file and len(file) == file_len:
            file_date = file[date_start:date_end]
            parsed_date = datetime.strptime(file_date, date_format)
            if last_date is None or parsed_date < last_date:
                last_date = parsed_date
    return last_date


def get_progress(folder: str) -> datetime:
    folder = [f for f in os.listdir(folder) if os.path.isfile(os.path.join(folder, f))]
    if len(folder) == 0:
        debug("Starting with empty directory.")
        return datetime.today()
    else:
        folder.sort()
        last_month = find_last_date(folder, 'monthly-trade-data-', 29, 19, 25, '%Y%m')
        last_blue_chip = find_last_date(folder, 'swiss_blue_chip_shares_', 37, 23, 33, '%Y-%m-%d')
        last_small_and_mid = find_last_date(folder, 'mid_and_small_caps_swiss_shares_', 46, 32, 42, '%Y-%m-%d')
        debug(last_month)
        debug(last_blue_chip)
        debug(last_small_and_mid)

if __name__ == '__main__':
    if len(argv) < 2:
        print_help()
    else:
        folder = argv[1]
        get_progress(folder)
