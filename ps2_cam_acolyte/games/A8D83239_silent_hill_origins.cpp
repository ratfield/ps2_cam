#include "imgui.h"
#include "../ps2.h"
#include "../ps2_commands.h"
#include "shared_camera.h"
#include "shared_ui.h"
#include "glm/trigonometric.hpp"
#include "glm/vec3.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include <iostream>
#include <thread>
#include <chrono>

class silent_hill_origins : public ps2_game
{
private:
	sentinel_counter sentinel;
	toggle_state camera_flag;
	toggle_state brightness_flag; 
	tweakable_value_set<char, 1> pause_toggle; 
	tweakable_value_set<float, 16> camera_matrix_1;
	tweakable_value_set<float, 16> camera_matrix_2;
	read_only_value_set<char, 1> in_gameplay;
	read_only_value_set<float, 3> player_pos;
	read_only_value_set<float, 3> player_forward;
	float current_yaw = 0.0f;
	float current_pitch = 0.0f;

	bool modern_controls = false;
	toggle_state modern_controls_instructions;
	tweakable_value_set<char, 5> modern_controls_flags;
	float modern_controls_x_offset = 0.0f;
	float modern_controls_y_offset = 0.0f;
	float modern_controls_distance_offset = 0.0f;
	float time_until_attack_allowed = 0.0f;
	float time_until_stop_attack_allowed = 0.0f;
	float time_until_fully_out_of_cinematic = 0.0f;

	const int modern_controls_flags_block_offset = 0;
	const int modern_controls_flags_run_offset = 1;
	const int modern_controls_flags_attack_offset = 2;
	const int modern_controls_flags_stance_offset = 3;
	const int modern_controls_flags_on = 4;

public:
	explicit silent_hill_origins(const pcsx2& ps2)
		: sentinel(ps2, 0x00080000)
		, camera_flag(ps2)
		, brightness_flag(ps2) 
		, camera_matrix_1(ps2, 0x0091B5F0)
		, camera_matrix_2(ps2, 0x016E3BD0)
		, in_gameplay(ps2, 0x003386b8)
		, pause_toggle(ps2, 0x00339CEC)
		, player_pos(ps2, 0x3449c8)
		, player_forward(ps2, 0x3449d4)
		, modern_controls_instructions(ps2)
		, modern_controls_flags(ps2, { 0x00080004, 0x00080008, 0x0008000C, 0x00080010, 0x00080040 })
	{
		camera_flag.edit_off()
			.write<int32_t>(0x001133DC, 0x0C0899E8) 
			.finalize();
		camera_flag.edit_on()
			.write<int32_t>(0x001133DC, 0x00000000) 
			.finalize();

		brightness_flag.edit_off()
			.write<int32_t>(0x00929684, 0x41B00000)
			.write<int32_t>(0x00929688, 0x3F800000)
			.write<int32_t>(0x00929690, 0xC414A420)
			.finalize();
		brightness_flag.edit_on()
			.write<int32_t>(0x00929684, 0x43FA0000)
			.write<int32_t>(0x00929688, 0x43F78000)
			.write<int32_t>(0x00929690, 0x00000000)
			.finalize();

		modern_controls_instructions.edit_off()
			.write<int32_t>(0x0012FCF4, 0xE62018E4) 
			.write<int32_t>(0x0012FCF8, 0xE62118E8) 
			.finalize();

		modern_controls_instructions.edit_on()
			.write<int32_t>(0x0012FCF4, 0x00000000) 
			.write<int32_t>(0x0012FCF8, 0x00000000) 
			.write<int32_t>(0x00080080 + 4*0, 0x27BDFFE0) 
			.write<int32_t>(0x00080080 + 4*1, 0xFFBF0000) 
			.write<int32_t>(0x00080080 + 4*2, 0xFFA90008) 
			.write<int32_t>(0x00080080 + 4*3, 0xFFA50010) 
			.write<int32_t>(0x00080080 + 4*4, 0x0280202D) 
			.write<int32_t>(0x00080080 + 4*5, 0x0C067FCA) 
			.write<int32_t>(0x00080080 + 4*6, 0x00000000) 
			.write<int32_t>(0x00080080 + 4*7, 0xDFBF0000) 
			.write<int32_t>(0x00080080 + 4*8, 0xDFA90008) 
			.write<int32_t>(0x00080080 + 4*9, 0xDFA50010) 
			.write<int32_t>(0x00080080 + 4*10, 0x27BD0020) 
			.write<int32_t>(0x00080080 + 4*11, 0x3C080008) 
			.write<int32_t>(0x00080080 + 4*12, 0x910A0040) 
			.write<int32_t>(0x00080080 + 4*13, 0x11400004) 
			.write<int32_t>(0x00080080 + 4*14, 0x01094020) 
			.write<int32_t>(0x00080080 + 4*15, 0x81090000) 
			.write<int32_t>(0x00080080 + 4*16, 0x8CAA0004) 
			.write<int32_t>(0x00080080 + 4*17, 0xA1490004) 
			.write<int32_t>(0x00080080 + 4*18, 0x03E00008) 
			.write<int32_t>(0x00080080 + 4*19, 0x00000000) 
			.write<int32_t>(0x00130A0C, 0x34090004) 
			.write<int32_t>(0x00130A10, 0x0C020020) 
			.write<int32_t>(0x00130A1C, 0x34090008) 
			.write<int32_t>(0x00130A20, 0x0C020020) 
			.write<int32_t>(0x00130A54, 0x3409000C) 
			.write<int32_t>(0x00130A58, 0x0C020020) 
			.write<int32_t>(0x00130A64, 0x34090010) 
			.write<int32_t>(0x00130A68, 0x0C020020) 
			.finalize();

		modern_controls = ps2.get_prefs().read_bool("A8D83239_modern_controls", false);
		modern_controls_x_offset = ps2.get_prefs().read_float("A8D83239_modern_controls_x_offset", 0.0f);
		modern_controls_y_offset = ps2.get_prefs().read_float("A8D83239_modern_controls_y_offset", 1.5f);
		modern_controls_distance_offset = ps2.get_prefs().read_float("A8D83239_modern_controls_distance_offset", 3.0f);
	}

