
#include "../nvm_chip/flash_memory/Physical_Page_Address.h"
#include "Flash_Block_Manager.h"
#include "../exec/Flash_Parameter_Set.h"
#include "Stats.h"

namespace SSD_Components
{
    static inline bool is_page_allowed_for_hot_warm(flash_page_ID_type page_id, Block_Pool_Slot_Type* block)
    {
        // COLD blocks: no restriction
        if (block->Temperature == BlockTemperature::COLD) return true;

        // HOT or WARM: apply provided latencyType rules
        switch (Flash_Parameter_Set::Flash_Technology) {
        case Flash_Technology_Type::SLC:
            // Only LSB pages exist; allow all
            return true;
        case Flash_Technology_Type::MLC:
        {
            int latencyType = page_id % 2; // 0: LSB, 1: MSB
            return latencyType == 0; // allow LSB only
        }
        case Flash_Technology_Type::TLC:
        {
            // From Yaakobi et al., ICNC 2012
            int latencyType = 0;
            if (page_id <= 5) latencyType = 0; // LSB
            else if (page_id <= 7) latencyType = 1; // CSB
            else latencyType = (((int)page_id - 8) >> 1) % 3; // 0: LSB, 1: CSB, 2: MSB
            return latencyType != 2; // allow LSB, CSB; skip MSB
        }
        default:
            return true;
        }
    }
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

    void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_user_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
        PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
        Block_Pool_Slot_Type* wf = plane_record->Data_wf[stream_id];

