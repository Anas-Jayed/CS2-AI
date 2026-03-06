#include "CS2/GameInformationHandler.h"
#include <chrono>
#include <cmath> 
#include "Utility/Logging.h"
#include "Utility/Utility.h"
#include "CS2/Constants.h" 

// --- Helper for FOV Calculation ---
static Vec2D<float> calculate_angle_to_target(const Vec3D<float>& player_head, const Vec3D<float>& enemy_head)
{
	const Vec3D<float> vec_to_enemy = enemy_head - player_head;
	const auto z_vec = Vec3D<float>(0, 0, 1);

	float dot = z_vec.dot_product(vec_to_enemy);
	float mags = z_vec.calc_abs() * vec_to_enemy.calc_abs();

	if (mags == 0) return { 0.0f, 0.0f };

	float cos_val = dot / mags;
	if (cos_val > 1.0f) cos_val = 1.0f;
	if (cos_val < -1.0f) cos_val = -1.0f;

	float vertical_angle = std::acos(cos_val) / static_cast<float>(PI) * 180.0f;
	vertical_angle -= 90.0f;

	Vec2D<float> result = {};
	result.x = vertical_angle;
	result.y = std::atan2(vec_to_enemy.y, vec_to_enemy.x) / static_cast<float>(PI) * 180.0f;

	return result;
}

// --- CORE FUNCTIONS (MUST BE PRESENT) ---

bool GameInformationhandler::init(const Config& config)
{
	m_config = config;
	m_process_memory.attach_to_process(config.windowname.c_str());
	m_client_dll_address = m_process_memory.get_module_address(config.client_dll_name.c_str());
	m_attached_to_process = m_client_dll_address != 0;
	return m_attached_to_process;
}

bool GameInformationhandler::loadOffsets()
{
	auto offsets = load_offsets_from_files();
	if (offsets)
		m_offsets = offsets.value();
	return offsets.has_value();
}

void GameInformationhandler::update_game_information()
{
	auto player_controller_address = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.local_player_controller_offset);
	auto player_pawn_address = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.local_player_pawn);

	m_game_information.controlled_player = read_controlled_player_information(player_controller_address);
	m_game_information.player_in_crosshair = read_player_in_crosshair(player_controller_address, player_pawn_address);
	m_game_information.other_players = read_other_players(player_controller_address);
	m_game_information.closest_target_player = get_closest_player(m_game_information, m_config.ignore_same_team);
	m_game_information.current_map = read_in_current_map();
}

GameInformation GameInformationhandler::get_game_information() const
{
	return m_game_information;
}

bool GameInformationhandler::is_attached_to_process() const
{
	return m_attached_to_process;
}

void GameInformationhandler::set_view_vec(const Vec2D<float>& view_vec)
{
	if (std::isnan(view_vec.x) || std::isnan(view_vec.y))
		return;
	m_process_memory.write_memory<Vec2D<float>>(m_client_dll_address + m_offsets.client_state_view_angle, view_vec);
}

void GameInformationhandler::set_player_movement(const Movement& movement)
{
	HWND hwnd = FindWindowA(nullptr, m_config.windowname.c_str());
	if (!hwnd) return;

	auto handle_key = [&](bool value, DWORD key_code) {
		if (value) PostMessage(hwnd, WM_KEYDOWN, key_code, 0);
		else PostMessage(hwnd, WM_KEYUP, key_code, 0);
		};

	handle_key(movement.forward, 0x57);  // W
	handle_key(movement.backward, 0x53); // S
	handle_key(movement.left, 0x41);     // A
	handle_key(movement.right, 0x44);    // D
}

void GameInformationhandler::set_player_shooting(bool val)
{
	constexpr DWORD not_shooting_value = 16777472;
	DWORD mem_val = val ? button_pressed_value : not_shooting_value;
	m_process_memory.write_memory<DWORD>(m_client_dll_address + m_offsets.force_attack, mem_val);
}

void GameInformationhandler::set_config(Config config)
{
	m_config = std::move(config);
}

// --- TARGETING LOGIC ---

std::optional<PlayerInformation> GameInformationhandler::get_closest_player(const GameInformation& game_info, bool only_enemy_team)
{
	std::optional<PlayerInformation> closest_enemy = {};
	const auto& controlled_player = game_info.controlled_player;
	float lowest_fov = FLT_MAX;

	for (const auto& enemy : game_info.other_players)
	{
		if (only_enemy_team && (enemy.team == controlled_player.team))
			continue;

		if (enemy.health <= 0)
			continue;

		Vec2D<float> ideal_angle = calculate_angle_to_target(controlled_player.head_position, enemy.head_position);

		float d_pitch = ideal_angle.x - controlled_player.view_vec.x;
		float d_yaw = ideal_angle.y - controlled_player.view_vec.y;

		if (d_yaw > 180.0f) d_yaw -= 360.0f;
		if (d_yaw < -180.0f) d_yaw += 360.0f;

		float current_fov = std::sqrt(d_pitch * d_pitch + d_yaw * d_yaw);

		if (current_fov < lowest_fov)
		{
			lowest_fov = current_fov;
			closest_enemy = enemy;
		}
	}
	return closest_enemy;
}

// --- MEMORY READERS ---

std::string GameInformationhandler::read_in_current_map()
{
	constexpr uintptr_t global_var_map = 0x180;
	const auto global_vars = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.global_vars);
	const auto map_path_ptr = m_process_memory.read_memory<uintptr_t>(global_vars + global_var_map);

	char name_buffer[64] = "";
	m_process_memory.read_string_from_memory(map_path_ptr, name_buffer, std::size(name_buffer));
	return name_buffer[0] ? get_filename(name_buffer) : "";
}

