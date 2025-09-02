// SPIFlashBridge.ino — Minimal SPI<->Serial bridge for 25-series SPI flash
// Works with a simple PC-side Python script to read/write whole chips.
// Target: Arduino Uno @ 115200 baud
//
// Wiring to SOIC-8 (25Q256):
//   1 CS#   -> D10
//   2 DO    -> D12 (MISO)
//   3 WP#   -> 3.3V
//   4 GND   -> GND
//   5 DI    -> D11 (MOSI)
//   6 CLK   -> D13 (SCK)
//   7 HOLD# -> 3.3V
//   8 VCC   -> 3.3V  (NEVER 5V)
//
// Protocol (host -> Arduino over serial, little-endian sizes):
//   'I'                    : identify (returns "SPIBRIDGE1\n")
//   'R' addr(4) len(4)     : read len bytes starting at addr (JEDEC READ 0x03)
//   'W' addr(4) len(4)     : page program len bytes starting at addr (0x02)
//                            host must send exactly len data bytes after header
//   'E'                    : chip erase (0xC7 / 0x60), blocking wait until ready
//   'S'                    : read JEDEC ID (0x9F), returns 3 bytes
//   'B'                    : bulk read (fast read 0x0B) like 'R' but uses 0x0B
//
// Notes:
// - W: breaks writes on 256-byte page boundaries automatically.
// - Before every write or erase, code issues WREN (0x06).
// - Waits for WIP=0 by polling RDSR (0x05).
// - No wear-level magic; for big writes you may want to erase first (E).
//
// This is intentionally small and robust for Uno.

#include <SPI.h>

static const uint8_t PIN_CS = 10;
static const uint32_t BAUD = 115200;

static inline void cs_low()  { digitalWrite(PIN_CS, LOW);  }
static inline void cs_high() { digitalWrite(PIN_CS, HIGH); }

uint8_t spi_xfer(uint8_t v) { return SPI.transfer(v); }

void send_ok()   { Serial.write('K'); }       // 'K' = OK
void send_err()  { Serial.write('!'); }       // '!' = error

void wren() {
  cs_low();
  spi_xfer(0x06); // WREN
  cs_high();
}

uint8_t rdsr() {
  cs_low();
  spi_xfer(0x05); // RDSR
  uint8_t v = spi_xfer(0xFF);
  cs_high();
  return v;
}

void wait_wip_clear() {
  // Wait until WIP bit (bit0) clears
  while (rdsr() & 0x01) { /* spin */ }
}

void read_bytes(uint32_t addr, uint32_t len, bool fast) {
  cs_low();
  spi_xfer(fast ? 0x0B : 0x03); // 0x03 READ, 0x0B FAST READ
  spi_xfer((addr >> 16) & 0xFF);
  spi_xfer((addr >> 8)  & 0xFF);
  spi_xfer(addr & 0xFF);
  if (fast) spi_xfer(0x00); // dummy byte for 0x0B
  for (uint32_t i = 0; i < len; i++) {
    Serial.write(spi_xfer(0xFF));
  }
  cs_high();
}

void page_program(uint32_t addr, const uint8_t* buf, uint32_t len) {
  // len <= 256 and must not cross page boundary
  wren();
  cs_low();
  spi_xfer(0x02); // PP
  spi_xfer((addr >> 16) & 0xFF);
  spi_xfer((addr >> 8)  & 0xFF);
  spi_xfer(addr & 0xFF);
  for (uint32_t i = 0; i < len; i++) spi_xfer(buf[i]);
  cs_high();
  wait_wip_clear();
}

void chip_erase() {
  wren();
  cs_low();
  spi_xfer(0xC7); // Chip Erase (also 0x60 on some)
  cs_high();
  wait_wip_clear();
}

uint32_t read_u32() {
  uint32_t v = 0;
  while (Serial.available() < 4) {}
  v |= (uint32_t)Serial.read();
  v |= (uint32_t)Serial.read() << 8;
  v |= (uint32_t)Serial.read() << 16;
  v |= (uint32_t)Serial.read() << 24;
  return v;
}

void setup() {
  pinMode(PIN_CS, OUTPUT);
  cs_high();
  Serial.begin(BAUD);
  SPI.begin();
  SPI.beginTransaction(SPISettings(2000000, MSBFIRST, SPI_MODE0)); // 2 MHz is safe with clip
}

void loop() {
  if (!Serial.available()) return;
  char cmd = (char)Serial.read();

  if (cmd == 'I') {
    Serial.print("SPIBRIDGE1\n");
  } else if (cmd == 'S') {
    cs_low();
    spi_xfer(0x9F);
    uint8_t m = spi_xfer(0xFF);
    uint8_t t = spi_xfer(0xFF);
    uint8_t c = spi_xfer(0xFF);
    cs_high();
    Serial.write(m); Serial.write(t); Serial.write(c);
  } else if (cmd == 'R' || cmd == 'B') {
    uint32_t addr = read_u32();
    uint32_t len  = read_u32();
    read_bytes(addr, len, cmd == 'B');
  } else if (cmd == 'W') {
    uint32_t addr = read_u32();
    uint32_t len  = read_u32();

    const uint16_t PAGE = 256;
    static uint8_t buf[PAGE];

    uint32_t remaining = len;
    uint32_t cur = addr;

    while (remaining > 0) {
      // bytes allowed until page boundary
      uint16_t room = PAGE - (cur & (PAGE - 1));
      uint16_t chunk = (remaining < room) ? remaining : room;
      // read chunk from serial
      uint16_t got = 0;
      while (got < chunk) {
        if (Serial.available()) buf[got++] = Serial.read();
      }
      page_program(cur, buf, chunk);
      cur += chunk;
      remaining -= chunk;
    }
    send_ok();
  } else if (cmd == 'E') {
    chip_erase();
    send_ok();
  } else {
    send_err();
  }
}