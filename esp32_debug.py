import urllib.request
try:
    urllib.request.urlopen("http://192.168.178.71/stream", timeout=5)
    print("Stream reachable!")
except Exception as e:
    print(f"Failed: {e}")