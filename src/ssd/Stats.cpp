#include "Stats.h"
#include <string>


namespace SSD_Components
{
	unsigned long Stats::IssuedReadCMD = 0;
	unsigned long Stats::IssuedCopybackReadCMD = 0;
	unsigned long Stats::IssuedInterleaveReadCMD = 0;
	unsigned long Stats::IssuedMultiplaneReadCMD = 0;
	unsigned long Stats::IssuedMultiplaneCopybackReadCMD = 0;
	unsigned long Stats::IssuedProgramCMD = 0;
	unsigned long Stats::IssuedInterleaveProgramCMD = 0;
	unsigned long Stats::IssuedMultiplaneProgramCMD = 0;
	unsigned long Stats::IssuedMultiplaneCopybackProgramCMD = 0;
	unsigned long Stats::IssuedInterleaveMultiplaneProgramCMD = 0;
	unsigned long Stats::IssuedSuspendProgramCMD = 0;
	unsigned long Stats::IssuedCopybackProgramCMD = 0;
	unsigned long Stats::IssuedEraseCMD = 0;
	unsigned long Stats::IssuedInterleaveEraseCMD = 0;
	unsigned long Stats::IssuedMultiplaneEraseCMD = 0;
	unsigned long Stats::IssuedInterleaveMultiplaneEraseCMD = 0;
	unsigned long Stats::IssuedSuspendEraseCMD = 0;
	unsigned long Stats::Total_flash_reads_for_mapping = 0;
	unsigned long Stats::Total_flash_writes_for_mapping = 0;
	unsigned long Stats::Total_flash_reads_for_mapping_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned long Stats::Total_flash_writes_for_mapping_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned int***** Stats::Block_erase_histogram;
	unsigned int Stats::Baseline_GC_Executions_AfterPreconditioning = 0;
	unsigned int Stats::Baseline_GC_Page_Movements_AfterPreconditioning = 0;
	unsigned int  Stats::CMT_hits = 0, Stats::readTR_CMT_hits = 0, Stats::writeTR_CMT_hits = 0;
	unsigned int  Stats::CMT_miss = 0, Stats::readTR_CMT_miss = 0, Stats::writeTR_CMT_miss = 0;
	unsigned int  Stats::total_CMT_queries = 0, Stats::total_readTR_CMT_queries = 0, Stats::total_writeTR_CMT_queries = 0;

	unsigned int Stats::Total_gc_executions = 0, Stats::Total_gc_executions_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned int Stats::Total_page_movements_for_gc = 0, Stats::Total_gc_page_movements_per_stream[MAX_SUPPORT_STREAMS] = { 0 };

	unsigned int Stats::Total_wl_executions = 0, Stats::Total_wl_executions_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned int Stats::Total_page_movements_for_wl = 0, Stats::Total_wl_page_movements_per_stream[MAX_SUPPORT_STREAMS] = { 0 };

	unsigned int Stats::CMT_hits_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::readTR_CMT_hits_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::writeTR_CMT_hits_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned int Stats::CMT_miss_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::readTR_CMT_miss_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::writeTR_CMT_miss_per_stream[MAX_SUPPORT_STREAMS] = { 0 };
	unsigned int Stats::total_CMT_queries_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::total_readTR_CMT_queries_per_stream[MAX_SUPPORT_STREAMS] = { 0 }, Stats::total_writeTR_CMT_queries_per_stream[MAX_SUPPORT_STREAMS] = { 0 };


	void Stats::Init_stats(unsigned int channel_no, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die, 
		unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int max_allowed_block_erase_count)
	{
		Block_erase_histogram = new unsigned int ****[channel_no];
		for (unsigned int channel_cntr = 0; channel_cntr < channel_no; channel_cntr++) {
			Block_erase_histogram[channel_cntr] = new unsigned int***[chip_no_per_channel];
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++) {
				Block_erase_histogram[channel_cntr][chip_cntr] = new unsigned int**[die_no_per_chip];
				for (unsigned int die_cntr = 0; die_cntr < die_no_per_chip; die_cntr++) {
					Block_erase_histogram[channel_cntr][chip_cntr][die_cntr] = new unsigned int*[plane_no_per_die];
					for (unsigned int plane_cntr = 0; plane_cntr < plane_no_per_die; plane_cntr++) {
						Block_erase_histogram[channel_cntr][chip_cntr][die_cntr][plane_cntr] = new unsigned int[max_allowed_block_erase_count];
						Block_erase_histogram[channel_cntr][chip_cntr][die_cntr][plane_cntr][0] = block_no_per_plane * page_no_per_block; //At the start of the simulation all pages have zero erase count
						for (unsigned int i = 1; i < max_allowed_block_erase_count; ++i) {
							Block_erase_histogram[channel_cntr][chip_cntr][die_cntr][plane_cntr][i] = 0;
						}
					}
				}
			}
		}

