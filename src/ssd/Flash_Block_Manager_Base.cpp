#include "Flash_Block_Manager.h"


namespace SSD_Components
{
	unsigned int Block_Pool_Slot_Type::Page_vector_size = 0;
	Flash_Block_Manager_Base::Flash_Block_Manager_Base(GC_and_WL_Unit_Base* gc_and_wl_unit, unsigned int max_allowed_block_erase_count, unsigned int total_concurrent_streams_no,
		unsigned int channel_count, unsigned int chip_no_per_channel, unsigned int die_no_per_chip, unsigned int plane_no_per_die,
		unsigned int block_no_per_plane, unsigned int page_no_per_block)
		: gc_and_wl_unit(gc_and_wl_unit), max_allowed_block_erase_count(max_allowed_block_erase_count), total_concurrent_streams_no(total_concurrent_streams_no),
		channel_count(channel_count), chip_no_per_channel(chip_no_per_channel), die_no_per_chip(die_no_per_chip), plane_no_per_die(plane_no_per_die),
		block_no_per_plane(block_no_per_plane), pages_no_per_block(page_no_per_block)
	{
		plane_manager = new PlaneBookKeepingType***[channel_count];
		

		float hot_ratio  = 0.20f;   // 예: 10%를 hot
		float warm_ratio = 0.30f;   // 예: 20%를 warm

		unsigned int blocks_per_plane = block_no_per_plane;
		//std::cout << "blocks_per_plane: " << blocks_per_plane << std::endl;

		unsigned int hot_count  = static_cast<unsigned int>(blocks_per_plane * hot_ratio);
		unsigned int warm_count = static_cast<unsigned int>(blocks_per_plane * warm_ratio);
		//std::cout << "hot_count: " << hot_count << " warm_count: " << warm_count << std::endl;
		for (unsigned int channelID = 0; channelID < channel_count; channelID++) {
			plane_manager[channelID] = new PlaneBookKeepingType**[chip_no_per_channel];
			for (unsigned int chipID = 0; chipID < chip_no_per_channel; chipID++) {
				plane_manager[channelID][chipID] = new PlaneBookKeepingType*[die_no_per_chip];
				for (unsigned int dieID = 0; dieID < die_no_per_chip; dieID++) {
					plane_manager[channelID][chipID][dieID] = new PlaneBookKeepingType[plane_no_per_die];

					//Initialize plane book keeping data structure
					for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
						plane_manager[channelID][chipID][dieID][planeID].Total_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Free_pages_count = block_no_per_plane * pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Valid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count = 0;
						plane_manager[channelID][chipID][dieID][planeID].pages_no_per_block = pages_no_per_block;
						plane_manager[channelID][chipID][dieID][planeID].Ongoing_erase_operations.clear();
						plane_manager[channelID][chipID][dieID][planeID].Blocks = new Block_Pool_Slot_Type[block_no_per_plane];
						
						//Initialize block pool for plane
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++) {
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].BlockID = blockID;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_page_write_index = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Current_status = Block_Service_Status::IDLE;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Holds_mapping_data = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Has_ongoing_gc_wl = false;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Erase_transaction = NULL;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_program_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Ongoing_user_read_count = 0;
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Last_access_time = 0;

							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].page_hotness_counters = new uint8_t[pages_no_per_block];
           					memset(plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].page_hotness_counters, 0, pages_no_per_block * sizeof(uint8_t));

							Block_Pool_Slot_Type::Page_vector_size = pages_no_per_block / (sizeof(uint64_t) * 8) + (pages_no_per_block % (sizeof(uint64_t) * 8) == 0 ? 0 : 1);
							plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap = new uint64_t[Block_Pool_Slot_Type::Page_vector_size];
							for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++) {
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[i] = All_VALID_PAGE;
							}
							BlockHotness h;
							if (blockID < hot_count) {
								h = BlockHotness::HOT;
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Hotness = h;
								
								// For HOT blocks, invalidate all CSB/MSB pages since only LSB pages are used
								for (flash_page_ID_type pageID = 0; pageID < pages_no_per_block; pageID++) {
									if (!is_lsb_page(pageID)) {
										plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
										plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count++;
										plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count++;
										plane_manager[channelID][chipID][dieID][planeID].Free_pages_count--;
									}
								}
							}
							else if (blockID < hot_count + warm_count) {
								h = BlockHotness::WARM;
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Hotness = h;
								
								// For WARM blocks, invalidate all CSB/MSB pages since only LSB pages are used
								for (flash_page_ID_type pageID = 0; pageID < pages_no_per_block; pageID++) {
									if (!is_lsb_page(pageID)) {
										plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
										plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Invalid_page_count++;
										plane_manager[channelID][chipID][dieID][planeID].Invalid_pages_count++;
										plane_manager[channelID][chipID][dieID][planeID].Free_pages_count--;
									}
								}
							}
							else {
								h = BlockHotness::COLD;
								plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID].Hotness = h;
							}
							plane_manager[channelID][chipID][dieID][planeID].Add_to_free_block_pool(&plane_manager[channelID][chipID][dieID][planeID].Blocks[blockID], false, h);
						}
						plane_manager[channelID][chipID][dieID][planeID].Data_hot_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Data_warm_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Data_cold_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].Translation_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].GC_hot_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].GC_warm_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						plane_manager[channelID][chipID][dieID][planeID].GC_cold_wf = new Block_Pool_Slot_Type*[total_concurrent_streams_no];
						for (unsigned int stream_cntr = 0; stream_cntr < total_concurrent_streams_no; stream_cntr++) {
							plane_manager[channelID][chipID][dieID][planeID].Data_hot_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::HOT);
							plane_manager[channelID][chipID][dieID][planeID].Translation_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, true, BlockHotness::WARM);
							plane_manager[channelID][chipID][dieID][planeID].GC_hot_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::HOT);
							plane_manager[channelID][chipID][dieID][planeID].Data_warm_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::WARM);
							plane_manager[channelID][chipID][dieID][planeID].Data_cold_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::COLD);
							plane_manager[channelID][chipID][dieID][planeID].GC_warm_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::WARM);
							plane_manager[channelID][chipID][dieID][planeID].GC_cold_wf[stream_cntr] = plane_manager[channelID][chipID][dieID][planeID].Get_a_free_block(stream_cntr, false, BlockHotness::COLD);

						}
					}
				}
			}
		}
	}

	Flash_Block_Manager_Base::~Flash_Block_Manager_Base() 
	{
		for (unsigned int channel_id = 0; channel_id < channel_count; channel_id++) {
			for (unsigned int chip_id = 0; chip_id < chip_no_per_channel; chip_id++) {
				for (unsigned int die_id = 0; die_id < die_no_per_chip; die_id++) {
					for (unsigned int plane_id = 0; plane_id < plane_no_per_die; plane_id++) {
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++) {
							delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Blocks[blockID].Invalid_page_bitmap;
							delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Blocks[blockID].page_hotness_counters; // addition
						}
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Blocks;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].GC_hot_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].GC_warm_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].GC_cold_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Data_hot_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Data_warm_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Data_cold_wf;
						delete[] plane_manager[channel_id][chip_id][die_id][plane_id].Translation_wf;
					}
					delete[] plane_manager[channel_id][chip_id][die_id];
				}
				delete[] plane_manager[channel_id][chip_id];
			}
			delete[] plane_manager[channel_id];
		}
		delete[] plane_manager;
	}

	void Flash_Block_Manager_Base::Set_GC_and_WL_Unit(GC_and_WL_Unit_Base* gcwl)
	{
		this->gc_and_wl_unit = gcwl;
	}

	void Block_Pool_Slot_Type::Erase(BlockHotness hotness)
	{
		Current_page_write_index = 0;
		Invalid_page_count = 0;
		Erase_count++;
		// For HOT and warm blocks, invalidate all CSB/MSB pages since only LSB pages are used
		if (hotness == BlockHotness::HOT || hotness == BlockHotness::WARM) {
			for (flash_page_ID_type pageID = 0; Block_Pool_Slot_Type::Page_vector_size; pageID++) {
				if (!is_lsb_page(pageID)) {
					Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
					Invalid_page_count++;
				}
			}
		}
		else {
			for (unsigned int i = 0; i < Block_Pool_Slot_Type::Page_vector_size; i++) {
				Invalid_page_bitmap[i] = All_VALID_PAGE;
			}
		}
		Stream_id = NO_STREAM;
		Holds_mapping_data = false;
		Erase_transaction = NULL;
		lsb_page_written_count = 0;  // Initialize LSB page count
	}

	void Flash_Block_Manager_Base::Change_block_status_to_hot(Block_Pool_Slot_Type* block, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		PlaneBookKeepingType* pbke = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];
		
		// remove from existing write frontier (if this block was a write frontier)
		for (stream_id_type stream_id = 0; stream_id < total_concurrent_streams_no; stream_id++) {
			if (pbke->Data_warm_wf[stream_id] == block) {
				// assign new warm block
				pbke->Data_warm_wf[stream_id] = pbke->Get_a_free_block(stream_id, false, BlockHotness::WARM);
				break;
			}
		}
		
		// change block status
		block->Hotness = BlockHotness::HOT;
		
		// if needed, add to hot pool (currently just changing status)
	}

	void Flash_Block_Manager_Base::Change_block_status_to_cold(Block_Pool_Slot_Type* block, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		PlaneBookKeepingType* pbke = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];
		
		// remove from existing write frontier (if this block was a write frontier)
		for (stream_id_type stream_id = 0; stream_id < total_concurrent_streams_no; stream_id++) {
			if (pbke->Data_warm_wf[stream_id] == block) {
				// assign new warm block
				pbke->Data_warm_wf[stream_id] = pbke->Get_a_free_block(stream_id, false, BlockHotness::WARM);
				break;
			}
		}
		
		// For COLD blocks, make all pages valid since all pages can be used
		for (flash_page_ID_type pageID = 0; pageID < pages_no_per_block; pageID++) {
			if (!is_lsb_page(pageID)) {
				// Mark CSB/MSB pages as valid (clear invalid bit)
				block->Invalid_page_bitmap[pageID / 64] &= ~(((uint64_t)0x1) << (pageID % 64));
				block->Invalid_page_count--;
				pbke->Invalid_pages_count--;
				pbke->Free_pages_count++;
			}
		}
		
		// change block status
		block->Hotness = BlockHotness::COLD;
	}

	void Flash_Block_Manager_Base::Change_block_status_to_warm(Block_Pool_Slot_Type* block, const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		PlaneBookKeepingType* pbke = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		// remove from existing write frontier (if this block was a write frontier)
		for (stream_id_type stream_id = 0; stream_id < total_concurrent_streams_no; stream_id++) {
			if (pbke->Data_hot_wf[stream_id] == block) {
				// assign new warm block
				pbke->Data_hot_wf[stream_id] = pbke->Get_a_free_block(stream_id, false, BlockHotness::HOT);
				break;
			}
		}
		
		// For WARM blocks, invalidate all CSB/MSB pages since only LSB pages are used
		for (flash_page_ID_type pageID = 0; pageID < pages_no_per_block; pageID++) {
			if (!is_lsb_page(pageID)) {
				// Mark CSB/MSB pages as invalid
				block->Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
				block->Invalid_page_count++;
				pbke->Invalid_pages_count++;
				pbke->Free_pages_count--;
			}
		}
		
		// change block status
		block->Hotness = BlockHotness::WARM;
	}

	void Flash_Block_Manager_Base::ResetAllPageHotnessCounters()
	{
		std::cout << "Resetting all page hotness counters in the entire SSD..." << std::endl;
		
		// Reset all page hotness counters in all blocks across all planes
		for (unsigned int channelID = 0; channelID < channel_count; channelID++) {
			for (unsigned int chipID = 0; chipID < chip_no_per_channel; chipID++) {
				for (unsigned int dieID = 0; dieID < die_no_per_chip; dieID++) {
					for (unsigned int planeID = 0; planeID < plane_no_per_die; planeID++) {
						PlaneBookKeepingType* pbke = &plane_manager[channelID][chipID][dieID][planeID];
						
						for (unsigned int blockID = 0; blockID < block_no_per_plane; blockID++) {
							Block_Pool_Slot_Type* block = &pbke->Blocks[blockID];
							
							// Reset all page hotness counters in this block
							for (flash_page_ID_type pageID = 0; pageID < this->pages_no_per_block; pageID++) {
								block->page_hotness_counters[pageID] = 0;
							}
						}
					}
				}
			}
		}
		
		std::cout << "All page hotness counters have been reset to 0." << std::endl;
	}


	Block_Pool_Slot_Type* PlaneBookKeepingType::Get_a_free_block(stream_id_type stream_id, bool for_mapping_data, SSD_Components::BlockHotness blockhotness)
	{
		// 3-2) select proper block pool
		std::multimap<unsigned int, Block_Pool_Slot_Type*>* primary_pool = nullptr;
		std::multimap<unsigned int, Block_Pool_Slot_Type*>* fallback_pool1 = nullptr;
		std::multimap<unsigned int, Block_Pool_Slot_Type*>* fallback_pool2 = nullptr;

		// Set primary and fallback pools based on hotness
		switch(blockhotness) {
			case BlockHotness::HOT:
				primary_pool = &Free_hot_block_pool;
				fallback_pool1 = &Free_warm_block_pool;
				fallback_pool2 = &Free_cold_block_pool;
				break;
			case BlockHotness::WARM:
				primary_pool = &Free_warm_block_pool;
				fallback_pool1 = &Free_hot_block_pool;
				fallback_pool2 = &Free_cold_block_pool;
				break;
			case BlockHotness::COLD:
				primary_pool = &Free_cold_block_pool;
				fallback_pool1 = &Free_warm_block_pool;
				fallback_pool2 = &Free_hot_block_pool;
				break;
		}

		// Try to get a block from primary pool
		if (!primary_pool->empty()) {
			auto it = primary_pool->begin();
			Block_Pool_Slot_Type* new_block = it->second;
			primary_pool->erase(it);
			
			new_block->Stream_id = stream_id;
			new_block->Holds_mapping_data = for_mapping_data;
			Block_usage_history.push(new_block->BlockID);
			
			return new_block;
		}

		// Try first fallback pool
		if (!fallback_pool1->empty()) {
			//PRINT_MESSAGE("Warning: Primary pool empty, using first fallback pool for hotness " + 
				//std::string(blockhotness == BlockHotness::HOT ? "HOT" : 
						//blockhotness == BlockHotness::WARM ? "WARM" : "COLD"));
			
			auto it = fallback_pool1->begin();
			Block_Pool_Slot_Type* new_block = it->second;
			fallback_pool1->erase(it);
			
			// Change block hotness to match requested hotness
			if (new_block->Hotness != blockhotness) {
				//std::cout << "Fallback pool: Changing block " << new_block->BlockID 
						//<< " from " << (int)new_block->Hotness << " to " << (int)blockhotness << std::endl;
				
				// Update hotness and invalid page settings
				if (blockhotness == BlockHotness::HOT || blockhotness == BlockHotness::WARM) {
					// Invalidate CSB/MSB pages for HOT/WARM blocks
					for (flash_page_ID_type pageID = 0; pageID < this->pages_no_per_block; pageID++) {
						if (!is_lsb_page(pageID)) {
							new_block->Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
							new_block->Invalid_page_count++;
						}
					}
				} else if (blockhotness == BlockHotness::COLD) {
					// Make all pages valid for COLD blocks
					for (flash_page_ID_type pageID = 0; pageID < this->pages_no_per_block; pageID++) {
						if (!is_lsb_page(pageID)) {
							new_block->Invalid_page_bitmap[pageID / 64] &= ~(((uint64_t)0x1) << (pageID % 64));
							new_block->Invalid_page_count--;
						}
					}
				}
				new_block->Hotness = blockhotness;
			}
			
			new_block->Stream_id = stream_id;
			new_block->Holds_mapping_data = for_mapping_data;
			Block_usage_history.push(new_block->BlockID);
			
			return new_block;
		}

		// Try second fallback pool
		if (!fallback_pool2->empty()) {
			//PRINT_MESSAGE("Warning: Primary and first fallback pools empty, using second fallback pool for hotness " + 
				//std::string(blockhotness == BlockHotness::HOT ? "HOT" : 
						//blockhotness == BlockHotness::WARM ? "WARM" : "COLD"));
			
			auto it = fallback_pool2->begin();
			Block_Pool_Slot_Type* new_block = it->second;
			fallback_pool2->erase(it);
			
			// Change block hotness to match requested hotness
			if (new_block->Hotness != blockhotness) {
				//std::cout << "Second fallback pool: Changing block " << new_block->BlockID 
						//<< " from " << (int)new_block->Hotness << " to " << (int)blockhotness << std::endl;
				
				// Update hotness and invalid page settings
				if (blockhotness == BlockHotness::HOT || blockhotness == BlockHotness::WARM) {
					// Invalidate CSB/MSB pages for HOT/WARM blocks
					for (flash_page_ID_type pageID = 0; pageID < this->pages_no_per_block; pageID++) {
						if (!is_lsb_page(pageID)) {
							new_block->Invalid_page_bitmap[pageID / 64] |= ((uint64_t)0x1) << (pageID % 64);
							new_block->Invalid_page_count++;
						}
					}
				} else if (blockhotness == BlockHotness::COLD) {
					// Make all pages valid for COLD blocks
					for (flash_page_ID_type pageID = 0; pageID < this->pages_no_per_block; pageID++) {
						if (!is_lsb_page(pageID)) {
							new_block->Invalid_page_bitmap[pageID / 64] &= ~(((uint64_t)0x1) << (pageID % 64));
							new_block->Invalid_page_count--;
						}
					}
				}
				new_block->Hotness = blockhotness;
			}
			
			new_block->Stream_id = stream_id;
			new_block->Holds_mapping_data = for_mapping_data;
			Block_usage_history.push(new_block->BlockID);
			
			return new_block;
		}

		// All pools are empty
		PRINT_ERROR("All free block pools are empty!");
		return nullptr;
	}
	
	void PlaneBookKeepingType::Check_bookkeeping_correctness(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		if (Total_pages_count !=  Free_pages_count + Valid_pages_count + Invalid_pages_count) {
					//std::cout << "Free pages: " << Free_pages_count << " Valid pages: " << Valid_pages_count <<" Invalid_pages: " << Invalid_pages_count << std::endl;
		//std::cout << "Total pages: " << Total_pages_count << std::endl;
			PRINT_ERROR("Inconsistent status in the plane bookkeeping record!")
		}
		if (Free_pages_count == 0) {
			//PRINT_ERROR("Plane " << "@" << plane_address.ChannelID << "@" << plane_address.ChipID << "@" << plane_address.DieID << "@" << plane_address.PlaneID << " pool size: " << Get_free_block_pool_size() << " ran out of free pages! Bad resource management! It is not safe to continue simulation!");
		}
	}

	unsigned int PlaneBookKeepingType::Get_free_block_pool_size()
	{
		return (unsigned int)(Free_cold_block_pool.size() + Free_warm_block_pool.size() + Free_hot_block_pool.size());
	}

	void PlaneBookKeepingType::Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl)
	{
		if (consider_dynamic_wl) {
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(block->Erase_count, block);
			Free_block_pool.insert(entry);
		} else {
			std::pair<unsigned int, Block_Pool_Slot_Type*> entry(0, block);
			Free_block_pool.insert(entry);
		}
	}

	void PlaneBookKeepingType::Add_to_free_block_pool(Block_Pool_Slot_Type* block, bool consider_dynamic_wl, BlockHotness hotness)
	{
    		// determine key
    		unsigned int key = consider_dynamic_wl ? block->Erase_count : 0;

    		// multimap for hotness
    		switch (hotness) {
        	case BlockHotness::HOT:
            		Free_hot_block_pool.emplace(key, block);
            		break;
        	case BlockHotness::WARM:
            		Free_warm_block_pool.emplace(key, block);
            		break;
        	case BlockHotness::COLD:
            		Free_cold_block_pool.emplace(key, block);
            		break;
    		}
	}

	unsigned int Flash_Block_Manager_Base::Get_min_max_erase_difference(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		unsigned int max_erased_block = 0;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++) {
			if (plane_record->Blocks[i].Erase_count > plane_record->Blocks[max_erased_block].Erase_count) {
				max_erased_block = i;
			}
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count) {
				min_erased_block = i;
			}
		}

		return max_erased_block - min_erased_block;
	}

	flash_block_ID_type Flash_Block_Manager_Base::Get_coldest_block_id(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		unsigned int min_erased_block = 0;
		PlaneBookKeepingType *plane_record = &plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID];

		for (unsigned int i = 1; i < block_no_per_plane; i++) {
			if (plane_record->Blocks[i].Erase_count < plane_record->Blocks[min_erased_block].Erase_count) {
				min_erased_block = i;
			}
		}
		
		return min_erased_block;
	}

	PlaneBookKeepingType* Flash_Block_Manager_Base::Get_plane_bookkeeping_entry(const NVM::FlashMemory::Physical_Page_Address& plane_address)
	{
		return &(plane_manager[plane_address.ChannelID][plane_address.ChipID][plane_address.DieID][plane_address.PlaneID]);
	}

	bool Flash_Block_Manager_Base::Block_has_ongoing_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl;
	}
	
	bool Flash_Block_Manager_Base::Can_execute_gc_wl(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return (plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count + plane_record->Blocks[block_address.BlockID].Ongoing_user_read_count == 0);
	}
	
	void Flash_Block_Manager_Base::GC_WL_started(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = true;
	}
	
	void Flash_Block_Manager_Base::program_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count++;
	}
	
	void Flash_Block_Manager_Base::Read_transaction_issued(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count++;
	}

	void Flash_Block_Manager_Base::Program_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_program_count--;
	}

	void Flash_Block_Manager_Base::Read_transaction_serviced(const NVM::FlashMemory::Physical_Page_Address& page_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[page_address.ChannelID][page_address.ChipID][page_address.DieID][page_address.PlaneID];
		plane_record->Blocks[page_address.BlockID].Ongoing_user_read_count--;
	}
	
	bool Flash_Block_Manager_Base::Is_having_ongoing_program(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		return plane_record->Blocks[block_address.BlockID].Ongoing_user_program_count > 0;
	}

	void Flash_Block_Manager_Base::GC_WL_finished(const NVM::FlashMemory::Physical_Page_Address& block_address)
	{
		PlaneBookKeepingType *plane_record = &plane_manager[block_address.ChannelID][block_address.ChipID][block_address.DieID][block_address.PlaneID];
		plane_record->Blocks[block_address.BlockID].Has_ongoing_gc_wl = false;
	}
	
	bool Flash_Block_Manager_Base::Is_page_valid(Block_Pool_Slot_Type* block, flash_page_ID_type page_id)
	{
		if ((block->Invalid_page_bitmap[page_id / 64] & (((uint64_t)1) << (page_id % 64))) == 0) {
			return true;
		}
		return false;
	}
}
