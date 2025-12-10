#include "LpaWriteCounter.h"
#include <unordered_map>
#include <vector>
#include <algorithm>

static std::unordered_map<LPA_type, uint64_t> g_lpa_write_count;
static sim_time_type g_last_reset_time = 0;
// Use a distinct name to avoid collision with Sim_Defs.h macro ONE_SECOND
static const sim_time_type LPAWC_ONE_SECOND = ONE_SECOND * 3; // nanoseconds
static std::vector<uint64_t> g_prev_window_counts;
static uint64_t g_prev_window_threshold = 50; // 초기 기본값: 윈도우당 50회 이상 쓰기면 HOT
static uint64_t g_prev_window_cold_threshold = 0; // 초기 기본값: 0회 이하는 COLD 판정 안 함

// Time interval based hot/cold detection: track last access time per LPA
static std::unordered_map<LPA_type, sim_time_type> g_lpa_last_access_time;
static const sim_time_type TIME_INTERVAL_THRESHOLD = ONE_SECOND * 2; // 2초 내 접근이면 HOT

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

bool LpaWriteCounter_TryPeriodicReset(sim_time_type now)
{
    if (now - g_last_reset_time >= LPAWC_ONE_SECOND) {
        // Snapshot previous window and compute thresholds as top-10% (hot) and bottom-10% (cold) cutoff values
        g_prev_window_counts.clear();
        g_prev_window_counts.reserve(g_lpa_write_count.size());
        // 0인 LPA는 제외 (쓰기가 없었던 LPA는 hotness 계산에서 제외)
        for (auto &kv : g_lpa_write_count) {
            if (kv.second > 0) {
                g_prev_window_counts.push_back(kv.second);
            }
        }
        // 충분한 non-zero 샘플이 있을 때만 임계값 재계산 (10개 이상), 없으면 이전 값 유지
        if (!g_prev_window_counts.empty()) {
            // Hot threshold (상위 10%)
            std::vector<uint64_t> sorted_hot = g_prev_window_counts;
            std::sort(sorted_hot.begin(), sorted_hot.end(), std::greater<uint64_t>());
            size_t hot_rank = (size_t)(sorted_hot.size() * 0.2);
            if (hot_rank >= sorted_hot.size()) hot_rank = sorted_hot.size() - 1;
            g_prev_window_threshold = sorted_hot[hot_rank];

            // Cold threshold (하위 10%)
            std::vector<uint64_t> sorted_cold = g_prev_window_counts;
            std::sort(sorted_cold.begin(), sorted_cold.end());
            size_t cold_rank = (size_t)(sorted_cold.size() * 0.2);
            if (cold_rank >= sorted_cold.size()) cold_rank = sorted_cold.size() - 1;
            g_prev_window_cold_threshold = sorted_cold[cold_rank];
           

            // 상호배타 보장: HotThreshold ≤ ColdThreshold 인 경우 COLD 판정 비활성화
            if (g_prev_window_threshold <= g_prev_window_cold_threshold) {
                // cold threshold를 0으로 내려 cold 판정이 항상 false 되도록 처리
                g_prev_window_cold_threshold = g_prev_window_threshold - 1;
            }
        }
        // 샘플이 부족하면 이전 임계값 유지 (초기값 또는 직전 윈도우 값)
        LpaWriteCounter_ResetAll();
        g_last_reset_time = now;
        return true; // Reset occurred
    }
    return false; // No reset
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

void LpaWriteCounter_OnWrite_WithTime(LPA_type lpa, sim_time_type current_time)
{
    // Update write count
    LpaWriteCounter_OnWrite(lpa);
    // Update last access time
    g_lpa_last_access_time[lpa] = current_time;
}

bool LpaWriteCounter_IsHot_ByTimeInterval(LPA_type lpa, sim_time_type current_time)
{
    auto it = g_lpa_last_access_time.find(lpa);
    if (it == g_lpa_last_access_time.end()) {
        // Never accessed before
        return false;
    }
    sim_time_type time_since_last_access = current_time - it->second;
    // HOT if accessed within 1 second
    return time_since_last_access <= TIME_INTERVAL_THRESHOLD;
}

bool LpaWriteCounter_IsCold_ByTimeInterval(LPA_type lpa, sim_time_type current_time)
{
    auto it = g_lpa_last_access_time.find(lpa);
    if (it == g_lpa_last_access_time.end()) {
        // Never accessed before: treat as COLD
        return true;
    }
    sim_time_type time_since_last_access = current_time - it->second;
    // COLD if not accessed within 1 second (i.e., more than 1 second ago)
    return time_since_last_access > TIME_INTERVAL_THRESHOLD;
}