		IssuedReadCMD = 0; IssuedCopybackReadCMD = 0; IssuedInterleaveReadCMD = 0; IssuedMultiplaneReadCMD = 0; IssuedMultiplaneCopybackReadCMD = 0;
		IssuedProgramCMD = 0; IssuedInterleaveProgramCMD = 0; IssuedMultiplaneProgramCMD = 0; IssuedMultiplaneCopybackProgramCMD = 0; IssuedInterleaveMultiplaneProgramCMD = 0; IssuedSuspendProgramCMD = 0; IssuedCopybackProgramCMD = 0;
		IssuedEraseCMD = 0; IssuedInterleaveEraseCMD = 0; IssuedMultiplaneEraseCMD = 0; IssuedInterleaveMultiplaneEraseCMD = 0;
		IssuedSuspendEraseCMD = 0;
		Total_flash_reads_for_mapping = 0; Total_flash_writes_for_mapping = 0; 
		Program_LSB_Count = 0; Program_CSB_Count = 0; Program_MSB_Count = 0;
		CMT_hits = 0; readTR_CMT_hits = 0; writeTR_CMT_hits = 0;
		CMT_miss = 0; readTR_CMT_miss = 0; writeTR_CMT_miss = 0;
		total_CMT_queries = 0; total_readTR_CMT_queries = 0; total_writeTR_CMT_queries = 0;

		Total_gc_executions = 0;  Total_page_movements_for_gc = 0;
		Baseline_GC_Executions_AfterPreconditioning = 0;
		Baseline_GC_Page_Movements_AfterPreconditioning = 0;
		Total_wl_executions = 0;  Total_page_movements_for_wl = 0;

