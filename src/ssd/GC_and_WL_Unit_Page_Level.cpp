#include <math.h>
#include <vector>
#include <set>
#include "GC_and_WL_Unit_Page_Level.h"
#include "Flash_Block_Manager.h"
#include "FTL.h"
#include "Stats.h"

namespace SSD_Components
{

	GC_and_WL_Unit_Page_Level::GC_and_WL_Unit_Page_Level(const sim_object_id_type& id,
		Address_Mapping_Unit_Base* address_mapping_unit, Flash_Block_Manager_Base* block_manager, TSU_Base* tsu, NVM_PHY_ONFI* flash_controller, 
		GC_Block_Selection_Policy_Type block_selection_policy, double gc_threshold, bool preemptible_gc_enabled, double gc_hard_threshold,
		unsigned int ChannelCount, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int Page_no_per_block, unsigned int sectors_per_page, 
		bool use_copyback, double rho, unsigned int max_ongoing_gc_reqs_per_plane, bool dynamic_wearleveling_enabled, bool static_wearleveling_enabled, unsigned int static_wearleveling_threshold, int seed)
		: GC_and_WL_Unit_Base(id, address_mapping_unit, block_manager, tsu, flash_controller, block_selection_policy, gc_threshold, preemptible_gc_enabled, gc_hard_threshold,
		ChannelCount, chip_no_per_channel, die_no_per_chip, plane_no_per_die, block_no_per_plane, Page_no_per_block, sectors_per_page, use_copyback, rho, max_ongoing_gc_reqs_per_plane, 
			dynamic_wearleveling_enabled, static_wearleveling_enabled, static_wearleveling_threshold, seed)
	{
		rga_set_size = (unsigned int)log2(block_no_per_plane);
	}
	