        // If HOT/WARM, skip disallowed pages until we find an allowed one
        if (wf->Temperature != BlockTemperature::COLD) {
            NVM::FlashMemory::Physical_Page_Address skip_addr(page_address);
            // For HOT on TLC: do LSB-first, then CSB scan
            if (wf->Temperature == BlockTemperature::WARM || wf->Temperature == BlockTemperature::HOT) {
                if (Flash_Parameter_Set::Flash_Technology == Flash_Technology_Type::TLC) {
                    // Try to find next LSB hole from current index
                    bool moved = false;
                    for (flash_page_ID_type pid = wf->Current_page_write_index; pid < pages_no_per_block; pid++) {
                        int lt = 0;
                        if (pid <= 5) lt = 0; else if (pid <= 7) lt = 1; else lt = (((int)pid - 8) >> 1) % 3;
                        bool is_lsb = (lt == 0);
                        bool is_invalid = (wf->Invalid_page_bitmap[pid / 64] & (((uint64_t)1) << (pid % 64))) != 0;
                        if (is_lsb && is_invalid) { wf->Current_page_write_index = pid; moved = true; break; }
                    }
                    if (!moved) {
                        // No LSB holes ahead; try CSB holes
                        for (flash_page_ID_type pid = wf->Current_page_write_index; pid < pages_no_per_block; pid++) {
                            int lt = 0;
                            if (pid <= 5) lt = 0; else if (pid <= 7) lt = 1; else lt = (((int)pid - 8) >> 1) % 3;
                            bool is_csb = (lt == 1);
                            bool is_invalid = (wf->Invalid_page_bitmap[pid / 64] & (((uint64_t)1) << (pid % 64))) != 0;
                            if (is_csb && is_invalid) { wf->Current_page_write_index = pid; moved = true; break; }
                        }
                        if (!moved) {
                            // Fallback to previous behavior: advance until allowed type
                            while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                                skip_addr.BlockID = wf->BlockID;
                                skip_addr.PageID = wf->Current_page_write_index++;
                            }
                        }
                    }
                } else {
                    while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                        skip_addr.BlockID = wf->BlockID;
                        skip_addr.PageID = wf->Current_page_write_index++;
                    }
                }
            } else {
                while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                    skip_addr.BlockID = wf->BlockID;
                    skip_addr.PageID = wf->Current_page_write_index++;
                }
            }
        }

        // If COLD, ensure we don't overwrite already-programmed valid pages when write index was rewound
        if (wf->Temperature == BlockTemperature::COLD) {
            while (wf->Current_page_write_index < pages_no_per_block) {
                bool is_valid = ((wf->Invalid_page_bitmap[wf->Current_page_write_index / 64] & (((uint64_t)1) << (wf->Current_page_write_index % 64))) == 0);
                if (is_valid) {
                    wf->Current_page_write_index++;
                } else {
                    break;
                }
            }
        }

        // If block ended due to skipping, rotate write frontier
        if (wf->Current_page_write_index >= pages_no_per_block) {
            plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
            gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
            wf = plane_record->Data_wf[stream_id];
        }

        plane_record->Valid_pages_count++;
        plane_record->Free_pages_count--;
        page_address.BlockID = wf->BlockID;
        page_address.PageID = wf->Current_page_write_index++;
        program_transaction_issued(page_address);

		// //The current write frontier block is written to the end
        // if(plane_record->Data_wf[stream_id]->Current_page_write_index >= pages_no_per_block) {
		// 	//Assign a new write frontier block
		// 	plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
		// 	gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		// }

		plane_record->Check_bookkeeping_correctness(page_address);
	}

    void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_gc_write(const stream_id_type stream_id, NVM::FlashMemory::Physical_Page_Address& page_address)
	{
        PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
        Block_Pool_Slot_Type* wf = plane_record->GC_wf[stream_id];

        if (wf->Temperature != BlockTemperature::COLD) {
            NVM::FlashMemory::Physical_Page_Address skip_addr(page_address);
            if ((wf->Temperature == BlockTemperature::HOT || wf->Temperature == BlockTemperature::WARM) && Flash_Parameter_Set::Flash_Technology == Flash_Technology_Type::TLC) {
                bool moved = false;
                // Prefer LSB hole
                for (flash_page_ID_type pid = wf->Current_page_write_index; pid < pages_no_per_block; pid++) {
                    int lt = 0;
                    if (pid <= 5) lt = 0; else if (pid <= 7) lt = 1; else lt = (((int)pid - 8) >> 1) % 3;
                    bool is_lsb = (lt == 0);
                    bool is_invalid = (wf->Invalid_page_bitmap[pid / 64] & (((uint64_t)1) << (pid % 64))) != 0;
                    if (is_lsb && is_invalid) { wf->Current_page_write_index = pid; moved = true; break; }
                }
                if (!moved) {
                    // Then CSB hole
                    for (flash_page_ID_type pid = wf->Current_page_write_index; pid < pages_no_per_block; pid++) {
                        int lt = 0;
                        if (pid <= 5) lt = 0; else if (pid <= 7) lt = 1; else lt = (((int)pid - 8) >> 1) % 3;
                        bool is_csb = (lt == 1);
                        bool is_invalid = (wf->Invalid_page_bitmap[pid / 64] & (((uint64_t)1) << (pid % 64))) != 0;
                        if (is_csb && is_invalid) { wf->Current_page_write_index = pid; moved = true; break; }
                    }
                }
                if (!moved) {
                    while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                        skip_addr.BlockID = wf->BlockID;
                        skip_addr.PageID = wf->Current_page_write_index++;
                    }
                }
            } else {
                while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                    skip_addr.BlockID = wf->BlockID;
                    skip_addr.PageID = wf->Current_page_write_index++;
                }
            }
        }

        if (wf->Current_page_write_index >= pages_no_per_block) {
            plane_record->GC_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
            gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
            wf = plane_record->GC_wf[stream_id];
        }

        plane_record->Valid_pages_count++;
        plane_record->Free_pages_count--;
        page_address.BlockID = wf->BlockID;
        page_address.PageID = wf->Current_page_write_index++;

		
		// //The current write frontier block is written to the end
        // if (plane_record->GC_wf[stream_id]->Current_page_write_index >= pages_no_per_block) {
		// 	//Assign a new write frontier block
		// 	plane_record->GC_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
		// 	gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
		// }
		plane_record->Check_bookkeeping_correctness(page_address);
	}
	
	void Flash_Block_Manager::Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning(const stream_id_type stream_id, const NVM::FlashMemory::Physical_Page_Address& plane_address, std::vector<NVM::FlashMemory::Physical_Page_Address>& page_addresses)
	{
		if(page_addresses.size() > pages_no_per_block) {
			PRINT_ERROR("Error while precondition a physical block: the size of the address list is larger than the pages_no_per_block!")
		}
			
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];
        // 스킵-소모에서 사용할 임시 주소 변수
		NVM::FlashMemory::Physical_Page_Address target_address(plane_address);
		if (plane_record->Data_wf[stream_id]->Current_page_write_index > 0) {
			PRINT_ERROR("Illegal operation: the Allocate_Pages_in_block_and_invalidate_remaining_for_preconditioning function should be executed for an erased block!")
		}

		//Assign physical addresses
		for (int i = 0; i < page_addresses.size(); i++) {
			// HOT/WARM 블록이면 허용되지 않는 페이지를 스킵-소모 처리
			Block_Pool_Slot_Type* wf = plane_record->Data_wf[stream_id];
			while (wf->Temperature != BlockTemperature::COLD && wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
				target_address.BlockID = wf->BlockID;
				target_address.PageID = wf->Current_page_write_index++;
				plane_record->Free_pages_count--;
				Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
				plane_record->Check_bookkeeping_correctness(plane_address);
				// 스킵으로 블록이 끝난 경우 새 write frontier로 교체
				if (wf->Current_page_write_index >= pages_no_per_block) {
					plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
					wf = plane_record->Data_wf[stream_id];
				}
			}
			plane_record->Valid_pages_count++;
			plane_record->Free_pages_count--;
			page_addresses[i].BlockID = plane_record->Data_wf[stream_id]->BlockID;
			page_addresses[i].PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			plane_record->Check_bookkeeping_correctness(page_addresses[i]);
		}

		//Invalidate the remaining pages in the block
		while (plane_record->Data_wf[stream_id]->Current_page_write_index < pages_no_per_block) {
			plane_record->Free_pages_count--;
			target_address.BlockID = plane_record->Data_wf[stream_id]->BlockID;
			target_address.PageID = plane_record->Data_wf[stream_id]->Current_page_write_index++;
			Invalidate_page_in_block_for_preconditioning(stream_id, target_address);
			plane_record->Check_bookkeeping_correctness(plane_address);
		}

		//Update the write frontier
		plane_record->Data_wf[stream_id] = plane_record->Get_a_free_block(stream_id, false);
	}

    void Flash_Block_Manager::Allocate_block_and_page_in_plane_for_translation_write(const stream_id_type streamID, NVM::FlashMemory::Physical_Page_Address& page_address, bool is_for_gc)
	{
        PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
        Block_Pool_Slot_Type* wf = plane_record->Translation_wf[streamID];

        if (wf->Temperature != BlockTemperature::COLD) {
            NVM::FlashMemory::Physical_Page_Address skip_addr(page_address);
            while (wf->Current_page_write_index < pages_no_per_block && !is_page_allowed_for_hot_warm(wf->Current_page_write_index, wf)) {
                skip_addr.BlockID = wf->BlockID;
                skip_addr.PageID = wf->Current_page_write_index++;
            }
        }

        if (wf->Current_page_write_index >= pages_no_per_block) {
            plane_record->Translation_wf[streamID] = plane_record->Get_a_free_block(streamID, true);
            if (!is_for_gc) {
                gc_and_wl_unit->Check_gc_required(plane_record->Get_free_block_pool_size(), page_address);
            }
            wf = plane_record->Translation_wf[streamID];
        }

        plane_record->Valid_pages_count++;
        plane_record->Free_pages_count--;
        page_address.BlockID = wf->BlockID;
        page_address.PageID = wf->Current_page_write_index++;
        program_transaction_issued(page_address);

		//The current write frontier block for translation pages is written to the end
        if (plane_record->Translation_wf[streamID]->Current_page_write_index >= pages_no_per_block) {
			//Assign a new write frontier block
			plane_record->Translation_wf[streamID] = plane_record->Get_a_free_block(streamID, true);
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
