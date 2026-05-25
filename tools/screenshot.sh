#!/usr/bin/env bash
# Boot NexusOS headless and capture a framebuffer screenshot.
# Logs everything to build/shot.log and leaves a PNG at build/screen.png.
# Does not block on QEMU (uses timeout). Run from anywhere: bash tools/screenshot.sh
set -u
cd "$(dirname "$0")/.."
exec >build/shot.log 2>&1

SOCK=/tmp/qmon.sock
OUT_PPM=/tmp/nexus_screen.ppm
OUT_PNG=build/screen.png

pkill -9 -f qemu-system 2>/dev/null
sleep 1
rm -f "$SOCK" "$OUT_PPM" "$OUT_PNG" /tmp/serial.log

timeout 12 qemu-system-x86_64 \
  -drive format=raw,file=build/os.bin,if=ide,index=0 \
  -drive format=raw,file=build/fat.img,if=ide,index=1 \
  -m 256M -serial file:/tmp/serial.log -display none \
  -monitor unix:"$SOCK",server,nowait &
disown

for i in $(seq 1 60); do [ -S "$SOCK" ] && break; sleep 0.1; done
sleep 3

python3 - "$SOCK" "$OUT_PPM" "${1:-}" <<'PY'
import socket, sys, time
sock, out = sys.argv[1], sys.argv[2]
keys = sys.argv[3] if len(sys.argv) > 3 else ""
s = socket.socket(socket.AF_UNIX); s.connect(sock); time.sleep(0.3)
try: s.recv(8192)
except Exception: pass

# Optionally type a command (via QEMU PS/2 sendkey) before the screenshot.
named = {" ": "spc", ".": "dot", "-": "minus", "/": "slash", "\n": "ret"}
def sendkey(name):
    s.sendall(b"sendkey " + name.encode() + b"\n"); time.sleep(0.12)
for ch in keys:
    if ch.isalnum():       sendkey(ch.lower())
    elif ch in named:      sendkey(named[ch])
if keys:
    sendkey("ret"); time.sleep(0.5)

s.sendall(b"screendump " + out.encode() + b"\n")
time.sleep(1.5)
print("MONITOR:", s.recv(8192).decode(errors="replace")[:200])
s.close()
PY

sleep 0.5
if [ -f "$OUT_PPM" ]; then
  echo "PPM: $(ls -l "$OUT_PPM")"
  python3 - "$OUT_PPM" "$OUT_PNG" <<'PY'
import sys, struct, zlib
ppm, png = sys.argv[1], sys.argv[2]
data = open(ppm,'rb').read()
def tokens(buf):
    i=0; out=[]
    while len(out)<4:
        while i<len(buf) and buf[i:i+1].isspace(): i+=1
        if buf[i:i+1]==b'#':
            while i<len(buf) and buf[i:i+1] not in (b'\n',b'\r'): i+=1
            continue
        j=i
        while j<len(buf) and not buf[j:j+1].isspace(): j+=1
        out.append(buf[i:j]); i=j
    return out, i+1
toks, off = tokens(data)
w, h = int(toks[1]), int(toks[2])
pix = data[off:off+w*h*3]
def chunk(t,d):
    c=t+d; return struct.pack('>I',len(d))+c+struct.pack('>I',zlib.crc32(c)&0xffffffff)
raw=bytearray()
for y in range(h):
    raw.append(0); raw += pix[y*w*3:(y+1)*w*3]
out=b'\x89PNG\r\n\x1a\n'
out+=chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,2,0,0,0))
out+=chunk(b'IDAT',zlib.compress(bytes(raw),6))
out+=chunk(b'IEND',b'')
open(png,'wb').write(out)
print("PNG:", png, w, "x", h)
PY
else
  echo "NO PPM PRODUCED"
fi
grep "\[fb\]" /tmp/serial.log || echo "no fb line"
ls -l "$OUT_PNG" 2>/dev/null || echo "NO PNG"
echo "DONE"
