import sys, cv2, serial, numpy as np
import serial.tools.list_ports

W = H = 96
COUNT = W * H
BAUD = 115200

def find_port():
    ports = serial.tools.list_ports.comports()
    for p in ports:
        d = (p.description or "").lower(); m = (p.manufacturer or "").lower()
        if any(k in d or k in m for k in ("arduino","nano","genuino","ch340","ftdi")):
            return p.device
    if ports: return ports[0].device
    raise RuntimeError("No serial port found")

port = sys.argv[1] if len(sys.argv) > 1 else find_port()
print(f"Connecting to {port}")
ser = serial.Serial(port, BAUD, timeout=2)
ser.reset_input_buffer()

def read_exact(n):
    buf = b""
    while len(buf) < n:
        c = ser.read(n - len(buf))
        if not c: return None
        buf += c
    return buf

while True:
    if ser.read(1) != b'\xAA': continue
    if ser.read(1) != b'\x55': continue
    cnt = read_exact(4)
    if cnt is None or int.from_bytes(cnt, "little") != COUNT: continue
    mask = read_exact(COUNT)
    if mask is None: continue
    ln = read_exact(1)
    if ln is None: continue
    label = read_exact(ln[0]).decode(errors="ignore")
    cf = read_exact(4)
    if cf is None: continue
    conf = float(np.frombuffer(cf, dtype=np.float32)[0])

    img  = np.frombuffer(mask, dtype=np.uint8).reshape((H, W))
    disp = cv2.cvtColor(cv2.resize(img,(384,384),interpolation=cv2.INTER_NEAREST),
                        cv2.COLOR_GRAY2BGR)
    cv2.rectangle(disp,(0,0),(384,42),(0,0,0),-1)
    cv2.putText(disp,f"{label}  {conf*100:.0f}%",(10,30),
                cv2.FONT_HERSHEY_SIMPLEX,0.8,(0,255,0),2)
    cv2.imshow("Nano model view",disp)
    if cv2.waitKey(1) == 27: break

cv2.destroyAllWindows()