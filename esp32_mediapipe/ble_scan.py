import asyncio
from bleak import BleakScanner

async def scan():
    print("Scanning for BLE devices for 5 seconds...")
    devices = await BleakScanner.discover(timeout=5.0)
    for d in devices:
        print(f"  {d.address}  —  {d.name}")

asyncio.run(scan())