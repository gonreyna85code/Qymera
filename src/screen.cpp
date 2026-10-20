#include <Arduino.h>
#include <Wire.h>
#include "screen.h"
#include "config.h"
#include "entities.h"
#include "model.h"
using namespace qymera::model;
#include "core.h"
#include "net.h"

namespace screen {

static uint16_t W = 0, H = 0;
static uint8_t SDA = 0, SCL = 0;
static bool initialised = false;
static bool init_pending = false;
static uint32_t init_due_ms = 0;
static uint32_t last_redraw_ms = 0;
static uint8_t anim_phase = 0;
static uint8_t *fb = nullptr;
static uint16_t fb_bytes = 0;
static int scroll_offset = 0;
static uint32_t last_scroll_ms = 0;

static const uint8_t FONT5x7[][5] PROGMEM = {
  {0x00,0x00,0x00,0x00,0x00},
  {0x00,0x00,0x5F,0x00,0x00},
  {0x00,0x07,0x00,0x07,0x00},
  {0x14,0x7F,0x14,0x7F,0x14},
  {0x24,0x2A,0x7F,0x2A,0x12},
  {0x23,0x13,0x08,0x64,0x62},
  {0x36,0x49,0x55,0x22,0x50},
  {0x00,0x05,0x03,0x00,0x00},
  {0x00,0x1C,0x22,0x41,0x00},
  {0x00,0x41,0x22,0x1C,0x00},
  {0x14,0x08,0x3E,0x08,0x14},
  {0x08,0x08,0x3E,0x08,0x08},
  {0x00,0x50,0x30,0x00,0x00},
  {0x08,0x08,0x08,0x08,0x08},
  {0x00,0x60,0x60,0x00,0x00},
  {0x20,0x10,0x08,0x04,0x02},
  {0x3E,0x51,0x49,0x45,0x3E},
  {0x00,0x42,0x7F,0x40,0x00},
  {0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},
  {0x18,0x14,0x12,0x7F,0x10},
  {0x27,0x45,0x45,0x45,0x39},
  {0x3C,0x4A,0x49,0x49,0x30},
  {0x01,0x71,0x09,0x05,0x03},
  {0x36,0x49,0x49,0x49,0x36},
  {0x06,0x49,0x49,0x29,0x1E},
  {0x00,0x36,0x36,0x00,0x00},
  {0x00,0x56,0x36,0x00,0x00},
  {0x08,0x14,0x22,0x41,0x00},
  {0x14,0x14,0x14,0x14,0x14},
  {0x41,0x22,0x14,0x08,0x00},
  {0x02,0x01,0x51,0x09,0x06},
  {0x32,0x49,0x79,0x41,0x3E},
  {0x7E,0x11,0x11,0x11,0x7E},
  {0x7F,0x49,0x49,0x49,0x36},
  {0x3E,0x41,0x41,0x41,0x22},
  {0x7F,0x41,0x41,0x22,0x1C},
  {0x7F,0x49,0x49,0x49,0x41},
  {0x7F,0x09,0x09,0x09,0x01},
  {0x3E,0x41,0x49,0x49,0x7A},
  {0x7F,0x08,0x08,0x08,0x7F},
  {0x00,0x41,0x7F,0x41,0x00},
  {0x20,0x40,0x41,0x3F,0x01},
  {0x7F,0x08,0x14,0x22,0x41},
  {0x7F,0x40,0x40,0x40,0x40},
  {0x7F,0x02,0x0C,0x02,0x7F},
  {0x7F,0x04,0x08,0x10,0x7F},
  {0x3E,0x41,0x41,0x41,0x3E},
  {0x7F,0x09,0x09,0x09,0x06},
  {0x3E,0x41,0x51,0x21,0x5E},
  {0x7F,0x09,0x19,0x29,0x46},
  {0x46,0x49,0x49,0x49,0x31},
  {0x01,0x01,0x7F,0x01,0x01},
  {0x3F,0x40,0x40,0x40,0x3F},
  {0x1F,0x20,0x40,0x20,0x1F},
  {0x3F,0x40,0x38,0x40,0x3F},
  {0x63,0x14,0x08,0x14,0x63},
  {0x07,0x08,0x70,0x08,0x07},
  {0x61,0x51,0x49,0x45,0x43},
  {0x00,0x7F,0x41,0x41,0x00},
  {0x02,0x04,0x08,0x10,0x20},
  {0x00,0x41,0x41,0x7F,0x00},
  {0x04,0x02,0x01,0x02,0x04},
  {0x40,0x40,0x40,0x40,0x40},
  {0x00,0x01,0x02,0x04,0x00},
  {0x20,0x54,0x54,0x54,0x78},
  {0x7F,0x48,0x44,0x44,0x38},
  {0x38,0x44,0x44,0x44,0x20},
  {0x38,0x44,0x44,0x48,0x7F},
  {0x38,0x54,0x54,0x54,0x18},
  {0x08,0x7E,0x09,0x01,0x02},
  {0x0C,0x52,0x52,0x52,0x3E},
  {0x7F,0x08,0x04,0x04,0x78},
  {0x00,0x44,0x7D,0x40,0x00},
  {0x20,0x40,0x44,0x3D,0x00},
  {0x7F,0x10,0x28,0x44,0x00},
  {0x00,0x41,0x7F,0x40,0x00},
  {0x7C,0x04,0x18,0x04,0x78},
  {0x7C,0x08,0x04,0x04,0x78},
  {0x38,0x44,0x44,0x44,0x38},
  {0x7C,0x14,0x14,0x14,0x08},
  {0x08,0x14,0x14,0x18,0x7C},
  {0x7C,0x08,0x04,0x04,0x08},
  {0x48,0x54,0x54,0x54,0x20},
  {0x04,0x3F,0x44,0x40,0x20},
  {0x3C,0x40,0x40,0x20,0x7C},
  {0x1C,0x20,0x40,0x20,0x1C},
  {0x3C,0x40,0x30,0x40,0x3C},
  {0x44,0x28,0x10,0x28,0x44},
  {0x0C,0x50,0x50,0x50,0x3C},
  {0x44,0x64,0x54,0x4C,0x44},
  {0x00,0x08,0x36,0x41,0x00},
  {0x00,0x00,0x7F,0x00,0x00},
  {0x00,0x41,0x36,0x08,0x00},
  {0x10,0x08,0x08,0x10,0x08},
};

static void i2cCmd(uint8_t c) {
  Wire.beginTransmission(0x3C);
  Wire.write(0x00);
  Wire.write(c);
  Wire.endTransmission();
}

static void i2cData(const uint8_t *d, uint16_t n) {
  Wire.beginTransmission(0x3C);
  Wire.write(0x40);
  for (uint16_t i = 0; i < n; i++) Wire.write(d[i]);
  Wire.endTransmission();
}

static bool probe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static void fbClear() {
  if (!fb) return;
  memset(fb, 0, fb_bytes);
}

static void fbSet(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= (int)W || y >= (int)H) return;
  uint16_t idx = (y >> 3) * W + x;
  if (on) fb[idx] |= (1 << (y & 7));
  else    fb[idx] &= ~(1 << (y & 7));
}