		for (stream_id_type stream_id = 0; stream_id < MAX_SUPPORT_STREAMS; stream_id++) {
			Total_flash_reads_for_mapping_per_stream[stream_id] = 0;
			Total_flash_writes_for_mapping_per_stream[stream_id] = 0;
			CMT_hits_per_stream[stream_id] = 0; readTR_CMT_hits_per_stream[stream_id] = 0; writeTR_CMT_hits_per_stream[stream_id] = 0;
			CMT_miss_per_stream[stream_id] = 0; readTR_CMT_miss_per_stream[stream_id] = 0;  writeTR_CMT_miss_per_stream[stream_id] = 0;
			total_CMT_queries_per_stream[stream_id] = 0; total_readTR_CMT_queries_per_stream[stream_id] = 0; total_writeTR_CMT_queries_per_stream[stream_id] = 0;
			Total_gc_executions_per_stream[stream_id] = 0;
			Total_gc_page_movements_per_stream[stream_id] = 0;
			Total_wl_executions_per_stream[stream_id] = 0;
			Total_wl_page_movements_per_stream[stream_id] = 0;
		}
        Init_GC_Log();
        Init_BlockPool_HotCold_Log();
	}

    // Page type counts
    unsigned long Stats::Program_LSB_Count = 0;
    unsigned long Stats::Program_CSB_Count = 0;
    unsigned long Stats::Program_MSB_Count = 0;

    void Stats::Dump_page_type_counts_csv()
    {
        FILE* f = fopen("page_type_counts.csv", "w");
        if (!f) return;
        fprintf(f, "type,count\n");
        fprintf(f, "LSB,%lu\n", Program_LSB_Count);
        fprintf(f, "CSB,%lu\n", Program_CSB_Count);
        fprintf(f, "MSB,%lu\n", Program_MSB_Count);
        fclose(f);
    }

    // GC Event Logging
    FILE* Stats::gc_log_file = NULL;

    void Stats::Init_GC_Log() {
        if (gc_log_file) fclose(gc_log_file);
        gc_log_file = fopen("gc_event_log.csv", "w");
        if (gc_log_file) {
            fprintf(gc_log_file, "Event_Type,Time\n"); // 0: Start, 1: End
        }
    }

    void Stats::Record_GC_Start(double time) {
        if (gc_log_file) {
            fprintf(gc_log_file, "0,%.6f\n", time);
        }
    }

    void Stats::Record_GC_End(double time) {
        if (gc_log_file) {
            fprintf(gc_log_file, "1,%.6f\n", time);
        }
    }

    void Stats::Close_GC_Log() {
        if (gc_log_file) {
            fclose(gc_log_file);
            gc_log_file = NULL;
        }
    }

    // Block Pool Hot/Cold Ratio Logging
    FILE* Stats::blockpool_hotcold_log_file = NULL;
    std::string Stats::workload_file_path = "";

    void Stats::SetWorkloadPath(const std::string& path) {
        workload_file_path = path;
    }

    void Stats::Init_BlockPool_HotCold_Log() {
        if (blockpool_hotcold_log_file) fclose(blockpool_hotcold_log_file);
        
        // 워크로드 파일 경로에서 파일명 추출
        std::string log_filename = "blockpool_hotcold_ratio.csv";
        if (!workload_file_path.empty()) {
            // 경로에서 파일명 추출 (마지막 '/' 또는 '\' 이후)
            size_t last_slash = workload_file_path.find_last_of("/\\");
            std::string filename = (last_slash != std::string::npos) 
                ? workload_file_path.substr(last_slash + 1) 
                : workload_file_path;
            
            // 확장자 제거
            size_t last_dot = filename.find_last_of(".");
            if (last_dot != std::string::npos) {
                filename = filename.substr(0, last_dot);
            }
            
            // 로그 파일명 생성
            log_filename = filename + "_blockpool_hotcold_ratio.csv";
        }
        
        blockpool_hotcold_log_file = fopen(log_filename.c_str(), "w");
        if (blockpool_hotcold_log_file) {
            fprintf(blockpool_hotcold_log_file, "Time(us),HotPool_HotPages,HotPool_ColdPages,HotPool_TotalPages,HotPool_HotRatio,WarmPool_HotPages,WarmPool_ColdPages,WarmPool_TotalPages,WarmPool_HotRatio,ColdPool_HotPages,ColdPool_ColdPages,ColdPool_TotalPages,ColdPool_HotRatio\n");
        }
    }

    void Stats::Record_BlockPool_HotCold_Ratio(double time,
        unsigned int hot_pool_hot_pages, unsigned int hot_pool_cold_pages, unsigned int hot_pool_total_pages,
        unsigned int warm_pool_hot_pages, unsigned int warm_pool_cold_pages, unsigned int warm_pool_total_pages,
        unsigned int cold_pool_hot_pages, unsigned int cold_pool_cold_pages, unsigned int cold_pool_total_pages) {
        if (blockpool_hotcold_log_file) {
            double hot_pool_ratio = hot_pool_total_pages > 0 ? (double)hot_pool_hot_pages / (double)hot_pool_total_pages : 0.0;
            double warm_pool_ratio = warm_pool_total_pages > 0 ? (double)warm_pool_hot_pages / (double)warm_pool_total_pages : 0.0;
            double cold_pool_ratio = cold_pool_total_pages > 0 ? (double)cold_pool_hot_pages / (double)cold_pool_total_pages : 0.0;
            fprintf(blockpool_hotcold_log_file, "%.6f,%u,%u,%u,%.4f,%u,%u,%u,%.4f,%u,%u,%u,%.4f\n",
                time,
                hot_pool_hot_pages, hot_pool_cold_pages, hot_pool_total_pages, hot_pool_ratio,
                warm_pool_hot_pages, warm_pool_cold_pages, warm_pool_total_pages, warm_pool_ratio,
                cold_pool_hot_pages, cold_pool_cold_pages, cold_pool_total_pages, cold_pool_ratio);
        }
    }

    void Stats::Close_BlockPool_HotCold_Log() {
        if (blockpool_hotcold_log_file) {
            fclose(blockpool_hotcold_log_file);
            blockpool_hotcold_log_file = NULL;
        }
    }

	void Stats::Clear_stats(unsigned int channel_no, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block, unsigned int max_allowed_block_erase_count)
	{
        Close_GC_Log();
        Close_BlockPool_HotCold_Log();
		for (unsigned int channel_cntr = 0; channel_cntr < channel_no; channel_cntr++) {
			for (unsigned int chip_cntr = 0; chip_cntr < chip_no_per_channel; chip_cntr++) {
				for (unsigned int die_cntr = 0; die_cntr < die_no_per_chip; die_cntr++) {
					for (unsigned int plane_cntr = 0; plane_cntr < plane_no_per_die; plane_cntr++) {
						delete[] Block_erase_histogram[channel_cntr][chip_cntr][die_cntr][plane_cntr];
					}
					delete[] Block_erase_histogram[channel_cntr][chip_cntr][die_cntr];
				}
				delete[] Block_erase_histogram[channel_cntr][chip_cntr];
			}
			delete[] Block_erase_histogram[channel_cntr];
		}
		delete[] Block_erase_histogram;
	}
}
