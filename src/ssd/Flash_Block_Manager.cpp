
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "Flash_Block_Manager.h"
#include "Stats.h"

namespace SSD_Components
{
	Flash_Block_Manager::Flash_Block_Manager(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: Flash_Block_Manager_Base(gc_and_wl_unit, max_allowed_block_erase_count, total_concurrent_streams_no, channel_count, chip_no_per_channel, die_no_per_chip,
			plane_no_per_die, block_no_per_plane, page_no_per_block)
	{
	}

	Flash_Block_Manager::~Flash_Block_Manager()
	{
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address, SSD_Components::BlockHotness hotness)
	{
		// 1) Locate plane record
    		PlaneBookKeepingType &pbk =
        		plane_manager[page_address.ChannelID]
                     		[page_address.ChipID]
                     		[page_address.DieID]
                     		[page_address.PlaneID];

    		// 2) Select the appropriate write frontier based on hotness
    		Block_Pool_Slot_Type *frontier = nullptr;
    		switch (hotness) {
        		case BlockHotness::HOT:
            			frontier = pbk.Data_hot_wf[stream_id];
            			break;
        		case BlockHotness::WARM:
            			frontier = pbk.Data_warm_wf[stream_id];
            			break;
        		case BlockHotness::COLD:
            			frontier = pbk.Data_cold_wf[stream_id];
            			break;
    		}

    		// 3) Update bookkeeping counts
    		pbk.Valid_pages_count++;
    		pbk.Free_pages_count--;

    		// 4) Assign PPA
    		page_address.BlockID = frontier->BlockID;
    		page_address.PageID  = frontier->Current_page_write_index++;
    		program_transaction_issued(page_address);

    		// 5) If block is full, allocate a new frontier from same pool
    		if (frontier->Current_page_write_index == pages_no_per_block) {
        		Block_Pool_Slot_Type *new_frontier =
            		pbk.Get_a_free_block(stream_id, false, hotness);
        		switch (hotness) {
            		case BlockHotness::HOT:
                		pbk.Data_hot_wf[stream_id] = new_frontier;
                		break;
            		case BlockHotness::WARM:
                		pbk.Data_warm_wf[stream_id] = new_frontier;
                		break;
            		case BlockHotness::COLD:
                		pbk.Data_cold_wf[stream_id] = new_frontier;
                		break;
        		}
        		gc_and_wl_unit->Check_gc_required(
            		pbk.Get_free_block_pool_size(),
            		page_address);
    		}

    		// 6) Sanity check
    		pbk.Check_bookkeeping_correctness(page_address);
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address, SSD_Components::BlockHotness hotness)
	{
		// 1) Locate plane record
    		PlaneBookKeepingType &pbk =
        		plane_manager[page_address.ChannelID]
                     		[page_address.ChipID]
                     		[page_address.DieID]
                     		[page_address.PlaneID];

    		// 2) Select the appropriate GC write frontier based on hotness
    		Block_Pool_Slot_Type *frontier = nullptr;
    		switch (hotness) {
        		case BlockHotness::HOT:
            			frontier = pbk.GC_hot_wf[stream_id];
            			break;
        		case BlockHotness::WARM:
            			frontier = pbk.GC_warm_wf[stream_id];
            			break;
        		case BlockHotness::COLD:
            			frontier = pbk.GC_cold_wf[stream_id];
            			break;
    		}

    		// 3) Update bookkeeping counts
    		pbk.Valid_pages_count++;
    		pbk.Free_pages_count--;

    		// 4) Assign PPA
    		page_address.BlockID = frontier->BlockID;
    		page_address.PageID  = frontier->Current_page_write_index++;

    		// 5) The current GC frontier is written to the end
    		if (frontier->Current_page_write_index == pages_no_per_block) {
        		// Assign a new GC write frontier block from same pool
        		Block_Pool_Slot_Type *new_frontier =
            		pbk.Get_a_free_block(stream_id, false, hotness);
        		switch (hotness) {
            		case BlockHotness::HOT:
                		pbk.GC_hot_wf[stream_id]  = new_frontier;
                		break;
            		case BlockHotness::WARM:
                		pbk.GC_warm_wf[stream_id] = new_frontier;
                		break;
            		case BlockHotness::COLD:
                		pbk.GC_cold_wf[stream_id] = new_frontier;
                		break;
        		}
        		gc_and_wl_unit->Check_gc_required(
            		pbk.Get_free_block_pool_size(),
            		page_address);
    		}

    		// 6) Sanity check
    		pbk.Check_bookkeeping_correctness(page_address);
	}
	
	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses, SSD_Components::BlockHotness hotness)
	{
		// 1) Locate plane record
    		PlaneBookKeepingType &pbk =
        		plane_manager[plane_address.ChannelID]
                     		[plane_address.ChipID]
                     		[plane_address.DieID]
                     		[plane_address.PlaneID];

    		// 2) Select the appropriate data frontier based on hotness
    		Block_Pool_Slot_Type *frontier = nullptr;
    		switch (hotness) {
        		case BlockHotness::HOT:
            			frontier = pbk.Data_hot_wf[stream_id];
            			break;
        		case BlockHotness::WARM:
            			frontier = pbk.Data_warm_wf[stream_id];
            			break;
        		case BlockHotness::COLD:
            			frontier = pbk.Data_cold_wf[stream_id];
            			break;
    		}

    		// Ensure block is fresh (erased)
    		if (frontier->Current_page_write_index > 0) {
        		PRINT_ERROR("Illegal operation: preconditioning expects an erased block frontier!");
    		}

    		// 3) Assign physical addresses for valid precondition pages
    		for (size_t i = 0; i < page_addresses.size(); ++i) {
        		pbk.Valid_pages_count++;
        		pbk.Free_pages_count--;
        		page_addresses[i].BlockID = frontier->BlockID;
        		page_addresses[i].PageID  = frontier->Current_page_write_index++;
        		pbk.Check_bookkeeping_correctness(page_addresses[i]);
    		}

    		// 4) Invalidate remaining pages
    		NVM::FlashMemory::Physical_Page_Address target_address(plane_address);
    		while (frontier->Current_page_write_index < pages_no_per_block) {
        		pbk.Free_pages_count--;
        		target_address.BlockID = frontier->BlockID;
        		target_address.PageID  = frontier->Current_page_write_index++;
        		Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
        		pbk.Check_bookkeeping_correctness(target_address);
    		}

    		// 5) Allocate a new frontier from same hotness pool
    		Block_Pool_Slot_Type *new_frontier =
        		pbk.Get_a_free_block(stream_id, false, hotness);
    		switch (hotness) {
        		case BlockHotness::HOT:
            			pbk.Data_hot_wf[stream_id]  = new_frontier;
            			break;
        		case BlockHotness::WARM:
            			pbk.Data_warm_wf[stream_id] = new_frontier;
            			break;
        		case BlockHotness::COLD:
            			pbk.Data_cold_wf[stream_id] = new_frontier;
            			break;
    		}
	}