	void draw_game_ui(const pcsx2& ps2, const controller& c, playback& camera_playback) override
	{
		preferences& prefs = ps2.get_prefs();
		camera_playback.draw_playback_ui(c);
		if (!modern_controls) { shared_ui::toggle(c, controller_bindings::freecam, camera_flag, "Freecam"); }
		shared_ui::toggle(c, controller_bindings::lighting, brightness_flag, "Brightness Boost");
		shared_ui::toggle(c, controller_bindings::pause, pause_toggle, "Pause");
		ImGui::Separator();
		bool modern_was_on = modern_controls;
		ImGui::Checkbox("Modern Controls Mode", &modern_controls);
		if (modern_was_on != modern_controls) { prefs.write_bool("A8D83239_modern_controls", modern_controls); reset_all(); }
		if (modern_controls)
		{
			ps2.get_game_controller().draw_tool();
			if (ImGui::Button("Default")) {
				prefs.write_on_change_float("A8D83239_modern_controls_x_offset", &modern_controls_x_offset, 0.0f);
				prefs.write_on_change_float("A8D83239_modern_controls_y_offset", &modern_controls_y_offset, 1.5f);
				prefs.write_on_change_float("A8D83239_modern_controls_distnace_offset", &modern_controls_distance_offset, 3.0f);
			}
			shared_ui::slider(prefs, "Left/Right Offset", "A8D83239_modern_controls_x_offset", &modern_controls_x_offset, -10.0f, 10.0f);
			shared_ui::slider(prefs, "Height Offset", "A8D83239_modern_controls_y_offset", &modern_controls_y_offset, -2.0f, 2.0f);
			shared_ui::slider(prefs, "Distance", "A8D83239_modern_controls_distance_offset", &modern_controls_distance_offset, 0.0f, 10.0f);
		}
	}

