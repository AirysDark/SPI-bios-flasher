1) Arduino firmware (flash this to your Uno)

Board: Arduino Uno

Port: your COM/tty

Baud: 115200

Requires only the built-in SPI library

⚠️ Power/logic must be 3.3 V. Tie WP# (pin 3) and HOLD# (pin 7) to 3.3 V.

2) PC-side script (Python 3) — read / write / erase

Save  your Biso .bin files next to the spiflash_tool.py.

3) Wiring (quick recap)

D13 → CLK, D12 → DO, D11 → DI, D10 → CS

3.3 V → VCC (pin 8), GND → GND (pin 4)

3.3 V → WP# (pin 3) and 3.3 V → HOLD# (pin 7)


Use a SOIC-8 clip. If your Uno is 5 V logic, avoid directly driving the flash at 5 V. Ideally use a level shifter or a 3.3 V Arduino Pro Mini + FTDI.


---

4) Typical commands

Identify chip

python spiflash_tool.py --port /dev/ttyUSB0 id
# or on Windows:
python spiflash_tool.py --port COM5 id

Make a full backup (the 25Q256 is 32 MiB = 33554432 bytes)

python spiflash_tool.py --port /dev/ttyUSB0 read 0 33554432 backup.bin

Erase (optional, but recommended before a full-chip write)

python spiflash_tool.py --port /dev/ttyUSB0 erase

Write your unlocked dump

python spiflash_tool.py --port /dev/ttyUSB0 write 0 unlocked_backup.bin


---

Safety notes (important)

Back up first (always keep backup.bin safe).

Keep the laptop fully powered off and battery disconnected while clipping the chip.

Ensure solid clip contact — marginal contact causes random verify errors.

If a full-chip write is overkill, you can selectively write ranges (split your .bin) — the tool supports arbitrary addr+data size.

This bridge runs SPI at ~2 MHz for reliability with a clip. You can bump it if your wiring is short/solid.
