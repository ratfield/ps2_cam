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
#include <chrono>
#include <thread>
#include <Windows.h> // Критично для GetAsyncKeyState и VK-кодов клавиш

class silent_hill_origins : public ps2_game
{
private:
	sentinel_counter sentinel;
	toggle_state camera_flag;
	tweakable_value_set<float, 16> camera_matrix_1;
	tweakable_value_set<float, 16> camera_matrix_2;
	read_only_value_set<char, 1> in_gameplay;
	read_only_value_set<float, 3> player_pos;
	read_only_value_set<float, 3> player_forward;
	
	float current_yaw = 0.0f;
	float current_pitch = 0.0f;
	bool modern_controls = false;

	float modern_controls_x_offset = 0.0f;
	float modern_controls_y_offset = 1.5f;
	float modern_controls_distance_offset = 3.0f;
	float time_until_fully_out_of_cinematic = 0.0f;

public:
	explicit silent_hill_origins(const pcsx2& ps2)
		: sentinel(ps2, 0x00080000)
		, camera_flag(ps2)
		, camera_matrix_1(ps2, 0x0091B5F0)
		, camera_matrix_2(ps2, 0x016E3BD0)
		, in_gameplay(ps2, 0x003386b8)
		, player_pos(ps2, 0x3449c8)
		, player_forward(ps2, 0x3449d4)
	{
		camera_flag.edit_off().write<int32_t>(0x001133DC, 0x0C0899E8).finalize();
		camera_flag.edit_on().write<int32_t>(0x001133DC, 0x00000000).finalize();

		modern_controls = ps2.get_prefs().read_bool("A8D83239_modern_controls", false);
	}

	void draw_game_ui(const pcsx2& ps2, const controller& c, playback& camera_playback) override
	{
		ImGui::Text("Silent Hill: Origins Native KB+M Mod");
		ImGui::Separator();
		ImGui::Checkbox("Modern Controls Mode (F3 to toggle)", &modern_controls);
	}

	void reset_all()
	{
		camera_flag.reset();
		camera_matrix_1.reset();
		camera_matrix_2.reset();
	}

	void update(const pcsx2& ps2, const controller_state& c, playback& camera_playback, float time_delta) override
	{
		if (sentinel.has_reset())
		{
			reset_all();
		}

		// Хоткей F3: Включение/выключение мода на лету
		static bool f3_pressed = false;
		if (GetAsyncKeyState(VK_F3) & 0x8000)
		{
			if (!f3_pressed)
			{
				modern_controls = !modern_controls;
				ps2.get_prefs().write_bool("A8D83239_modern_controls", modern_controls);
				reset_all();
				sentinel.increment();
				f3_pressed = true;
			}
		}
		else
		{
			f3_pressed = false;
		}

		if (modern_controls)
		{
			in_gameplay.update();
			if (in_gameplay.get(0) == 0) time_until_fully_out_of_cinematic = 0.5f;
			else time_until_fully_out_of_cinematic -= time_delta;

			bool camera_should_be_on = time_until_fully_out_of_cinematic <= 0.0f;
			if (camera_should_be_on && !camera_flag.is_on())
			{
				camera_flag.set_on(true);
				camera_matrix_1.start_tweaking();
				camera_matrix_2.start_tweaking();
				player_forward.update();
				glm::vec3 forward = glm::vec3(player_forward.get(0), player_forward.get(1), player_forward.get(2));
				current_yaw = atan2(forward.z, forward.x);
				current_pitch = 0.0f;
				sentinel.increment();
			}
			else if (!camera_should_be_on && camera_flag.is_on())
			{
				camera_flag.set_on(false);
				camera_matrix_1.stop_tweaking(false);
				camera_matrix_2.stop_tweaking(false);
				sentinel.increment();
			}

			// =========================================================================
			// НАТИВНЫЙ ИНЪЕКТОР КЛАВИАТУРЫ И МЫШИ ЧЕРЕЗ ЭМУЛЯТОР PCSX2 (ПРОТОКОЛ PINE)
			// =========================================================================
			auto& virtual_pad = const_cast<pcsx2&>(ps2).get_game_controller();

			// 1. Ходьба (W, A, S, D -> Левый стик)
			float move_x = 0.0f;
			float move_y = 0.0f;
			if (GetAsyncKeyState('W') & 0x8000) move_y += 1.0f;
			if (GetAsyncKeyState('S') & 0x8000) move_y -= 1.0f;
			if (GetAsyncKeyState('D') & 0x8000) move_x += 1.0f;
			if (GetAsyncKeyState('A') & 0x8000) move_x -= 1.0f;

			// Нормализуем вектор, чтобы по диагонали Трэвис не бегал быстрее
			if (move_x != 0.0f || move_y != 0.0f) {
				glm::vec2 norm_move = glm::normalize(glm::vec2(move_x, move_y)) * 127.0f;
				virtual_pad.set_axis(0, (int)norm_move.x);       // LX (Влево/Вправо)
				virtual_pad.set_axis(1, (int)(-norm_move.y));    // LY (Вверх/Вниз, инверсия для PS2)
			} else {
				virtual_pad.set_axis(0, 0);
				virtual_pad.set_axis(1, 0);
			}

			// 2. Действия и Кнопки
			bool wants_aim  = (GetAsyncKeyState(VK_RBUTTON) & 0x8000); // ПКМ - Прицеливание (R1)
			bool wants_hit  = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) || (GetAsyncKeyState('E') & 0x8000); // ЛКМ или E - Атака / Обыск (Cross)
			bool wants_run  = (GetAsyncKeyState(VK_MENU) & 0x8000);    // Левый ALT - Бег (Square)
			bool wants_map  = (GetAsyncKeyState(VK_TAB) & 0x8000);     // TAB - Карта (Triangle)
			bool wants_inv  = (GetAsyncKeyState('C') & 0x8000);        // C - Инвентарь (Start)
			bool wants_lamp = (GetAsyncKeyState('F') & 0x8000);        // F - Фонарь (Select)

			// Отправляем кнопки в эмулятор (картируем под пресет Origins)
			virtual_pad.set_button(controller_bindings::r1, wants_aim);    // Боевая стойка
			virtual_pad.set_button(controller_bindings::cross, wants_hit);  // Удар / Действие
			virtual_pad.set_button(controller_bindings::square, wants_run); // Бег
			virtual_pad.set_button(controller_bindings::triangle, wants_map); // Карта
			virtual_pad.set_button(controller_bindings::start, wants_inv);  // Инвентарь
			virtual_pad.set_button(controller_bindings::back, wants_lamp);  // Фонарь
		}