	bool GC_and_WL_Unit_Page_Level::GC_is_in_urgent_mode(const NVM::FlashMemory::Flash_Chip* chip)
	{
		if (!preemptible_gc_enabled) {
			return true;
		}

		NVM::FlashMemory::Physical_Page_Address addr;
		addr.ChannelID = chip->ChannelID; addr.ChipID = chip->ChipID;
		for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++) {
			for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++) {
				addr.DieID = die_id; addr.PlaneID = plane_id;
				auto* pbke = block_manager->Get_plane_bookkeeping_entry(addr);
				if (pbke->Fully_written_block_count >= block_pool_gc_hard_threshold)
					return true;
			}
		}

		return false;
	}

	void GC_and_WL_Unit_Page_Level::Check_gc_required(const unsigned int plane_fully_written_blocks, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (plane_fully_written_blocks < block_pool_gc_threshold) {
			// PRINT_MESSAGE("[GC_Check] return plane_full<threshold plane=" << plane_address.ChannelID << ":" << plane_address.ChipID
			// 	<< ":" << plane_address.DieID << ":" << plane_address.PlaneID
			// 	<< " plane_full=" << plane_fully_written_blocks
			// 	<< " threshold=" << block_pool_gc_threshold);
			return;
		}

		// PRINT_MESSAGE("[GC_Check] enter plane=" << plane_address.ChannelID << ":" << plane_address.ChipID
		// 	<< ":" << plane_address.DieID << ":" << plane_address.PlaneID
		// 	<< " plane_full=" << plane_fully_written_blocks);

		flash_block_ID_type gc_candidate_block_id = block_manager->Get_coldest_block_id(plane_address);
		PlaneBookKeepingType* pbke = block_manager->Get_plane_bookkeeping_entry(plane_address);

			if (pbke->Ongoing_erase_operations.size() >= max_ongoing_gc_reqs_per_plane) {
				PRINT_MESSAGE("[GC_Check] return ongoing_erase_limit plane=" << plane_address.ChannelID << ":" << plane_address.ChipID
					<< ":" << plane_address.DieID << ":" << plane_address.PlaneID
					<< " ongoing=" << pbke->Ongoing_erase_operations.size()
					<< " limit=" << max_ongoing_gc_reqs_per_plane);
				return;
			}

			switch (block_selection_policy) {
				case SSD_Components::GC_Block_Selection_Policy_Type::GREEDY://Find the set of blocks with maximum number of invalid pages and no free pages
				{
					// 초기 후보 블록 찾기: invalid page가 있고, reported_full이고, safe한 블록
					bool found_candidate = false;
					for (flash_block_ID_type block_id = 0; block_id < block_no_per_plane; block_id++) {
						SSD_Components::Block_Pool_Slot_Type* blk = &pbke->Blocks[block_id];
						if (blk->Invalid_page_count > 0
							&& blk->Reported_full
							&& pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
							&& is_safe_gc_wl_candidate(pbke, block_id)) {
							gc_candidate_block_id = block_id;
							found_candidate = true;
							break; // 첫 번째 유효한 후보를 찾으면 중단
						}
					}
					
					// 더 나은 후보 찾기: invalid page가 가장 많은 블록
					if (found_candidate) {
						for (flash_block_ID_type block_id = gc_candidate_block_id + 1; block_id < block_no_per_plane; block_id++) {
							SSD_Components::Block_Pool_Slot_Type* blk = &pbke->Blocks[block_id];
							SSD_Components::Block_Pool_Slot_Type* best_blk = &pbke->Blocks[gc_candidate_block_id];
							if (blk->Invalid_page_count > best_blk->Invalid_page_count
								&& blk->Reported_full
								&& pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
								&& is_safe_gc_wl_candidate(pbke, block_id)) {
								gc_candidate_block_id = block_id;
							}
						}
					} else {
						// 유효한 후보를 찾지 못한 경우
						PRINT_MESSAGE("[GC_Check] GREEDY: no valid candidate found (no invalid pages or not safe)");
						return;
					}
					break;
				}
				case SSD_Components::GC_Block_Selection_Policy_Type::RGA:
				{
					std::set<flash_block_ID_type> random_set;
					while (random_set.size() < rga_set_size) {
						flash_block_ID_type block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
						if (pbke->Ongoing_erase_operations.find(block_id) == pbke->Ongoing_erase_operations.end()
							&& is_safe_gc_wl_candidate(pbke, block_id)) {
							random_set.insert(block_id);
							}
					}
					gc_candidate_block_id = *random_set.begin();
					for(auto &block_id : random_set) {
						if (pbke->Blocks[block_id].Invalid_page_count > pbke->Blocks[gc_candidate_block_id].Invalid_page_count
							&& pbke->Blocks[block_id].Current_page_write_index == pages_no_per_block) {
							gc_candidate_block_id = block_id;
						}
					}
					break;
				}
				case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM:
				{
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					unsigned int repeat = 0;

					//A write frontier block should not be selected for garbage collection
					while (!is_safe_gc_wl_candidate(pbke, gc_candidate_block_id) && repeat++ < block_no_per_plane) {
						gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					}
					break;
				}
				case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_P:
				{
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					unsigned int repeat = 0;

					//A write frontier block or a block with free pages should not be selected for garbage collection
					while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block || !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
						&& repeat++ < block_no_per_plane) {
						gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					}
					break;
				}
				case SSD_Components::GC_Block_Selection_Policy_Type::RANDOM_PP:
				{
					gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					unsigned int repeat = 0;

					//The selected gc block should have a minimum number of invalid pages
					while ((pbke->Blocks[gc_candidate_block_id].Current_page_write_index < pages_no_per_block 
						|| pbke->Blocks[gc_candidate_block_id].Invalid_page_count < random_pp_threshold
						|| !is_safe_gc_wl_candidate(pbke, gc_candidate_block_id))
						&& repeat++ < block_no_per_plane) {
						gc_candidate_block_id = random_generator.Uniform_uint(0, block_no_per_plane - 1);
					}
					break;
				}
				case SSD_Components::GC_Block_Selection_Policy_Type::FIFO:
					gc_candidate_block_id = pbke->Block_usage_history.front();
					pbke->Block_usage_history.pop();
					break;
				default:
					break;
			}

			//This should never happen, but we check it here for safty
			if (pbke->Ongoing_erase_operations.find(gc_candidate_block_id) != pbke->Ongoing_erase_operations.end()) {
				PRINT_MESSAGE("[GC_Check] return candidate_has_ongoing plane=" << plane_address.ChannelID << ":" << plane_address.ChipID
					<< ":" << plane_address.DieID << ":" << plane_address.PlaneID
					<< " candidate=" << gc_candidate_block_id);
				return;
			}
			
			NVM::FlashMemory::Physical_Page_Address gc_candidate_address(plane_address);
			gc_candidate_address.BlockID = gc_candidate_block_id;
			Block_Pool_Slot_Type* block = &pbke->Blocks[gc_candidate_block_id];

			//No invalid page to erase
			if (block->Current_page_write_index == 0 || block->Invalid_page_count == 0) {
				PRINT_MESSAGE("[GC_Check] return candidate_has_no_invalid plane=" << plane_address.ChannelID << ":" << plane_address.ChipID
					<< ":" << plane_address.DieID << ":" << plane_address.PlaneID
					<< " candidate=" << gc_candidate_block_id
					<< " current_idx=" << block->Current_page_write_index
					<< " invalid=" << block->Invalid_page_count);
				return;
			}
			
			// GC 블록 선택 로그
			const char* temp_str = block->Temperature == SSD_Components::BlockTemperature::HOT ? "HOT" : 
			                       (block->Temperature == SSD_Components::BlockTemperature::WARM ? "WARM" : "COLD");
			// PRINT_MESSAGE("[GC_Select] Temp=" << temp_str << " Block=" << gc_candidate_block_id 
			//     << " Current_idx=" << block->Current_page_write_index 
			//     << " Invalid=" << block->Invalid_page_count)
			
			//Run the state machine to protect against race condition
            Stats::Record_GC_Start(Simulator->Time());
			block_manager->GC_WL_started(gc_candidate_address);
			pbke->Ongoing_erase_operations.insert(gc_candidate_block_id);
			address_mapping_unit->Set_barrier_for_accessing_physical_block(gc_candidate_address);//Lock the block, so no user request can intervene while the GC is progressing
			
			//If there are ongoing requests targeting the candidate block, the gc execution should be postponed
			if (block_manager->Can_execute_gc_wl(gc_candidate_address)) {
				Stats::Total_gc_executions++;
				tsu->Prepare_for_transaction_submit();

				NVM_Transaction_Flash_ER* gc_erase_tr = new NVM_Transaction_Flash_ER(Transaction_Source_Type::GC_WL, pbke->Blocks[gc_candidate_block_id].Stream_id, gc_candidate_address);
				//If there are some valid pages in block, then prepare flash transactions for page movement
				//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: block->Current_page_write_index = " << block->Current_page_write_index);
				//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: block->Invalid_page_count = " << block->Invalid_page_count);
				//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: block->Temperature = " << (int)block->Temperature);
                if (block->Current_page_write_index - block->Invalid_page_count > 0) {
					NVM_Transaction_Flash_RD* gc_read = NULL;
					NVM_Transaction_Flash_WR* gc_write = NULL;
					//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: upper = " << upper);
                    for (flash_page_ID_type pageID = 0; pageID < block->Current_page_write_index; pageID++) {
						//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: pageID = " << pageID);
						//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: block_manager->Is_page_valid(block, pageID) = " << block_manager->Is_page_valid(block, pageID));
						//PRINT_MESSAGE("GC_and_WL_Unit_Page_Level::Check_gc_required: block_manager->Should_consider_page_for_gc(block, pageID) = " << block_manager->Should_consider_page_for_gc(block, pageID));
                        if (block_manager->Is_page_valid(block, pageID) && block_manager->Should_consider_page_for_gc(block, pageID)) {
							Stats::Total_page_movements_for_gc++;
							gc_candidate_address.PageID = pageID;
                            if (use_copyback) {
								gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), NULL, 0, NULL, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::COPYBACK;
								tsu->Submit_transaction(gc_write);
							} else {
								gc_read = new NVM_Transaction_Flash_RD(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
									NO_LPA, address_mapping_unit->Convert_address_to_ppa(gc_candidate_address), gc_candidate_address, NULL, 0, NULL, 0, INVALID_TIME_STAMP);
                                // Create write and tag target temperature based on source block
                                gc_write = new NVM_Transaction_Flash_WR(Transaction_Source_Type::GC_WL, block->Stream_id, sector_no_per_page * SECTOR_SIZE_IN_BYTE,
                                    NO_LPA, NO_PPA, gc_candidate_address, NULL, 0, gc_read, 0, INVALID_TIME_STAMP);
								gc_write->ExecutionMode = WriteExecutionModeType::SIMPLE;
								gc_write->RelatedErase = gc_erase_tr;
								gc_read->RelatedWrite = gc_write;
								tsu->Submit_transaction(gc_read);//Only the read transaction would be submitted. The Write transaction is submitted when the read transaction is finished and the LPA of the target page is determined
							}
							gc_erase_tr->Page_movement_activities.push_back(gc_write);
						}
					}
				}
				block->Erase_transaction = gc_erase_tr;
				tsu->Submit_transaction(gc_erase_tr);

				tsu->Schedule();
			
		}
	}
}