static void fbHLine(int x, int y, int w, bool on) {
  for (int i = 0; i < w; i++) fbSet(x + i, y, on);
}
static void fbVLine(int x, int y, int h, bool on) {
  for (int i = 0; i < h; i++) fbSet(x, y + i, on);
}
static void fbRect(int x, int y, int w, int h, bool on) {
  fbHLine(x, y, w, on);
  fbHLine(x, y + h - 1, w, on);
  fbVLine(x, y, h, on);
  fbVLine(x + w - 1, y, h, on);
}
static void fbFillRect(int x, int y, int w, int h, bool on) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) fbSet(xx, yy, on);
}

static void fbText(int x, int y, const char *s, uint8_t scale) {
  int cx = x;
  while (*s) {
    uint8_t c = (uint8_t)*s++;
    if (c < 32 || c > 126) c = 32;
    const uint8_t *g = &FONT5x7[c - 32][0];
    for (uint8_t col = 0; col < 5; col++) {
      uint8_t bits = pgm_read_byte(g + col);
      for (uint8_t row = 0; row < 7; row++) {
        if (bits & (1 << row)) {
          for (uint8_t sx = 0; sx < scale; sx++)
            for (uint8_t sy = 0; sy < scale; sy++)
              fbSet(cx + col * scale + sx, y + row * scale + sy, true);
        }
      }
    }
    cx += 6 * scale;
    if (cx > (int)W) break;
  }
}