	void reset_all()
	{
		camera_flag.reset(); camera_matrix_1.reset(); camera_matrix_2.reset();
		brightness_flag.reset(); pause_toggle.reset();
		modern_controls_instructions.reset(); modern_controls_flags.reset();
	}
	void update(const pcsx2& ps2, const controller_state& c, playback& camera_playback, float time_delta) override
	{
		if (sentinel.has_reset()) { reset_all(); }

		static bool f2_pressed = false;
		if (GetAsyncKeyState(VK_F2) & 0x8000) {
			if (!f2_pressed) { brightness_flag.toggle(); sentinel.increment(); f2_pressed = true; }
		} else { f2_pressed = false; }

		static bool f3_pressed = false;
		if (GetAsyncKeyState(VK_F3) & 0x8000) {
			if (!f3_pressed) { modern_controls = !modern_controls; ps2.get_prefs().write_bool("A8D83239_modern_controls", modern_controls); reset_all(); sentinel.increment(); f3_pressed = true; }
		} else { f3_pressed = false; }

		if (modern_controls)
		{
			in_gameplay.update();
			if (in_gameplay.get(0) == 0) { time_until_fully_out_of_cinematic = 0.5f; }
			else { time_until_fully_out_of_cinematic -= time_delta; }

			bool camera_should_be_on = time_until_fully_out_of_cinematic <= 0.0f;
			if (camera_should_be_on && !camera_flag.is_on())
			{
				camera_flag.set_on(true); camera_matrix_1.start_tweaking(); camera_matrix_2.start_tweaking();
				player_forward.update(); glm::vec3 forward = glm::vec3(player_forward.get(0), player_forward.get(1), player_forward.get(2));
				current_yaw = atan2(forward.z, forward.x); current_pitch = 0.0f; sentinel.increment();
			}
			else if (!camera_should_be_on && camera_flag.is_on())
			{
				camera_flag.set_on(false); camera_matrix_1.stop_tweaking(false); camera_matrix_2.stop_tweaking(false);
				sentinel.increment(); std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		}

		if (camera_flag.is_on() && time_until_fully_out_of_cinematic <= 0.0f)
		{
			float final_yaw = 0.0f; float final_pitch = 0.0f; float turn_scale = time_delta * 3.0f; glm::vec3 pos;
			if (modern_controls)
			{
				player_pos.update(); player_forward.update();
				int mouse_dx = 0; int mouse_dy = 0;
				POINT current_pos;
				if (GetCursorPos(&current_pos)) {
					mouse_dx = current_pos.x - (GetSystemMetrics(SM_CXSCREEN) / 2);
					mouse_dy = current_pos.y - (GetSystemMetrics(SM_CYSCREEN) / 2);
					SetCursorPos(GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 2);
				}
				current_yaw += (float)mouse_dx * turn_scale * 0.1f; current_pitch -= (float)mouse_dy * turn_scale * 0.1f;
				current_pitch = glm::clamp(current_pitch, glm::radians(-70.0f), glm::radians(30.0f));
				glm::vec3 cam_forward = glm::vec3(glm::cos(current_yaw) * glm::cos(current_pitch), glm::sin(current_pitch), glm::sin(current_yaw) * glm::cos(current_pitch));
				glm::vec3 cam_right = glm::cross(cam_forward, glm::vec3(0.0f, 1.0f, 0.0f));
				pos = glm::vec3(player_pos.get(0), player_pos.get(1) + modern_controls_y_offset, player_pos.get(2)) - cam_forward * modern_controls_distance_offset + cam_right * modern_controls_x_offset;
				final_yaw = glm::radians(90.0f) - atan2(cam_forward.z, cam_forward.x); final_pitch = -asin(cam_forward.y);
			}
			else
			{
				final_yaw = current_yaw; final_pitch = current_pitch;
				pos = glm::vec3(camera_matrix_1.get(12), camera_matrix_1.get(13), camera_matrix_1.get(14));
			}
			camera_playback.update(time_delta, final_yaw, final_pitch, pos.x, pos.y, pos.z);
			glm::mat4 final_mat = shared_camera::compute_rotation_matrix_y_x(final_yaw, final_pitch);
			final_mat = glm::vec4(pos.x, pos.y, pos.z, 1.0f);
			camera_matrix_1.set(0, final_mat); camera_matrix_2.set(0, final_mat);
			camera_matrix_1.flush(ps2); camera_matrix_2.flush(ps2);
		}
	}
};

ps2_game_static_register<silent_hill_origins> r("A8D83239", "Silent Hill Origins (USA)");
