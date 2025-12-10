#ifndef HOT_LPA_LIST_H
#define HOT_LPA_LIST_H

#include "../ssd/SSD_Defs.h"
#include <string>
#include <unordered_set>

namespace SSD_Components
{
	class HotLpaList
	{
	public:
		// Hot LPA 리스트 파일을 로드
		// 파일 포맷: 한 줄에 하나의 LPA (주석 라인은 #으로 시작)
		// Returns: 성공 여부
		static bool LoadFromFile(const std::string& file_path);
		
		// 해당 LPA가 Hot인지 확인
		static bool IsHot(LPA_type lpa);
		
		// Hot LPA 개수 반환
		static size_t GetHotLpaCount();
		
		// 리스트 초기화 (시뮬레이션 재시작 시 사용)
		static void Clear();
		
	private:
		static std::unordered_set<LPA_type> hot_lpa_set;
		static bool is_loaded;
	};
}

#endif // !HOT_LPA_LIST_H

