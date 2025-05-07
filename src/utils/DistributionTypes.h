#ifndef DISTRIBUTION_TYPES
#define DISTRIBUTION_TYPES

namespace Utils
{
	enum class Address_Distribution_Type { RANDOM_HOTCOLD, STREAMING, RANDOM_UNIFORM, MIXED_STREAMING_RANDOM };
	enum class Request_Size_Distribution_Type { FIXED, NORMAL };
	enum class Workload_Type { TRACE_BASED, SYNTHETIC };
	enum class Request_Generator_Type { BANDWIDTH, QUEUE_DEPTH };//Time_INTERVAL: general requests based on the arrival rate definitions, DEMAND_BASED: just generate a request, every time that there is a demand
}

#endif // !DISTRIBUTION_TYPES
