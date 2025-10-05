#include "LpaWriteCounter.h"
#include <unordered_map>

static std::unordered_map<LPA_type, uint64_t> g_lpa_write_count;
static sim_time_type g_last_reset_time = 0;
// Use a distinct name to avoid collision with Sim_Defs.h macro ONE_SECOND
static const sim_time_type LPAWC_ONE_SECOND = ONE_SECOND; // nanoseconds

void LpaWriteCounter_OnWrite(LPA_type lpa)
{
    if (g_lpa_write_count.find(lpa) == g_lpa_write_count.end()) {
        g_lpa_write_count[lpa] = 1;
        return;
    }
    g_lpa_write_count[lpa]++;
}

void LpaWriteCounter_ResetOne(LPA_type lpa)
{
    auto it = g_lpa_write_count.find(lpa);
    if (it != g_lpa_write_count.end()) {
        it->second = 0;
    }
}

void LpaWriteCounter_ResetAll()
{
    for (auto &kv : g_lpa_write_count) {
        kv.second = 0;
    }
}

uint64_t LpaWriteCounter_Get(LPA_type lpa)
{
    auto it = g_lpa_write_count.find(lpa);
    return it == g_lpa_write_count.end() ? 0 : it->second;
}

void LpaWriteCounter_TryPeriodicReset(sim_time_type now)
{
    if (now - g_last_reset_time >= LPAWC_ONE_SECOND) {
        LpaWriteCounter_ResetAll();
        g_last_reset_time = now;
    }
}