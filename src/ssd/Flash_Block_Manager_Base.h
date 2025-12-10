#ifndef BLOCK_POOL_MANAGER_BASE_H
#define BLOCK_POOL_MANAGER_BASE_H

#include <list>
#include <deque>
#include <cstdint>
#include <queue>
#include <set>
#include "../nvm_chip/flash_memory/FlashTypes.h"
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "GC_and_WL_Unit_Base.h"
#include "../nvm_chip/flash_memory/FlashTypes.h"

namespace SSD_Components
{
#define All_VALID_PAGE 0x0000000000000000ULL
	class GC_and_WL_Unit_Base;
	/*
	* Block_Service_Status is used to impelement a state machine for each physical block in order to
	* eliminate race conditions between GC page movements and normal user I/O requests.
	* Allowed transitions:
	* 1: IDLE -> GC, IDLE -> USER
	* 2: GC -> IDLE, GC -> GC_UWAIT
	* 3: USER -> IDLE, USER -> GC_USER
	* 4: GC_UWAIT -> GC, GC_UWAIT -> GC_UWAIT
	* 5: GC_USER -> GC
	*/
	enum class Block_Service_Status {IDLE, GC_WL, USER, GC_USER, GC_UWAIT, GC_USER_UWAIT};

	enum class BlockTemperature { HOT, WARM, COLD };
	
	class Block_Pool_Slot_Type
	{
	public:
		flash_block_ID_type BlockID;
		flash_page_ID_type Current_page_write_index;
		Block_Service_Status Current_status;
		unsigned int Invalid_page_count;
		unsigned int Erase_count;
		static unsigned int Page_vector_size;
		uint64_t* Invalid_page_bitmap;//A bit sequence that keeps track of valid/invalid status of pages in the block. A "0" means valid, and a "1" means invalid.
		bool Reported_full = false;		// true once counted as fully written for GC thresholds
		stream_id_type Stream_id = NO_STREAM;
		bool Holds_mapping_data = false;
		bool Has_ongoing_gc_wl = false;
		NVM_Transaction_Flash_ER* Erase_transaction;
		bool Hot_block = false;//Used for hot/cold separation mentioned in the "On the necessity of hot and cold data identification to reduce the write amplification in flash-based SSDs", Perf. Eval., 2014.
		int Ongoing_user_read_count;
		int Ongoing_user_program_count;
		BlockTemperature Temperature = BlockTemperature::WARM;
		void Erase();
	};

