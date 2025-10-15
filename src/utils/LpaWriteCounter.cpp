#include "LpaWriteCounter.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

static std::unordered_map<LPA_type, uint64_t> g_lpa_write_count;
static sim_time_type g_last_reset_time = 0;
// Use a distinct name to avoid collision with Sim_Defs.h macro ONE_SECOND
static const sim_time_type LPAWC_ONE_SECOND = ONE_SECOND * 2; // nanoseconds
static std::vector<uint64_t> g_prev_window_counts;
static uint64_t g_prev_window_threshold = 0;
static uint64_t g_prev_window_cold_threshold = 0;

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
        // Snapshot previous window and compute thresholds as top-10% (hot) and bottom-10% (cold) cutoff values
        g_prev_window_counts.clear();
        g_prev_window_counts.reserve(g_lpa_write_count.size());
        for (auto &kv : g_lpa_write_count) {
            g_prev_window_counts.push_back(kv.second);
        }
        if (!g_prev_window_counts.empty()) {
            // Hot threshold
            std::vector<uint64_t> sorted_hot = g_prev_window_counts;
            std::sort(sorted_hot.begin(), sorted_hot.end(), std::greater<uint64_t>());
            size_t hot_rank = (size_t)(sorted_hot.size() * 0.1);
            if (hot_rank >= sorted_hot.size()) hot_rank = sorted_hot.size() - 1;
            g_prev_window_threshold = sorted_hot[hot_rank];

            // Cold threshold (bottom 10%)
            std::vector<uint64_t> sorted_cold = g_prev_window_counts;
            std::sort(sorted_cold.begin(), sorted_cold.end());
            size_t cold_rank = (size_t)(sorted_cold.size() * 0.2);
            if (cold_rank >= sorted_cold.size()) cold_rank = sorted_cold.size() - 1;
            g_prev_window_cold_threshold = sorted_cold[cold_rank];
        } else {
            g_prev_window_threshold = 50;
            g_prev_window_cold_threshold = 0;
        }
        LpaWriteCounter_ResetAll();
        g_last_reset_time = now;
    }
}

uint64_t LpaWriteCounter_GetPrevWindowThreshold()
{
    return g_prev_window_threshold;
}

bool LpaWriteCounter_IsHot_ByPrevWindow(LPA_type lpa)
{
    uint64_t th = LpaWriteCounter_GetPrevWindowThreshold();
    return th > 0 && LpaWriteCounter_Get(lpa) >= th;
}

uint64_t LpaWriteCounter_GetPrevWindowColdThreshold()
{
    return g_prev_window_cold_threshold;
}

bool LpaWriteCounter_IsCold_ByPrevWindow(LPA_type lpa)
{
    uint64_t th = LpaWriteCounter_GetPrevWindowColdThreshold();
    return th > 0 && LpaWriteCounter_Get(lpa) <= th;
}