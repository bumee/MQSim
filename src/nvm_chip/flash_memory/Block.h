#ifndef BLOCK_H
#define BLOCK_H

#include "FlashTypes.h"
#include "Page.h"


namespace NVM
{
	namespace FlashMemory
	{
		class Block
		{
		public:
			Block(unsigned int PagesNoPerBlock, flash_block_ID_type BlockID);
			~Block();
			Page* Pages;						//Records the status of each sub-page
			flash_block_ID_type ID;            //Again this variable is required in list based garbage collections
			//BlockMetadata Metadata;
			bool isispp;
			bool isispe;
			sim_time_type ispp_latency;
			sim_time_type ispe_latency;
		};
	}
}
#endif // ! BLOCK_H
