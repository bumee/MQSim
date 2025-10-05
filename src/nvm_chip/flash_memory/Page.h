#ifndef PAGE_H
#define PAGE_H

#include "FlashTypes.h"

namespace NVM
{
	namespace FlashMemory
	{
			struct PageOOB
			{
				sim_time_type LastAccessTime;
				PageOOB(): LastAccessTime(0) {}
				void Record(sim_time_type t)
				{
					LastAccessTime = t;
				}
				void Clear()
				{
					LastAccessTime = 0;
				}
			};

		struct PageMetadata
		{
			//page_status_type Status;
			LPA_type LPA;
		};

		class Page {
		public:
			Page()
			{
				//Metadata.Status = FREE_PAGE;
				Metadata.LPA = NO_LPA;
				//Metadata.SourceStreamID = NO_STREAM;
			};
			
			PageMetadata Metadata;
			PageOOB OOB;

			void Write_metadata(const PageMetadata& metadata)
			{
				this->Metadata.LPA = metadata.LPA;
			}
			
			void Read_metadata(PageMetadata& metadata)
			{
				metadata.LPA = this->Metadata.LPA;
			}

			void Record_access(sim_time_type time)
			{
				OOB.Record(time);
			}
		};
	}
}

#endif // !PAGE_H
