#include "Block.h"

namespace NVM
{
	namespace FlashMemory
	{
		Block::Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID)
		{
			ID = BlockID;
			Pages = new Page[PagesNoPerBlock];
			ispp_latency = 1000000; //1ms
			ispe_latency = 1000000; //1ms
			isispp = true;
			isispe = true;
		}

		Block::~Block()
		{
		delete[] Pages;
		}
	}
}
