#pragma once
#include <chrono>
#include <ostream>

namespace spi::prof {
	using Clock = std::chrono::steady_clock;
	using Timepoint = typename Clock::time_point;
	using Duration = typename Clock::duration;
	using Hours = std::chrono::hours;
	using Minutes = std::chrono::minutes;
	using Seconds = std::chrono::seconds;
	using Milliseconds = std::chrono::milliseconds;
	using Microseconds = std::chrono::microseconds;
	using Nanoseconds = std::chrono::nanoseconds;

	inline std::ostream& operator << (std::ostream& os, const Duration& d) {
		return os << "(duration)" << std::chrono::duration_cast<Microseconds>(d).count() << " microsec";
	}
	inline std::ostream& operator << (std::ostream& os, const Timepoint& t) {
		return os << "(timepoint)" << std::chrono::duration_cast<Milliseconds>(t.time_since_epoch()).count() << " ms";
	}
}
