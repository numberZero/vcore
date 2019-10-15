#pragma once
#include <time.h>
#include <errno.h>
#include <system_error>

inline static timespec operator+ (timespec a, timespec b) {
	timespec c = {
		a.tv_sec + b.tv_sec,
		a.tv_nsec + b.tv_nsec
	};
	if (c.tv_nsec > 1'000'000'000) {
		c.tv_nsec -= 1'000'000'000;
		c.tv_sec++;
	}
	assert(0 <= c.tv_nsec && c.tv_nsec < 1'000'000'000);
	return c;
}

inline static timespec operator- (timespec a, timespec b) {
	timespec c = {
		a.tv_sec - b.tv_sec,
		a.tv_nsec - b.tv_nsec
	};
	if (c.tv_nsec < 0) {
		c.tv_nsec += 1'000'000'000;
		c.tv_sec--;
	}
	assert(0 <= c.tv_nsec && c.tv_nsec < 1'000'000'000);
	return c;
}

inline static double to_double(timespec x) {
	return x.tv_sec + 1e-9 * x.tv_nsec;
}

inline static timespec thread_cpu_clock() {
	timespec x;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &x) == 0)
		return x;
	if (errno == EINVAL)
		throw std::runtime_error("Per-thread CPU time clock is not supported");
	throw std::system_error(errno, std::system_category(), "clock_gettime");
}
