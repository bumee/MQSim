#include "HotLpaList.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace SSD_Components
{
	std::unordered_set<LPA_type> HotLpaList::hot_lpa_set;
	bool HotLpaList::is_loaded = false;

	bool HotLpaList::LoadFromFile(const std::string& file_path)
	{
		std::ifstream file(file_path);
		if (!file.is_open())
		{
			std::cerr << "Warning: Hot LPA 리스트 파일을 열 수 없습니다: " << file_path << std::endl;
			return false;
		}

		hot_lpa_set.clear();
		std::string line;
		size_t line_count = 0;
		size_t loaded_count = 0;

		while (std::getline(file, line))
		{
			line_count++;
			
			// 주석 라인 건너뛰기
			if (line.empty() || line[0] == '#')
			{
				continue;
			}

			// 공백 제거
			std::istringstream iss(line);
			std::string token;
			iss >> token;
			
			if (token.empty())
			{
				continue;
			}

			try
			{
				LPA_type lpa = std::stoull(token);
				hot_lpa_set.insert(lpa);
				loaded_count++;
			}
			catch (const std::exception& e)
			{
				// 파싱 에러는 무시하고 계속 진행
				continue;
			}
		}

		file.close();
		is_loaded = true;

		std::cout << "Hot LPA 리스트 로드 완료: " << file_path << std::endl;
		std::cout << "  총 라인 수: " << line_count << std::endl;
		std::cout << "  로드된 Hot LPA 개수: " << loaded_count << std::endl;

		return true;
	}

	bool HotLpaList::IsHot(LPA_type lpa)
	{
		if (!is_loaded)
		{
			return false; // 리스트가 로드되지 않았으면 Hot이 아님
		}
		return hot_lpa_set.find(lpa) != hot_lpa_set.end();
	}

	size_t HotLpaList::GetHotLpaCount()
	{
		return hot_lpa_set.size();
	}

	void HotLpaList::Clear()
	{
		hot_lpa_set.clear();
		is_loaded = false;
	}
}

