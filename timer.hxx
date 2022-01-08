#pragma once
#include <GLFW/glfw3.h>

class FrameTimer {
public:
	static constexpr float fps_update_interval = 1.0f;

	void start() {
		t_begin = glfwGetTime();
	}

	void begin_frame() {
		double t = glfwGetTime();
		delta = t - t_begin;
		t_begin = t;
		update_fps();
	}

	void end_frame() {
	}

	double t() const noexcept { return t_begin; }
	float dt() const noexcept { return delta; }
	float fps() const noexcept { return calculated_fps; }

private:
	void update_fps() {
		fps_time += delta;
		fps_frames++;
		if (fps_time < fps_update_interval)
			return;
		calculated_fps = fps_frames / fps_time;
		fps_time = 0.0f;
		fps_frames = 0;
	}

	double t_begin;
	float delta = 0.0f;
	long long frame = 0;

	float calculated_fps = 1.0f;
	int fps_frames = 0;
	float fps_time = 0.0f;
};