static void fbCenterText(int y, const char *s, uint8_t scale) {
  int w = (int)strlen(s) * 6 * scale;
  int x = ((int)W - w) / 2;
  if (x < 0) x = 0;
  fbText(x, y, s, scale);
}

static bool panelInit() {
  if (!probe(0x3C) && !probe(0x3D)) return false;
  i2cCmd(0xAE);        // display off
  i2cCmd(0xD5); i2cCmd(0x80); // clock
  if (H == 32) {
    i2cCmd(0xA8); i2cCmd(0x1F); // multiplex 1/32
    i2cCmd(0xD3); i2cCmd(0x00); // offset
    i2cCmd(0x40);              // start line
    i2cCmd(0xA1);              // seg remap
    i2cCmd(0xC8);              // com scan dec
    i2cCmd(0xDA); i2cCmd(0x02); // com pins
  } else {
    i2cCmd(0xA8); i2cCmd(H - 1);
    i2cCmd(0xD3); i2cCmd(0x00);
    i2cCmd(0x40);
    i2cCmd(0xA1);
    i2cCmd(0xC8);
    i2cCmd(0xDA); i2cCmd((H == 64) ? 0x12 : 0x12);
  }
  i2cCmd(0x81); i2cCmd(0xCF); // contrast
  i2cCmd(0xD9); i2cCmd(0xF1); // precharge
  i2cCmd(0xDB); i2cCmd(0x40); // vcomh
  i2cCmd(0xA4);              // display follows RAM
  i2cCmd(0xA6);              // normal
  i2cCmd(0x8D); i2cCmd(0x14); // charge pump
  i2cCmd(0xAF);              // display on
  i2cCmd(0x20); i2cCmd(0x00); // horizontal addressing
  return true;
}

static void panelFlush() {
  i2cCmd(0x21); i2cCmd(0); i2cCmd(W - 1);
  i2cCmd(0x22); i2cCmd(0); i2cCmd((H / 8) - 1);
  for (uint16_t i = 0; i < fb_bytes; i += 16) {
    uint8_t n = (i + 16 <= fb_bytes) ? 16 : (fb_bytes - i);
    i2cData(fb + i, n);
  }
}

