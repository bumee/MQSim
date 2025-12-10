#ifndef LPA_WRITE_COUNTER_H
#define LPA_WRITE_COUNTER_H

#include "../ssd/SSD_Defs.h"

// Always enable by default unless explicitly disabled
#ifndef ENABLE_LPA_WRITE_COUNTER
#define ENABLE_LPA_WRITE_COUNTER
#endif

// Usage:
// Define ENABLE_LPA_WRITE_COUNTER and provide an implementation for
//   void LpaWriteCounter_OnWrite(LPA_type lpa);
// to enable counting. Otherwise, the macro call becomes a no-op.

#ifdef ENABLE_LPA_WRITE_COUNTER
extern void LpaWriteCounter_OnWrite(LPA_type lpa);
extern void LpaWriteCounter_ResetOne(LPA_type lpa);
extern void LpaWriteCounter_ResetAll();
extern uint64_t LpaWriteCounter_Get(LPA_type lpa);
extern bool LpaWriteCounter_TryPeriodicReset(sim_time_type now); // Returns true if reset occurred
// Dynamic threshold from the immediately previous window (top-40% cutoff of counts)
extern uint64_t LpaWriteCounter_GetPrevWindowThreshold();
extern bool LpaWriteCounter_IsHot_ByPrevWindow(LPA_type lpa);
extern uint64_t LpaWriteCounter_GetPrevWindowColdThreshold();
extern bool LpaWriteCounter_IsCold_ByPrevWindow(LPA_type lpa);
// Time interval based hot/cold detection
extern void LpaWriteCounter_OnWrite_WithTime(LPA_type lpa, sim_time_type current_time);
extern bool LpaWriteCounter_IsHot_ByTimeInterval(LPA_type lpa, sim_time_type current_time);
extern bool LpaWriteCounter_IsCold_ByTimeInterval(LPA_type lpa, sim_time_type current_time);
#define LPA_WRITE_COUNTER_ON_WRITE(LPA_VAL) LpaWriteCounter_OnWrite(LPA_VAL)
#else
#define LPA_WRITE_COUNTER_ON_WRITE(LPA_VAL) do {} while (0)
#endif

#endif // LPA_WRITE_COUNTER_H
