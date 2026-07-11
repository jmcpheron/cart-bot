#include "dancescript.h"

#include <cstdio>
#include <cstring>

namespace dancescript {
namespace {

long clamp_long(long v, long lo, long hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

float clamp_float(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

const char* skip_spaces(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

}  // namespace

int parse(const char* text, Step* out, int max_steps,
          uint32_t turn_timeout_ms) {
    if (!text || !*text || max_steps <= 0) return -1;

    char buf[1600];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int n = 0;
    uint32_t total = 0;
    char* save = nullptr;
    for (char* tok = strtok_r(buf, ";", &save); tok;
         tok = strtok_r(nullptr, ";", &save)) {
        if (n >= max_steps) return -1;
        Step s;
        const char* body = skip_spaces(tok);
        if (*body == 't' || *body == 'T') {
            float deg;
            int w;
            if (sscanf(body + 1, " ,%f,%d", &deg, &w) != 2) return -1;
            s.turn = true;
            s.turn_deg = clamp_float(deg, -720.0f, 720.0f);
            const long mag = clamp_long(w < 0 ? -w : w, 10, 100);
            // Direction comes from the sign of deg; w is speed only.
            s.w = static_cast<int8_t>(s.turn_deg >= 0 ? mag : -mag);
            s.ms = turn_timeout_ms;
        } else {
            int vx, vy, w;
            long ms;
            if (sscanf(body, "%d,%d,%d,%ld", &vx, &vy, &w, &ms) != 4)
                return -1;
            s.vx = static_cast<int8_t>(clamp_long(vx, -100, 100));
            s.vy = static_cast<int8_t>(clamp_long(vy, -100, 100));
            s.w = static_cast<int8_t>(clamp_long(w, -100, 100));
            s.ms = static_cast<uint32_t>(clamp_long(ms, 50, 15000));
        }
        total += s.ms;
        if (total > kMaxTotalMs) return -1;
        out[n++] = s;
    }
    return n > 0 ? n : -1;
}

}  // namespace dancescript
