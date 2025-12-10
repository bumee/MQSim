#ifndef STATS_H
#define STATS_H

#include "SSD_Defs.h"
#include <string>

namespace SSD_Components
{
	class Stats
	{
	public:
		static void Init_stats(unsigned int channel_no, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die, unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int max_allowed_block_erase_count);
		static void Clear_stats(unsigned int channel_no, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die, unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int max_allowed_block_erase_count);
		static unsigned long IssuedReadCMD, IssuedCopybackReadCMD, IssuedInterleaveReadCMD, IssuedMultiplaneReadCMD, IssuedMultiplaneCopybackReadCMD;
		static unsigned long IssuedProgramCMD, IssuedInterleaveProgramCMD, IssuedMultiplaneProgramCMD, IssuedInterleaveMultiplaneProgramCMD, IssuedCopybackProgramCMD, IssuedMultiplaneCopybackProgramCMD;
		static unsigned long IssuedEraseCMD, IssuedInterleaveEraseCMD, IssuedMultiplaneEraseCMD, IssuedInterleaveMultiplaneEraseCMD;

		static unsigned long IssuedSuspendProgramCMD, IssuedSuspendEraseCMD;

		static unsigned long Total_flash_reads_for_mapping, Total_flash_writes_for_mapping;
		static unsigned long Total_flash_reads_for_mapping_per_stream[MAX_SUPPORT_STREAMS], Total_flash_writes_for_mapping_per_stream[MAX_SUPPORT_STREAMS];

		static unsigned int CMT_hits, readTR_CMT_hits, writeTR_CMT_hits;
		static unsigned int CMT_miss, readTR_CMT_miss, writeTR_CMT_miss;
		static unsigned int total_CMT_queries, total_readTR_CMT_queries, total_writeTR_CMT_queries;
		
		static unsigned int CMT_hits_per_stream[MAX_SUPPORT_STREAMS], readTR_CMT_hits_per_stream[MAX_SUPPORT_STREAMS], writeTR_CMT_hits_per_stream[MAX_SUPPORT_STREAMS];
		static unsigned int CMT_miss_per_stream[MAX_SUPPORT_STREAMS], readTR_CMT_miss_per_stream[MAX_SUPPORT_STREAMS], writeTR_CMT_miss_per_stream[MAX_SUPPORT_STREAMS];
		static unsigned int total_CMT_queries_per_stream[MAX_SUPPORT_STREAMS], total_readTR_CMT_queries_per_stream[MAX_SUPPORT_STREAMS], total_writeTR_CMT_queries_per_stream[MAX_SUPPORT_STREAMS];
		

		static unsigned int Total_gc_executions, Total_gc_executions_per_stream[MAX_SUPPORT_STREAMS];
		static unsigned int Total_page_movements_for_gc, Total_gc_page_movements_per_stream[MAX_SUPPORT_STREAMS];

		static unsigned int Total_wl_executions, Total_wl_executions_per_stream[MAX_SUPPORT_STREAMS];
		static unsigned int Total_page_movements_for_wl, Total_wl_page_movements_per_stream[MAX_SUPPORT_STREAMS];

		static unsigned int***** Block_erase_histogram;

		// Baselines captured right after preconditioning finishes
		static unsigned int Baseline_GC_Executions_AfterPreconditioning;
		static unsigned int Baseline_GC_Page_Movements_AfterPreconditioning;

		// Page type program counts
		static unsigned long Program_LSB_Count;
		static unsigned long Program_CSB_Count;
		static unsigned long Program_MSB_Count;
		static void Dump_page_type_counts_csv();

        // GC Event Logging
        static FILE* gc_log_file;
        static void Init_GC_Log();
        static void Record_GC_Start(double time);
        static void Record_GC_End(double time);
        static void Close_GC_Log();
        
        // Block Pool Hot/Cold Ratio Logging
        static FILE* blockpool_hotcold_log_file;
        static std::string workload_file_path;
        static void SetWorkloadPath(const std::string& path);
        static void Init_BlockPool_HotCold_Log();
        static void Record_BlockPool_HotCold_Ratio(double time, 
            unsigned int hot_pool_hot_pages, unsigned int hot_pool_cold_pages, unsigned int hot_pool_total_pages,
            unsigned int warm_pool_hot_pages, unsigned int warm_pool_cold_pages, unsigned int warm_pool_total_pages,
            unsigned int cold_pool_hot_pages, unsigned int cold_pool_cold_pages, unsigned int cold_pool_total_pages);
        static void Close_BlockPool_HotCold_Log();
	};
}

#endif // !STATS_H
