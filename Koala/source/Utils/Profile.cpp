#include "Include/Profile.h"

std::ofstream CPUProfiler::s_output_file;
size_t CPUProfiler::s_count;

CPUProfiler::CPUProfiler(std::string_view name)
	: m_scope_name(name), m_start_point(std::chrono::steady_clock::now().time_since_epoch().count() / 1000) {}

CPUProfiler::~CPUProfiler() {
	long long end_point = std::chrono::steady_clock::now().time_since_epoch().count() / 1000;
	long long duration = end_point - m_start_point;

	if (s_count > 0)
		s_output_file << ",";

	s_output_file << "{";
	s_output_file << "\"cat\":\"function\",";
	s_output_file << "\"dur\":" << duration << ",";
	s_output_file << "\"name\":\"" << m_scope_name << "\",";
	s_output_file << "\"ts\":" << m_start_point << ",";
	s_output_file << "\"ph\":\"X\",";
	s_output_file << "\"pid\":0,";
	s_output_file << "\"tid\":0";
	s_output_file << "}";

	s_count++;
}

void CPUProfiler::start_session(std::string_view filename) {
	if (s_output_file.is_open())
		write_footer();

	s_output_file.open(filename.data());
	s_output_file << "{\"otherData\":{},\"displayTimeUnit\":\"ns\",\"traceEvents\":[";
}

void CPUProfiler::end_session() {
	write_footer();
	s_output_file.close();
}

void CPUProfiler::write_footer() {
	s_output_file << "]}";
}