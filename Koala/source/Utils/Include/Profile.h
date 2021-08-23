#pragma once

#include <chrono>
#include <fstream>
#include <string>
#include <string_view>

#define PROFILE_ENABLE

class CPUProfiler {
public:
	CPUProfiler(std::string_view name);
	~CPUProfiler();

	static void start_session(std::string_view filename);
	static void end_session();
	static void write_footer();

private:
	std::string m_scope_name;
	long long m_start_point;

	static std::ofstream s_output_file;
	static size_t s_count;
};

#if defined(PROFILE_ENABLE)
#define MY_PROFILE_START(name) CPUProfiler::start_session(name);
#else
#define MY_ROFILE_START(name)
#endif

#if defined(PROFILE_ENABLE)
#define MY_PROFILE_END() CPUProfiler::end_session();
#else
#define MY_PROFILE_END()
#endif

#if defined(PROFILE_ENABLE)
#define MY_PROFILE_FUNCTION() CPUProfiler cpu_profiler##__LINE__(__FUNCSIG__);
#else
#define MY_PROFILE_FUNCTION()
#endif