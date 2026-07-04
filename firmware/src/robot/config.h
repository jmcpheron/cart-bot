#pragma once

// The robot's own access point — used by the bench-tuning web page.
// Must share proto::kEspNowChannel so the transmitter link keeps working.
constexpr const char* kTuneApSsid = "cartbot-robot";
constexpr const char* kTuneApPass = "cartbot123";