	void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Valid_pages_count++;
		plane_record->Free_pages_count--;
		page_address.BlockID = plane_record->Translation_wf[streamID]->BlockID;
		page_address.PageID = plane_record->Translation_wf[streamID]->Current_page_write_index++;
		program_transaction_issued(page_address);

		//The current write frontier block for translation pages is written to the end
		if (plane_record->Translation_wf[streamID]->Current_page_write_index == pages_no_per_block) {
			//Assign a new write frontier block
			plane_record->Translation_wf[streamID] = plane_record->Get_a_free_block(streamID, true, BlockHotness::HOT);
			if (!is_for_gc) {
				gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
			}
		}
		plane_record->Check_bookkeeping_correctness(page_address);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		plane_record->Valid_pages_count--;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	inline void Flash_Block_Manager::Invalidate_page_in_block_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType* plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Invalid_pages_count++;
		plane_record->Valid_pages_count--;
		if (plane_record->Blocks[page_address.BlockID].Stream_id != stream_id) {
			PRINT_ERROR("Inconsistent status in the Invalidate_page_in_block function! The accessed block is not allocated to stream " << stream_id)
		}
		plane_record->Blocks[page_address.BlockID].Invalid_page_count++;
		plane_record->Blocks[page_address.BlockID].Invalid_page_bitmap[page_address.PageID / 64] |= ((uint64_t)0x1) << (page_address.PageID % 64);
	}

	void Flash_Block_Manager::Add_erased_block_to_pool(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		Block_Pool_Slot_Type* block = &(plane_record->Blocks[block_address.BlockID]);
		plane_record->Free_pages_count += block->Invalid_page_count;
		plane_record->Invalid_pages_count -= block->Invalid_page_count;

		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]--;
		block->Erase();
		Stats::Block_erase_histogram[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID][block->Erase_count]++;
		plane_record->Add_to_free_block_pool(block, gc_and_wl_unit->Use_dynamic_wearleveling());
		plane_record->Check_bookkeeping_correctness(block_address);
	}

	inline unsigned int Flash_Block_Manager::Get_pool_size(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return (unsigned int) plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID].Free_block_pool.size();
	}
}