static void drawSplash() {
  fbClear();
  uint8_t titleScale = (W >= 128) ? 2 : 1;
  fbCenterText(0, "QYMERA", titleScale);
  fbCenterText(16, "LOCAL AUTOMATION", 1);
  char foot[40];
  if (core::is_connected()) {
    IPAddress ip = WiFi.localIP();
    snprintf(foot, sizeof(foot), "IP %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
  } else {
    strcpy(foot, "MESH / ESP-NOW");
  }
  fbCenterText(H - 7, foot, 1);
  panelFlush();
}

static void drawEntities() {
  fbClear();
  const int footer_y = (int)H; // use full height for data pages
  const int start_y = 0;
  int avail = footer_y - start_y;
  uint8_t lines = 0;
  if (avail >= 16) lines = 2;
  else if (avail >= 8) lines = 1;
  else lines = 0;
  if (lines == 0) {
    panelFlush();
    return;
  }
  uint8_t row = 0;
  for (uint8_t i = 0; i < entities::capacity() && row < lines; i++) {
    if (!entities::isUsed(i)) continue;
    if (i < (uint8_t)scroll_offset) continue;
    const Entity &e = entities::peek(i);
    char vbuf[32] = {0};
    using qymera::model::kindOfType;
    using qymera::model::EntityKind;
    if (kindOfType(e.config.type) == EntityKind::ACTUATOR) {
      if (e.config.type == qymera::model::TYPE_DIMMER) {
        snprintf(vbuf, sizeof(vbuf), "%d%%", e.state.value);
      } else {
        snprintf(vbuf, sizeof(vbuf), "%s", e.state.state ? "ON" : "OFF");
      }
    } else if (e.config.type == qymera::model::SENSOR_TIME) {
      // Try to format time-like values if small; otherwise show raw
      snprintf(vbuf, sizeof(vbuf), "%lu", (unsigned long)e.state.value);
    } else if (e.config.type == qymera::model::SENSOR_TEMP ||
               e.config.type == qymera::model::SENSOR_HUMI ||
               e.config.type == qymera::model::SENSOR_PRESS ||
               e.config.type == qymera::model::SENSOR_LUMI ||
               e.config.type == qymera::model::SENSOR_GENERIC) {
      // Show with reasonable precision like calibration/GUI style
      float fv = e.state.value;
      // if value_f not set meaningfully, use value
      snprintf(vbuf, sizeof(vbuf), "%.1f", fv);
    } else {
      float fv = e.state.value;
      if (fv == 0.0f) snprintf(vbuf, sizeof(vbuf), "%lu", (unsigned long)e.state.value);
      else snprintf(vbuf, sizeof(vbuf), "%.2f", fv);
    }
    int vx = W - (int)strlen(vbuf) * 6;
    if (vx < 0) vx = 0;
    int ry = start_y + row * 8;
    if (ry + 7 >= footer_y) break;
    fbText(vx, ry, vbuf, 1);
    char line[40];
    snprintf(line, sizeof(line), "%s", e.config.name);
    int max_chars = vx / 6;
    if (max_chars >= 1) {
      if ((int)strlen(line) > max_chars) line[max_chars] = ' ';
      fbText(0, ry, line, 1);
    }
    row++;
  }
  // Use both available rows fully - if only 1 row of entities, we still have space
  // but requirement says use 2 unused rows; with 2 lines we consume both rows
  panelFlush();
}


void init(uint16_t width, uint16_t height, uint8_t sda, uint8_t scl) {
  W = width;
  H = height;
  SDA = sda;
  SCL = scl;
  fb_bytes = (W * H) / 8;
  if (fb) free(fb);
  fb = (uint8_t *)malloc(fb_bytes);
  if (!fb) {
    fb_bytes = 0;
    return;
  }
  memset(fb, 0, fb_bytes);
  init_pending = true;
  init_due_ms = millis() + 30000;
}

void tick(uint32_t now_ms) {
  if (!fb) return;
  if (init_pending && !initialised) {
    if ((int32_t)(now_ms - init_due_ms) >= 0) {
      Wire.begin(SDA, SCL);
      Wire.setClock(400000);
      if (panelInit()) {
        initialised = true;
        init_pending = false;
        last_redraw_ms = now_ms;
        drawSplash();
      } else {
        init_due_ms = now_ms + 5000;
      }
    }
    return;
  }
  if (!initialised) return;
  // Scroll entities for multi-page if more than 2 rows of data
  if (now_ms - last_scroll_ms >= 4000) {
    last_scroll_ms = now_ms;
    int used = 0;
    for (uint8_t i = 0; i < entities::capacity(); i++)
      if (entities::isUsed(i)) used++;
    if (used > 2) {
      scroll_offset += 2;
      if (scroll_offset > used - 2) scroll_offset = 0;
    } else {
      scroll_offset = 0;
    }
  }
  // Splash every 60 seconds, data pages every 4 seconds
  if (now_ms - last_redraw_ms >= 4000) {
    last_redraw_ms = now_ms;
    anim_phase++;
    if ((now_ms % 60000) < 4000) {
      drawSplash();
    } else {
      drawEntities();
    }
  }
}

void redraw() {
  if (!initialised) return;
  last_redraw_ms = 0;
}

bool ready() { return initialised; }
uint16_t width() { return W; }
uint16_t height() { return H; }

}  // namespace screen