bool GameInformationhandler::read_in_if_controlled_player_is_shooting()
{
	DWORD val = m_process_memory.read_memory<DWORD>(m_client_dll_address + m_offsets.force_attack);
	return val == button_pressed_value;
}

ControlledPlayer GameInformationhandler::read_controlled_player_information(uintptr_t player_address)
{
	ControlledPlayer dest{};
	dest.view_vec = m_process_memory.read_memory<Vec2D<float>>(m_client_dll_address + m_offsets.client_state_view_angle);
	dest.team = m_process_memory.read_memory<int>(player_address + m_offsets.team_offset);

	auto local_player_pawn = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.local_player_pawn);
	dest.health = m_process_memory.read_memory<int>(local_player_pawn + m_offsets.player_health_offset);
	dest.position = m_process_memory.read_memory<Vec3D<float>>(local_player_pawn + m_offsets.position);
	dest.shots_fired = m_process_memory.read_memory<DWORD>(local_player_pawn + m_offsets.shots_fired_offset);
	dest.shooting = read_in_if_controlled_player_is_shooting();
	dest.movement = read_controlled_player_movement(player_address);
	dest.head_position = get_head_bone_position(local_player_pawn);

	return dest;
}

std::vector<PlayerInformation> GameInformationhandler::read_other_players(uintptr_t player_address)
{
	constexpr size_t max_players = 64;
	std::vector<PlayerInformation> other_players;
	uintptr_t entity_list_start_address = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.entity_list_start_offset);
	if (!entity_list_start_address) return other_players;

	for (int i = 0; i < max_players; i++)
	{
		uintptr_t listEntity = get_list_entity(i, entity_list_start_address);
		if (!listEntity) continue;

		auto current_controller = get_entity_controller_or_pawn(listEntity, i);
		if (!current_controller || current_controller == player_address) continue;

		auto controller_pawn_handle = m_process_memory.read_memory<uintptr_t>(current_controller + m_offsets.player_pawn_handle);
		if (!controller_pawn_handle) continue;

		auto player = read_player(entity_list_start_address, controller_pawn_handle, player_address);
		if (player) other_players.emplace_back(*player);
	}
	return other_players;
}

Movement GameInformationhandler::read_controlled_player_movement(uintptr_t player_address)
{
	Movement return_value = {};
	return_value.forward = m_process_memory.read_memory<DWORD>(m_client_dll_address + m_offsets.force_forward) == button_pressed_value;
	return_value.backward = m_process_memory.read_memory<DWORD>(m_client_dll_address + m_offsets.force_backward) == button_pressed_value;
	return_value.left = m_process_memory.read_memory<DWORD>(m_client_dll_address + m_offsets.force_left) == button_pressed_value;
	return_value.right = m_process_memory.read_memory<DWORD>(m_client_dll_address + m_offsets.force_right) == button_pressed_value;
	return return_value;
}

Vec3D<float> GameInformationhandler::get_head_bone_position(uintptr_t player_pawn)
{
	constexpr DWORD bone_matrix_offset = 0x80;
	constexpr DWORD head_bone_index = 0x6;
	constexpr DWORD matrix_size = 0x20;

	auto game_scene_node = m_process_memory.read_memory<uintptr_t>(player_pawn + m_offsets.sceneNode);
	auto bone_matrix = m_process_memory.read_memory<uintptr_t>(game_scene_node + m_offsets.model_state + bone_matrix_offset);
	return m_process_memory.read_memory<Vec3D<float>>(bone_matrix + (head_bone_index * matrix_size));
}

uintptr_t GameInformationhandler::get_list_entity(uintptr_t id, uintptr_t entity_list)
{
	return m_process_memory.read_memory<uintptr_t>(entity_list + ((8 * ((id & 0x7FFF) >> 9)) + 0x10));
}

uintptr_t GameInformationhandler::get_entity_controller_or_pawn(uintptr_t list_entity, uintptr_t id)
{
	return m_process_memory.read_memory<uintptr_t>(list_entity + (id & 0x1FF) * 0x70);
}

std::optional<PlayerInformation> GameInformationhandler::read_player(uintptr_t entity_list_begin, uintptr_t id, uintptr_t player_address)
{
	uintptr_t listEntity = get_list_entity(id, entity_list_begin);
	if (!listEntity) return {};

	auto current_controller = get_entity_controller_or_pawn(listEntity, id);
	if (!current_controller || current_controller == player_address) return {};

	PlayerInformation ent;
	ent.position = m_process_memory.read_memory<Vec3D<float>>(current_controller + m_offsets.position);
	ent.health = m_process_memory.read_memory<DWORD>(current_controller + m_offsets.player_health_offset);
	ent.team = m_process_memory.read_memory<int>(current_controller + m_offsets.team_offset);
	ent.head_position = get_head_bone_position(current_controller);
	return ent;
}

std::optional<PlayerInformation> GameInformationhandler::read_player_in_crosshair(uintptr_t player_controller, uintptr_t player_pawn)
{
	const auto cross_hair_ID = m_process_memory.read_memory<int>(player_pawn + m_offsets.crosshair_offset);
	if (cross_hair_ID <= 0) return {};

	uintptr_t entity_list_start_address = m_process_memory.read_memory<uintptr_t>(m_client_dll_address + m_offsets.entity_list_start_offset);
	return read_player(entity_list_start_address, cross_hair_ID, player_controller);
}

ViewMatrix GameInformationhandler::get_view_matrix()
{
	uintptr_t view_matrix_offset = 36753312;
	return m_process_memory.read_memory<ViewMatrix>(m_client_dll_address + view_matrix_offset);
}