# spiflash_tool.py - Host tool for SPIFlashBridge.ino
# Usage examples:
#   python spiflash_tool.py --port COM5 id
#   python spiflash_tool.py --port COM5 read 0 33554432 backup.bin    # 32 MiB read
#   python spiflash_tool.py --port COM5 erase
#   python spiflash_tool.py --port COM5 write 0 unlocked_backup.bin

import argparse, sys, time, struct
import serial

BAUD = 115200

def ser_open(port):
    s = serial.Serial(port, BAUD, timeout=2)
    time.sleep(0.5)
    return s

def cmd_id(s):
    s.write(b'I')
    resp = s.readline().decode('ascii', errors='ignore').strip()
    print("FW:", resp)
    s.write(b'S')
    jedec = s.read(3)
    print("JEDEC ID:", jedec.hex())
    return jedec

def u32(n): return struct.pack('<I', n)

def cmd_read(s, addr, length, out_path, fast=False):
    s.write(b'B' if fast else b'R')
    s.write(u32(addr))
    s.write(u32(length))
    with open(out_path, 'wb') as f:
        remaining = length
        CHUNK = 4096
        while remaining:
            chunk = s.read(min(CHUNK, remaining))
            if not chunk:
                print("Read timeout; aborting")
                sys.exit(1)
            f.write(chunk)
            remaining -= len(chunk)
            print(f"\rRead {length-remaining}/{length} bytes", end='')
    print("\nDone.")

def cmd_erase(s):
    print("Chip erase… this can take a few minutes.")
    s.write(b'E')
    a = s.read(1)
    if a != b'K':
        print("Erase failed / no ACK")
        sys.exit(1)
    print("Erase complete.")

def cmd_write(s, addr, in_path):
    data = open(in_path, 'rb').read()
    print(f"Writing {len(data)} bytes starting @ 0x{addr:06X}")
    s.write(b'W')
    s.write(u32(addr))
    s.write(u32(len(data)))
    # stream data; the bridge will page-split
    s.write(data)
    a = s.read(1)
    if a != b'K':
        print("Write failed / no ACK")
        sys.exit(1)
    print("Write complete.")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', required=True, help='Serial port (e.g., COM5 or /dev/ttyUSB0)')
    ap.add_argument('op', choices=['id','read','write','erase'])
    ap.add_argument('args', nargs='*')
    A = ap.parse_args()

    s = ser_open(A.port)
    if A.op == 'id':
        cmd_id(s)
    elif A.op == 'read':
        if len(A.args) != 3:
            print("read needs: <addr> <len> <out.bin>")
            sys.exit(1)
        addr = int(A.args[0], 0)
        length = int(A.args[1], 0)
        outp = A.args[2]
        cmd_id(s)
        cmd_read(s, addr, length, outp, fast=True)
    elif A.op == 'erase':
        cmd_id(s)
        cmd_erase(s)
    elif A.op == 'write':
        if len(A.args) != 2:
            print("write needs: <addr> <in.bin>")
            sys.exit(1)
        addr = int(A.args[0], 0)
        inp = A.args[1]
        cmd_id(s)
        cmd_write(s, addr, inp)

if __name__ == '__main__':
    main()