	class PlaneBookKeepingType
	{
	public:
		unsigned int Total_pages_count;
		unsigned int Free_pages_count;
		unsigned int Valid_pages_count;
		unsigned int Invalid_pages_count;
		Block_Pool_Slot_Type* Blocks;
		std::multimap<unsigned int, Block_Pool_Slot_Type*> Free_block_pool;
		// Per-temperature, per-stream open pools for partially filled blocks not in free pool
		std::deque<Block_Pool_Slot_Type*>* Open_hot_pool;   // size: total_concurrent_streams_no
		std::deque<Block_Pool_Slot_Type*>* Open_warm_pool;  // size: total_concurrent_streams_no
		std::deque<Block_Pool_Slot_Type*>* Open_cold_pool;  // size: total_concurrent_streams_no
		Block_Pool_Slot_Type** Data_wf, ** GC_wf; //The write frontier blocks for data and GC pages. MQSim adopts Double Write Frontier approach for user and GC writes which is shown very advantages in: B. Van Houdt, "On the necessity of hot and cold data identification to reduce the write amplification in flash - based SSDs", Perf. Eval., 2014
		Block_Pool_Slot_Type** Translation_wf; //The write frontier blocks for translation GC pages
		std::queue<flash_block_ID_type> Block_usage_history;//A fifo queue that keeps track of flash blocks based on their usage history
		std::set<flash_block_ID_type> Ongoing_erase_operations;
		unsigned int Fully_written_block_count = 0; // Number of blocks whose pages have all been consumed
		Block_Pool_Slot_Type* Get_a_free_block(stream_id_type stream_id, bool for_mapping_data);
		unsigned int Get_free_block_pool_size();
		void Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl);
	};

	class Flash_Block_Manager_Base
	{
		friend class Address_Mapping_Unit_Page_Level;
		friend class GC_and_WL_Unit_Page_Level;
		friend class GC_and_WL_Unit_Base;
	public:
		Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
			unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
			unsigned int block_no_per_plane, unsigned int page_no_per_block);
		virtual ~Flash_Block_Manager_Base();
		virtual void Allocate_block_and_page_in_plane_for_user_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& address, bool is_for_gc) = 0;
		virtual void Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses) = 0;
		virtual void Invalidate_page_in_block(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Invalidate_page_in_block_for_preconditioning(const stream_id_type streamID, const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual void Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& address) = 0;
		virtual unsigned int Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address) = 0;
		flash_block_ID_type Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		unsigned int Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		void Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* );
		PlaneBookKeepingType* Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address);
		bool Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if there is an ongoing gc for block_address
		bool Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address);//Checks if the gc request can be executed on block_address (there shouldn't be any ongoing user read/program requests targeting block_address)
		void GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		void GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address);//Updates the block bookkeeping record
		unsigned int Get_fully_written_block_count(const NVM::FlashMemory::Physical_Page_Address& plane_address) const;
		unsigned long Get_total_fully_written_block_count() const;
        unsigned int Get_total_concurrent_streams_no() const { return total_concurrent_streams_no; }
		void Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		void Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
		bool Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address);//Cheks if block has any ongoing program request
		bool Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id);//Make the page invalid in the block bookkeeping record
		// Selection helper for GC/WL: apply latency-type filtering (e.g., skip MSB) on HOT/WARM
		bool Should_consider_page_for_gc(const Block_Pool_Slot_Type* block, flash_page_ID_type page_id) const;
		// Check if block has allowed pages remaining from current write index
		bool Has_allowed_pages_remaining(Block_Pool_Slot_Type* block) const;
		// Check GC required for a plane (delegates to gc_and_wl_unit)
		void Check_gc_required_for_plane(const NVM::FlashMemory::Physical_Page_Address& plane_address);
	protected:
		PlaneBookKeepingType ****plane_manager;//Keeps track of plane block usage information
		GC_and_WL_Unit_Base *gc_and_wl_unit;
		unsigned int max_allowed_block_erase_count;
		unsigned int total_concurrent_streams_no;
		unsigned int channel_count;
		unsigned int chip_no_per_channel;
		unsigned int die_no_per_chip;
		unsigned int plane_no_per_die;
		unsigned int block_no_per_plane;
		unsigned int pages_no_per_block;
		void program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address);//Updates the block bookkeeping record
	};

	inline bool is_there_any_remain(Block_Pool_Slot_Type* block, flash_page_ID_type page_id) {
		// HOT/WARM에서는 MSB 페이지를 GC 대상으로 고려하지 않도록 필터링
		if (block->Temperature != BlockTemperature::COLD) {
			int latencyType = 0;
			if (page_id <= 5) latencyType = 0; // LSB
			else if (page_id <= 7) latencyType = 1; // CSB
			else latencyType = (((int)page_id - 8) >> 1) % 3; // 0: LSB, 1: CSB, 2: MSB
			return latencyType != 2; // MSB 제외
		}
		// COLD는 전체 페이지 고려
		return true;
	}

	inline void Log_plane_status(PlaneBookKeepingType* plane_record, const NVM::FlashMemory::Physical_Page_Address& plane_address, const char* label)
	{
		PRINT_MESSAGE("[" << label << "] plane="
			<< plane_address.ChannelID << ":" << plane_address.ChipID << ":" << plane_address.DieID << ":" << plane_address.PlaneID
			<< " fully_written=" << plane_record->Fully_written_block_count
			<< " free_blocks=" << plane_record->Get_free_block_pool_size()
			<< " free_pages=" << plane_record->Free_pages_count
			<< " valid_pages=" << plane_record->Valid_pages_count
			<< " invalid_pages=" << plane_record->Invalid_pages_count);
	}

	inline void mark_block_consumed(PlaneBookKeepingType* plane_record, Block_Pool_Slot_Type* block, unsigned int pages_no_per_block)
	{
		bool found = false;
		if (block == NULL) {
			return;
		}
		if (block->Temperature != BlockTemperature::COLD) {
			for (unsigned int i = block->Current_page_write_index; i < pages_no_per_block; i++) {
				if (is_there_any_remain(block, i)) {
					found = true;
					break;
				}
			}
		}
		else if(block->Current_page_write_index < pages_no_per_block){
			found = true;
		}
		if (!found && !block->Reported_full) {
			plane_record->Fully_written_block_count++;
			block->Reported_full = true;
		}
	}
}

#endif//!BLOCK_POOL_MANAGER_BASE_H
