#pragma once

#include <Windows.h>
#include <string>
#include <optional>
#include <vector>
#include "Utility/Vec2D.h"
#include "Utility/Vec3D.h"
#include "CS2/Config.h"
#include "CS2/MemoryManager.h"
#include "CS2/Offsets.h"

// --- 1. ADD THIS STRUCT AT THE TOP ---
struct ViewMatrix {
	float matrix[16];
};
// -------------------------------------

struct Movement
{
	bool forward = false;
	bool backward = false;
	bool left = false;
	bool right = false;
};

struct ControlledPlayer
{
	Vec2D<float> view_vec;
	int health = 0;
	Vec3D<float> position;
	int team = 0;
	DWORD shots_fired = 0;
	bool shooting = false;
	Movement movement;
	Vec3D<float> head_position;
};

struct PlayerInformation
{
	Vec3D<float> position;
	int health = 0;
	int team = 0;
	Vec3D<float> head_position;
};

struct GameInformation
{
	ControlledPlayer controlled_player;
	std::optional<PlayerInformation> player_in_crosshair;
	std::vector<PlayerInformation> other_players;
	std::optional<PlayerInformation> closest_target_player;
	std::string current_map;
};

class GameInformationhandler
{
public:
	bool init(const Config& config);
	bool loadOffsets();
	void update_game_information();
	GameInformation get_game_information() const;
	bool is_attached_to_process() const;
	void set_view_vec(const Vec2D<float>& view_vec);
	void set_player_movement(const Movement& movement);
	void set_player_shooting(bool val);
	void set_config(Config config);

	// --- 2. ADD THIS FUNCTION DECLARATION ---
	ViewMatrix get_view_matrix();
	// ----------------------------------------

	// --- 3. ENSURE THIS VARIABLE IS HERE TOO ---
	bool esp_enabled = false;
	// -------------------------------------------

private:
	std::optional<PlayerInformation> get_closest_player(const GameInformation& game_info, bool only_enemy_team);
	std::string read_in_current_map();
	bool read_in_if_controlled_player_is_shooting();
	ControlledPlayer read_controlled_player_information(uintptr_t player_address);
	std::vector<PlayerInformation> read_other_players(uintptr_t player_address);
	Movement read_controlled_player_movement(uintptr_t player_address);
	Vec3D<float> get_head_bone_position(uintptr_t player_pawn);
	uintptr_t get_list_entity(uintptr_t id, uintptr_t entity_list);
	uintptr_t get_entity_controller_or_pawn(uintptr_t list_entity, uintptr_t id);
	std::optional<PlayerInformation> read_player(uintptr_t entity_list_begin, uintptr_t id, uintptr_t player_address);
	std::optional<PlayerInformation> read_player_in_crosshair(uintptr_t player_controller, uintptr_t player_pawn);

	Config m_config;
	MemoryManager m_process_memory;
	Offsets m_offsets;
	uintptr_t m_client_dll_address = 0;
	bool m_attached_to_process = false;
	GameInformation m_game_information;

	// Constants
	const int button_pressed_value = 65537;
};