		// Логика обработки и вывода координат камеры (Мышь -> Правый стик)
		if (camera_flag.is_on())
		{
			float final_yaw = 0.0f;
			float final_pitch = 0.0f;
			float turn_scale = time_delta * 3.0f;

			glm::vec3 pos;

			if (modern_controls)
			{
				player_pos.update();
				player_forward.update();

				// Считываем смещение мыши напрямую через SDL утилиты
				int mouse_dx = 0, mouse_dy = 0;
				SDL_GetRelativeMouseState(&mouse_dx, &mouse_dy);

				// Применяем движение мыши к углам обзора (turn_scale регулирует чувствительность)
				current_yaw += (float)mouse_dx * turn_scale * 0.1f;
				current_pitch += (float)mouse_dy * turn_scale * 0.1f; // НАШ САМОЛЕТНЫЙ ПЛЮС НА МЕСТЕ

				current_pitch = glm::clamp(current_pitch, glm::radians(-70.0f), glm::radians(30.0f));

				glm::vec3 cam_forward = glm::vec3(
					glm::cos(current_yaw) * glm::cos(current_pitch),
					glm::sin(current_pitch),
					glm::sin(current_yaw) * glm::cos(current_pitch)
				);

				glm::vec3 cam_right = glm::cross(cam_forward, glm::vec3(0.0f, 1.0f, 0.0f));
				pos = glm::vec3(player_pos.get(0), player_pos.get(1) + modern_controls_y_offset, player_pos.get(2)) - cam_forward * modern_controls_distance_offset + cam_right * modern_controls_x_offset;

				final_yaw = glm::radians(90.0f) - atan2(cam_forward.z, cam_forward.x);
				final_pitch = -asin(cam_forward.y);
			}
			else
			{
				final_yaw = current_yaw;
				final_pitch = current_pitch;
				pos = glm::vec3(camera_matrix_1.get(12), camera_matrix_1.get(13), camera_matrix_1.get(14));
			}

			camera_playback.update(time_delta, final_yaw, final_pitch, pos.x, pos.y, pos.z);

			glm::mat4 position_mat(1.0f);
			position_mat[3] = glm::vec4(pos.x, pos.y, pos.z, 1.0f);

			glm::mat4 final_mat = position_mat * shared_camera::compute_rotation_matrix_y_x(final_yaw, final_pitch);

			camera_matrix_1.set(0, final_mat);
			camera_matrix_2.set(0, final_mat);

			camera_matrix_1.flush(ps2);
			camera_matrix_2.flush(ps2);
		}
	}
};

ps2_game_static_register<silent_hill_origins> r("A8D83239", "Silent Hill Origins (USA)");
