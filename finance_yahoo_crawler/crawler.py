import time
import random
import requests

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

def download(url: str, file:str, folder: str):
    time.sleep(float(random.randint(5000, 9999)) / 10000.0)
    headers = {
                ":authority": "query1.finance.yahoo.com",
                ":method": "GET",
                ":path": "/v7/finance/download/GEBN.SW?period1=922665600&period2=1617321600&interval=1d&events=history&includeAdjustedClose=true",
                ":scheme": "https",
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.9",
                "Accept-Encoding": "gzip, deflate, br",
                "Accept-Language": "en-GB,en-US;q=0.9,en;q=0.8",
                "Cache-Control": "no-cache",
                "Connection": "keep-alive",
                "Host": "www.six-group.com",
                "Pragma": "no-cache",
                "Referer": "https://finance.yahoo.com/",
                "Sec-Fetch-Dest": "document",
                "Sec-Fetch-Mode": "navigate",
                "Sec-Fetch-Site": "same-site",
                "Sec-Fetch-User": "?1",
                "Sec-GPC": "1",
                "Upgrade-Insecure-Requests": "1",
                "User-Agent": random.choice(user_agents)
                } 
    r = requests.get(url)
    debug(url, "->", r.status_code)
    print(r.text)
    #if r.status_code == 200:
        #with open(os.path.join(folder, file), "w+") as f:
        #    f.write(r.text)
    return r.status_code

if __name__ == '__main__':
    download("https://query1.finance.yahoo.com/v7/finance/download/GEBN.SW?period1=922665600&period2=1617321600&interval=1d&events=history&includeAdjustedClose=true", "", "")
