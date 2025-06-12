// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include "LuaHandler.h"
#include <safetyhook.hpp>
#include "SRMath.h"
#include "GameConfig.h"
#include "BlingMenu_public.h"
#include "multi.h"
#include <stdfloat>

#include "JuicedAPIPublic.h"



float* CHeight_s = (float*)0xE28128;
float CHeight_remote_player;
SafetyHookInline get_avg_processing_time_T{};

typedef bool(*isCoopT)();
isCoopT isCoop_game = (isCoopT)0x007F7AD0;
bool allow_coop = true;
bool isCoop() {
    return allow_coop && isCoop_game();
}

typedef int(__cdecl* get_remote_player_dataT)();
get_remote_player_dataT get_remote_player_data = (get_remote_player_dataT)0x9D7BD0;

bool multi_CHeight_send(float height) {
    if (!isCoop())
        return false;

    if (!multi_session_get_active_session())
        return false;
    multi_game_send_CHeight_value(height);
    return true;
}
void SafeWriteBuf(void* addr, void* data, size_t len)
{
    unsigned long oldProtect;

    VirtualProtect((void*)addr, len, PAGE_EXECUTE_READWRITE, &oldProtect);
    memcpy((void*)addr, data, len);
    VirtualProtect((void*)addr, len, oldProtect, &oldProtect);
}

struct pcr_camera_focus_data
{
    float angle;
    float distance;
    vector3* offset;
    int facing;
};
vector3 original_offsets[4];
pcr_camera_focus_data copy[4]{};
pcr_camera_focus_data* pcr_camera_focus_data_list = (pcr_camera_focus_data*)0xE8D230;

vector3 pcu_set_active_category_vector_copy{};
static float mod = 0.3f;
SAFETYHOOK_NOINLINE void pcu_set_active_category_camera_midhook(SafetyHookContext& ctx) {
    vector3* offset = (vector3*)ctx.eax;
    memcpy(&pcu_set_active_category_vector_copy, offset, sizeof(vector3));
    float height_diff_feet = (*CHeight_s - 1.0f) * 6.0f;
    float camera_y_adjustment = height_diff_feet * mod;
    pcu_set_active_category_vector_copy.y += camera_y_adjustment;
    ctx.eax = (uintptr_t)&pcu_set_active_category_vector_copy;
}

void pcr_copy() {
    // Copy the struct data
    SafeWriteBuf(&copy, pcr_camera_focus_data_list, sizeof(copy));

    // Also copy the actual vector3 data that the pointers point to
    for (int i = 0; i < 4; i++) {
        if (pcr_camera_focus_data_list[i].offset != nullptr) {
            original_offsets[i] = *pcr_camera_focus_data_list[i].offset;
        }
    }
}

void update_camera_for_height(float height_scale) {
    // Restore original values first
    for (int i = 0; i < 4; i++) {
        if (pcr_camera_focus_data_list[i].offset != nullptr) {
            *pcr_camera_focus_data_list[i].offset = original_offsets[i];
        }
    }

    // Only apply adjustment if not default height
    if (height_scale != 1.0f) {
        float height_diff_feet = (height_scale - 1.0f) * 6.0f;
        float camera_y_adjustment = height_diff_feet * mod;

        for (int i = 0; i < 4; i++) {
            if (pcr_camera_focus_data_list[i].offset != nullptr) {
                pcr_camera_focus_data_list[i].offset->y += camera_y_adjustment;
            }
        }
    }
}

namespace CHooks_Characters {
    static unsigned int VKEYBOARD_RESULTS = 0x4D53E2B5;
    struct savegame
    {
        unsigned int checksum;
        char save_padding[0x94];
        void* mempool_ptr;
        char player_info[0x278];
        char saved_creation_data[0xE454];
        char unknown[0x75000];
    };
    __declspec(naked) bool release_virtual_mempool(void* mempool) {
        __asm {
            push ebp
            mov ebp, esp
            sub esp, __LOCAL_SIZE
            mov esi, mempool
            mov eax, 0xC15720
            call eax
            mov esp, ebp
            pop ebp
            ret
        }
    }
    typedef void(__cdecl* load_characterT)(void* creation_data, void* player_data);
    load_characterT load_character = (load_characterT)0x0081DFC0;

    typedef wchar_t* (__cdecl* get_symbol_textT)(unsigned int* hash);
    get_symbol_textT get_symbol_text = (get_symbol_textT)0x7ED9B0;

    typedef void(__cdecl* load_save_file_to_memoryT)(savegame** a1, const char* a2);
    load_save_file_to_memoryT load_save_file_to_memory = (load_save_file_to_memoryT)0x691E10;
    typedef uintptr_t(__stdcall* get_virtual_mempoolT)(const char* name, size_t size);
    get_virtual_mempoolT get_virtual_mempool = (get_virtual_mempoolT)0xC15470;
    int& num_save_games = *(int*)0x2527BEC;
    int& CurrentIndex = *(int*)0x25283B4;

    typedef int(__cdecl* save_player_infoT)(savegame* a1, bool mission_restart);
    save_player_infoT save_player_info = (save_player_infoT)0x692300;

    struct savegame_file_descriptor
    {
        char filename[65];
        char displayname[256];
        bool corrupt;
        bool invalid_version;
    };
    savegame_file_descriptor* Saved_game_file_desc = (savegame_file_descriptor*)0x140FFC0;

    void loadSave_TEST() {
        uintptr_t* gamesave_mempool = (uintptr_t*)0x2527BD4;
        ((char(*)(void))0x691C50)();
        savegame* savegametest = 0;
        if (gamesave_mempool && *gamesave_mempool) {
            release_virtual_mempool(gamesave_mempool);
            *gamesave_mempool = 0;
        }
        *gamesave_mempool = (uintptr_t)get_virtual_mempool("gamesave load mp", 0x100000);
        load_save_file_to_memory(&savegametest, Saved_game_file_desc[0].filename);


        load_character(&savegametest->saved_creation_data, &savegametest->player_info);
    }

    struct CharacterInfo {
        std::string filename;
        std::string character_name;
    };

    static std::vector<CharacterInfo> character_cache;
    // Function to clear the cache
    void clear_character_cache() {
        character_cache.clear();
    }

    // Function to populate the cache - scans directory and fills vector
    int populate_character_cache() {
        clear_character_cache(); // Clear existing cache first

        WIN32_FIND_DATAA findFileData;
        HANDLE hFind;

        hFind = FindFirstFileA("CGallery\\*.cchar_pc", &findFileData);
        if (hFind == INVALID_HANDLE_VALUE) {
            return 0; // No files found
        }

        do {
            CharacterInfo info;
            info.filename = findFileData.cFileName;

            // Read character name from file
            char full_path[1024];
            sprintf_s(full_path, sizeof(full_path), "CGallery\\%s", findFileData.cFileName);

            FILE* file = nullptr;
            errno_t file_result = fopen_s(&file, full_path, "rb");
            if (file_result != 0 || !file) {
                continue; // Skip this file
            }

            // Read and verify signature
            char signature[8];
            if (fread(signature, 1, 8, file) != 8 || memcmp(signature, "CCHAR01", 8) != 0) {
                fclose(file);
                continue; // Skip invalid files
            }

            // Read name length
            size_t name_len;
            if (fread(&name_len, sizeof(size_t), 1, file) != 1 || name_len == 0 || name_len > 64) {
                fclose(file);
                continue; // Skip invalid files
            }

            // Read character name
            char temp_name[65];
            if (fread(temp_name, sizeof(char), name_len, file) != name_len) {
                fclose(file);
                continue; // Skip invalid files
            }
            temp_name[name_len] = '\0';
            info.character_name = temp_name;

            fclose(file);

            // Add to cache vector
            character_cache.push_back(info);

        } while (FindNextFileA(hFind, &findFileData) != 0);

        FindClose(hFind);
        return character_cache.size(); // Return count
    }

    // Function to get character info by index from cache
    bool get_saved_character_info(int index, char* filename_out, char* character_name_out) {
        if (index < 0 || index >= character_cache.size()) {
            return false;
        }

        // Ensure null termination and bounds checking
        strncpy_s(filename_out, 260, character_cache[index].filename.c_str(), 259);
        filename_out[259] = '\0';

        strncpy_s(character_name_out, 65, character_cache[index].character_name.c_str(), 64);
        character_name_out[64] = '\0';

        return true;
    }


    void save_current_character() {
        wchar_t* wide_text = get_symbol_text(&VKEYBOARD_RESULTS);

        // Convert wchar_t* to char* for character name
        char converted_name[256];
        size_t converted_chars;

        if (wide_text) {
            errno_t result = wcstombs_s(&converted_chars,
                converted_name,
                sizeof(converted_name),
                wide_text,
                _TRUNCATE);

            if (result != 0 && result != STRUNCATE) {
                strcpy_s(converted_name, sizeof(converted_name), "Character");
            }
        }
        else {
            strcpy_s(converted_name, sizeof(converted_name), "Character");
        }

        // Trim whitespace and check if empty
        char* start = converted_name;
        char* end = converted_name + strlen(converted_name) - 1;

        // Skip leading whitespace
        while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
            start++;
        }

        // Skip trailing whitespace
        while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
            end--;
        }

        // Null terminate
        *(end + 1) = '\0';

        // If empty after trimming, use default name
        if (strlen(start) == 0) {
            strcpy_s(converted_name, sizeof(converted_name), "Character");
            start = converted_name;
        }
        else {
            // Move trimmed string to beginning of buffer
            memmove(converted_name, start, strlen(start) + 1);
        }

        // Get current save file name and hash it
        int& CurrentIndex = *(int*)0x25283B4;
        std::string save_filename(Saved_game_file_desc[CurrentIndex].filename);
        std::hash<std::string> hasher;
        size_t hash_value = hasher(save_filename);

        // Create filename: "CharacterName_A1B2C3D4.cchar_pc" (hash of save file name)
        char filename[512];
        sprintf_s(filename, sizeof(filename), "%s_%08X.cchar_pc",
            converted_name, (uint32_t)(hash_value & 0xFFFFFFFF));

        // Full path: "CGallery\\CharacterName_A1B2C3D4.cchar_pc"
        char full_path[1024];
        sprintf_s(full_path, sizeof(full_path), "CGallery\\%s", filename);

        // Create directory if it doesn't exist
        CreateDirectoryA("CGallery", NULL);

        savegame savegametest{};
        save_player_info(&savegametest, false);

        // Save character data to file
        FILE* file = nullptr;
        errno_t file_result = fopen_s(&file, full_path, "wb");
        if (file_result != 0 || !file) {
            return; // Failed to create file
        }

        // Write file header
        char signature[8] = "CCHAR01";
        fwrite(signature, 1, 8, file);

        // Write the character name (after trimming/defaulting)
        size_t name_len = strlen(converted_name);
        fwrite(&name_len, sizeof(size_t), 1, file);
        fwrite(converted_name, sizeof(char), name_len, file);

        // Write character data
        fwrite(savegametest.player_info, 1, 0x278, file);
        fwrite(savegametest.saved_creation_data, 1, 0xE454, file);

        // Write CHeight_s value at the end of the file
        fwrite(CHeight_s, sizeof(float), 1, file);

        fclose(file);
    }
    void load_character_from_file(const char* filename) {
        char full_path[1024];
        sprintf_s(full_path, sizeof(full_path), "CGallery\\%s", filename);

        FILE* file = nullptr;
        errno_t file_result = fopen_s(&file, full_path, "rb");
        if (file_result != 0 || !file) {
            return;
        }

        // Skip signature
        fseek(file, 8, SEEK_SET);

        // Skip name length and name
        size_t name_len;
        fread(&name_len, sizeof(size_t), 1, file);
        fseek(file, name_len, SEEK_CUR);

        // Read character data
        char player_info[0x278];
        char creation_data[0xE454];

        fread(player_info, 1, 0x278, file);
        fread(creation_data, 1, 0xE454, file);

        // Read CHeight_s value from the end of the file
        float saved_height;
        if (fread(&saved_height, sizeof(float), 1, file) == 1) {
            // Successfully read the height value
            if (saved_height > 0.0f) {  // Basic validation
                *CHeight_s = saved_height;
                update_camera_for_height(*CHeight_s);
                multi_CHeight_send(*CHeight_s);
            }
        }

        fclose(file);

        // Load the character using existing function
        load_character(creation_data, player_info);
    }
    const char* CUSTOM_BM_TEST(void* userdata, int action) {
        static char converted_buffer[512]; // Static buffer to hold converted string

        wchar_t* wide_text = get_symbol_text(&VKEYBOARD_RESULTS);
        if (wide_text) {
            size_t converted_chars;
            errno_t result = wcstombs_s(&converted_chars,
                converted_buffer,
                sizeof(converted_buffer),
                wide_text,
                _TRUNCATE);

            if (result == 0 || result == STRUNCATE) {
                return converted_buffer;
            }
        }

        return "NOTHING"; // Return NULL if get_symbol_text failed or conversion failed
    }

}

#pragma optimize("", off)
static bool CHeightMetric = false;
SAFETYHOOK_NOINLINE int lua_func_vint_get_avg_processing_time(lua_State* L) {
    if (L == NULL) {
        return get_avg_processing_time_T.ccall<int>(L);
    }
    const char* cmd = lua_tostring(L, 1);
    if (cmd == NULL) {
        lua_pushnil(L);
        return get_avg_processing_time_T.ccall<int>(L);
    }
    if (strcmp(cmd, "CHeightRead") == 0) {
        lua_pushnumber(L, *CHeight_s);
        multi_CHeight_send(*CHeight_s);
        return 1;
    }
    else if (strcmp(cmd, "CHeightWrite") == 0) {
        *CHeight_s = (float)lua_tonumber(L, 2);
        update_camera_for_height(*CHeight_s);
        multi_CHeight_send(*CHeight_s);
        lua_pushnumber(L, *CHeight_s);
        return 1;
    }
    else if (strcmp(cmd, "CHeightMetric") == 0) {
        lua_pushboolean(L, CHeightMetric);
        return 1;
    }
    else if (strcmp(cmd, "CStilwater_save") == 0) {
        CHooks_Characters::save_current_character();
        return 0;
    }
    else if (strcmp(cmd, "CStilwater_get_num_characters") == 0) {
        int count = CHooks_Characters::populate_character_cache();
        lua_pushnumber(L, count);
        return 1;
    }
    else if (strcmp(cmd, "CStilwater_get_name_characters") == 0) {
        int index = (int)lua_tonumber(L, 2);

        if (index >= 0 && index < CHooks_Characters::character_cache.size()) {
            const std::string& name = CHooks_Characters::character_cache[index].character_name;
            lua_pushlstring(L, name.c_str(), name.length());
            return 1;
        }
        else {
            lua_pushnil(L);
            return 1;
        }
    }

    else if (strcmp(cmd, "CStilwater_get_name_length") == 0) {
        int index = (int)lua_tonumber(L, 2);
        if (index >= 0 && index < CHooks_Characters::character_cache.size()) {
            lua_pushnumber(L, CHooks_Characters::character_cache[index].character_name.length());
            return 1;
        }
        else {
            lua_pushnumber(L, 0);
            return 1;
        }
    }
    // Get a single character code at given index and position
    else if (strcmp(cmd, "CStilwater_get_name_char") == 0) {
        int index = (int)lua_tonumber(L, 2);
        int char_pos = (int)lua_tonumber(L, 3);

        if (index >= 0 && index < CHooks_Characters::character_cache.size()) {
            const std::string& name = CHooks_Characters::character_cache[index].character_name;
            if (char_pos >= 0 && char_pos < name.length()) {
                lua_pushnumber(L, (unsigned char)name[char_pos]);
                return 1;
            }
        }
        lua_pushnumber(L, 0);
        return 1;
    }
    else if (strcmp(cmd, "CStilwater_load_character") == 0) {
        int index = (int)lua_tonumber(L, 2);

        if (index >= 0 && index < CHooks_Characters::character_cache.size()) {
            // Get the filename from cache
            const std::string& filename = CHooks_Characters::character_cache[index].filename;

            // Load the character using existing function
            CHooks_Characters::load_character_from_file(filename.c_str());

            lua_pushboolean(L, 1); // Return true for success
            return 1;
        }
        else {
            lua_pushboolean(L, 0); // Return false for failure
            return 1;
        }
    }
    return get_avg_processing_time_T.ccall<int>(L);
}
#pragma optimize("", on)
inline uintptr_t getplayer(bool provideaddress = false) {
    if (!provideaddress)
        return *(uintptr_t*)(0x21703D4);
    else return 0x21703D4;

}
bool bShouldDisableInCutscene = false;
bool DisableInCutscene() {
    bool* is_cutscene_active = (bool*)0x02527D14;
    return bShouldDisableInCutscene && *is_cutscene_active;
}

void apply_character_scale(uintptr_t player, float scale) {
    if (scale != 1.0f) {
        uintptr_t* char_inst = (uintptr_t*)(player + 0x5E0);
        float* character_scale = (float*)((*char_inst) + 0x238);
        *character_scale = scale;
    }
}

SAFETYHOOK_NOINLINE void human_process_root_bone_modifications_MidHook(SafetyHookContext& ctx) {
#if DBG
    static int last_coop_player = 0;
    if (last_coop_player != get_remote_player_data()) {
        print("CHEIGHT: remote_player 0x%X\n", get_remote_player_data());
        last_coop_player = get_remote_player_data();
    }
#endif
    if (DisableInCutscene())
        return;
    apply_character_scale(getplayer(), *CHeight_s);

    if(isCoop() && (get_remote_player_data() != NULL))
        apply_character_scale(get_remote_player_data(), CHeight_remote_player);

};
constexpr auto save_offset = 0x40;
SafetyHookInline unused_save_func{};
char* __cdecl unused_save_function(uintptr_t save_ptr) {
    char* result = unused_save_func.ccall<char*>(save_ptr);
    print("SAVE: 0x%X\n", save_ptr);
    float* save_height = (float*)(save_ptr + (0x38C52 + save_offset));
    *save_height = *CHeight_s;
    return result;
}

void __cdecl unused_load_function(uintptr_t save_ptr) {
    print("LOAD: 0x%X\n", save_ptr);
    float* save_height = (float*)(save_ptr + (0x38C52 + save_offset));
    print("LOAD save_height: 0x%X, %f\n", save_height, *save_height);
    if (*save_height != 0.f && (*save_height > 0.f))
        *CHeight_s = *save_height;
    else *CHeight_s = 1.f;

    update_camera_for_height(*CHeight_s);
    multi_CHeight_send(*CHeight_s);
}

void patchInt(void* addr, int val) {
    DWORD oldProtect;

    VirtualProtect(addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
    *(int*)addr = val;
    VirtualProtect(addr, 1, oldProtect, &oldProtect);
}

enum camera_free_submodes : BYTE
{
    CFSM_EXTERIOR_CLOSE = 0x0,
    CFSM_INTERIOR_CLOSE = 0x1,
    CFSM_INTERIOR_SPRINT = 0x2,
    CFSM_VEHICLE_DRIVER = 0x3,
    CFSM_VEHICLE_DRIVER_ALT = 0x4,
    CFSM_WATERCRAFT_DRIVER = 0x5,
    CFSM_HELICOPTER_DRIVER = 0x6,
    CFSM_HELICOPTER_FINE_AIM = 0x7,
    CFSM_AIRPLANE_DRIVER = 0x8,
    CFSM_ZOOM = 0x9,
    CFSM_SWIMMING = 0xA,
    CFSM_SPECTATOR = 0xB,
    CFSM_FENCE = 0xC,
    CFSM_RAGDOLL = 0xD,
    CFSM_FALLING = 0xE,
    CFSM_LEAPING = 0xF,
    CFSM_FINE_AIM = 0x10,
    CFSM_FINE_AIM_CROUCH = 0x11,
    CFSM_MELEE_LOCK = 0x12,
    CFSM_FINE_AIM_VEHICLE = 0x13,
    CFSM_FREEFALL = 0x14,
    CFSM_PARACHUTE = 0x15,
    CFSM_HUMAN_SHIELD = 0x16,
    CFSM_SPRINT = 0x17,
};

matrix* ideal_orient = (matrix*)0x25F5B5C;
float* x_shift = (float*)0xE9A660;
bool disable_in_climb = true;
bool canWeCHeight() {
    camera_free_submodes status = *(camera_free_submodes*)0x00E9A5BC;

    switch (status) {
    case CFSM_HELICOPTER_FINE_AIM:
    case CFSM_FINE_AIM_VEHICLE:
    case CFSM_ZOOM:
    case CFSM_AIRPLANE_DRIVER:
    case CFSM_HELICOPTER_DRIVER:
    case CFSM_WATERCRAFT_DRIVER:
    case CFSM_VEHICLE_DRIVER_ALT:
    case CFSM_VEHICLE_DRIVER:
        if(disable_in_climb)
    case CFSM_FENCE:
        return false;

    default:
        return true;
    }
}
float getCHeightCam() {
    float heightDifference = *CHeight_s - 1.0f;

    float cameraAdjustmentMultiplier = 1.2f;
    //printf("camera %p\n", &cameraAdjustmentMultiplier);
    if(*CHeight_s != 1.f)
    return heightDifference * cameraAdjustmentMultiplier;

    return 0.f;
}
vector3* unk = (vector3*)0x25F5D50;

inline float timestep() {
    return *(float*)0x00E84380;
}
#define version "r6"
float lerpFactor = 0.9f;
SafetyHookInline multi_game_process_remote_console_commandT{};
int __cdecl multi_game_process_CHeight_command(unsigned __int8* packet_data, const sender* from)
{
    float received_value;

    memcpy(&received_value, packet_data, sizeof(float));
    if (received_value)
        CHeight_remote_player = received_value;
    print("received_value %f\n", received_value);
    return 0;
}
SafetyHookInline send_local_player_morphsT{};
SAFETYHOOK_NOINLINE void __cdecl send_local_player_morphs(multi_connection* cp, char respond) {
    send_local_player_morphsT.ccall(cp, respond);
    multi_CHeight_send(*CHeight_s);
}

bool CHeightPCR01_hook(const char* filename, std::string& content, size_t size) {
    if (strcmp(filename, "pcr01.lua") == 0) {
        print("HEY PCR!!\n");

        // Find the Pcr01_globals_menu table
        size_t menu_pos = content.find("Pcr01_globals_menu = {");
        if (menu_pos != std::string::npos) {

            // Add the new functions before the Pcr01_globals_menu table
            std::string new_functions =
                "function scale_to_height_string(scale_value)\n"
                "    local is_metric = vint_get_avg_processing_time(\"CHeightMetric\")\n"
                "    \n"
                "    if is_metric then\n"
                "        local total_cm = 182.88 * scale_value\n"
                "        return tostring(floor(total_cm)) .. \"cm\"\n"
                "    else\n"
                "        local total_inches = 72 * scale_value\n"
                "        local feet = floor(total_inches / 12)\n"
                "        local inches = floor(total_inches - (feet * 12))\n"
                "        \n"
                "        return tostring(feet) .. \"ft\" .. tostring(inches)\n"
                "    end\n"
                "end\n"
                "function CHeight_update(menu_label, menu_data)\n"
                "    if menu_data.type == MENU_ITEM_TYPE_NUM_SLIDER then\n"
                "        if menu_data.prev_value == nil then\n"
                "            local current_scale = vint_get_avg_processing_time(\"CHeightRead\")\n"
                "            menu_data.cur_value = (current_scale - 0.83333334) / 0.33333333\n"
                "            menu_data.prev_value = menu_data.cur_value\n"
                "        end\n"
                "        \n"
                "        if menu_data.cur_value ~= menu_data.prev_value then\n"
                "            local scale_value = 0.83333334 + (menu_data.cur_value * 0.33333333)\n"
                "            vint_get_avg_processing_time(\"CHeightWrite\", scale_value)\n"
                "            menu_data.prev_value = menu_data.cur_value\n"
                "        end\n"
                "        \n"
                "        local current_scale = 0.83333334 + (menu_data.cur_value * 0.33333333)\n"
                "        \n"
                "        local h = vint_object_find(\"value_text\", menu_label.control.grp_h)\n"
                "        local height_display = scale_to_height_string(current_scale)\n"
                "        vint_set_property(h, \"text_tag\", height_display)\n"
                "    end\n"
                "	store_set_camera_pos(\"body\",true)\n"
                "end\n\n";

            content.insert(menu_pos, new_functions);

            // Now find the updated position after our insertion
            menu_pos = content.find("Pcr01_globals_menu = {", menu_pos);

            // Find the end of the table
            size_t table_start = content.find("{", menu_pos) + 1;
            size_t table_end = menu_pos;
            int brace_count = 1;
            for (size_t i = table_start; i < content.length() && brace_count > 0; i++) {
                if (content[i] == '{') brace_count++;
                else if (content[i] == '}') brace_count--;
                table_end = i;
            }

            // Extract the table content
            std::string table_content = content.substr(table_start, table_end - table_start);

            // Find and update num_items (look for any number)
            size_t num_items_pos = table_content.find("num_items");
            if (num_items_pos != std::string::npos) {
                size_t equals_pos = table_content.find("=", num_items_pos);
                if (equals_pos != std::string::npos) {
                    size_t number_start = equals_pos + 1;
                    while (number_start < table_content.length() &&
                        (table_content[number_start] == ' ' || table_content[number_start] == '\t')) {
                        number_start++;
                    }

                    size_t number_end = number_start;
                    while (number_end < table_content.length() &&
                        table_content[number_end] >= '0' && table_content[number_end] <= '9') {
                        number_end++;
                    }

                    if (number_end > number_start) {
                        int current_num = std::stoi(table_content.substr(number_start, number_end - number_start));
                        table_content.replace(number_start, number_end - number_start, std::to_string(current_num + 1));
                    }
                }
            }

            // Find the highest numbered item to insert before the last one
            int highest_index = -1;
            size_t search_pos = 0;
            while (true) {
                size_t bracket_pos = table_content.find("[", search_pos);
                if (bracket_pos == std::string::npos) break;

                size_t bracket_end = table_content.find("]", bracket_pos);
                if (bracket_end == std::string::npos) break;

                std::string index_str = table_content.substr(bracket_pos + 1, bracket_end - bracket_pos - 1);
                try {
                    int index = std::stoi(index_str);
                    if (index > highest_index) {
                        highest_index = index;
                    }
                }
                catch (...) {
                    // Not a number, skip
                }

                search_pos = bracket_end + 1;
            }

            if (highest_index >= 0) {
                // Find the last item entry
                std::string last_item_pattern = "[" + std::to_string(highest_index) + "]";
                size_t last_item_pos = table_content.find(last_item_pattern);

                if (last_item_pos != std::string::npos) {
                    // Insert our height item before the last item
                    std::string height_item =
                        "\t[" + std::to_string(highest_index) + "] = { label = \"Height\",\ttype = MENU_ITEM_TYPE_NUM_SLIDER,\tthumb_width = 70, cur_value = 0, on_value_update = CHeight_update},\n\t";

                    table_content.insert(last_item_pos, height_item);

                    // Update the old last item to the next index
                    size_t old_last_pos = table_content.find(last_item_pattern, last_item_pos + height_item.length());
                    if (old_last_pos != std::string::npos) {
                        std::string new_index = "[" + std::to_string(highest_index + 1) + "]";
                        table_content.replace(old_last_pos, last_item_pattern.length(), new_index);
                    }
                }
            }

            // Replace the original table content
            content.replace(table_start, table_end - table_start, table_content);

            return true;
        }
    }
    return false;
}

bool CHeightPCR01_Mystilwater_hook(const char* filename, std::string& content, size_t size) {
    if (strcmp(filename, "pcr01.lua") == 0) {
        print("Adding Mystilwater functionality to PCR01\n");

        // Find insertion point before the menu definitions
        size_t insert_pos = content.find("Pcr01_presets_sex_menu = {");
        if (insert_pos == std::string::npos) {
            print("Could not find insertion point for Mystilwater code\n");
            return false;
        }

        // Add all the Mystilwater functions, ASCII table, and menu definitions
        std::string mystilwater_code =
            "-- Function called when \"Save Character\" is selected\n"
            "function pcr01_mystilwater_save_character(menu_label, menu_data)\n"
            "    vkeyboard_input(\"Enter a name to save the file\", \"Enter character name:\", 64, \"CHARACTER_NAME_TAG\", \"pcr01_mystilwater_save_cb\", true)\n"
            "end\n\n"

            "-- Callback after keyboard input for saving\n"
            "function pcr01_mystilwater_save_cb(success)\n"
            "    if success == true then\n"
            "        debug_print(\"vint\", \"Would save character with name from VKEYBOARD_RESULTS\\n\")\n"
            "        vint_get_avg_processing_time(\"CStilwater_save\")\n"
            "        menu_show(Pcr01_mystilwater_menu, MENU_TRANSITION_SWEEP_LEFT)\n"
            "    end\n"
            "end\n\n"

            "local ascii_chars = {}\n"
            "ascii_chars[32] = \" \"\n"
            "ascii_chars[33] = \"!\"\n"
            "ascii_chars[34] = \"\\\"\"\n"
            "ascii_chars[35] = \"#\"\n"
            "ascii_chars[36] = \"$\"\n"
            "ascii_chars[37] = \"%\"\n"
            "ascii_chars[38] = \"&\"\n"
            "ascii_chars[39] = \"'\"\n"
            "ascii_chars[40] = \"(\"\n"
            "ascii_chars[41] = \")\"\n"
            "ascii_chars[42] = \"*\"\n"
            "ascii_chars[43] = \"+\"\n"
            "ascii_chars[44] = \",\"\n"
            "ascii_chars[45] = \"-\"\n"
            "ascii_chars[46] = \".\"\n"
            "ascii_chars[47] = \"/\"\n"
            "ascii_chars[48] = \"0\"\n"
            "ascii_chars[49] = \"1\"\n"
            "ascii_chars[50] = \"2\"\n"
            "ascii_chars[51] = \"3\"\n"
            "ascii_chars[52] = \"4\"\n"
            "ascii_chars[53] = \"5\"\n"
            "ascii_chars[54] = \"6\"\n"
            "ascii_chars[55] = \"7\"\n"
            "ascii_chars[56] = \"8\"\n"
            "ascii_chars[57] = \"9\"\n"
            "ascii_chars[58] = \":\"\n"
            "ascii_chars[59] = \";\"\n"
            "ascii_chars[60] = \"<\"\n"
            "ascii_chars[61] = \"=\"\n"
            "ascii_chars[62] = \">\"\n"
            "ascii_chars[63] = \"?\"\n"
            "ascii_chars[64] = \"@\"\n"
            "ascii_chars[65] = \"A\"\n"
            "ascii_chars[66] = \"B\"\n"
            "ascii_chars[67] = \"C\"\n"
            "ascii_chars[68] = \"D\"\n"
            "ascii_chars[69] = \"E\"\n"
            "ascii_chars[70] = \"F\"\n"
            "ascii_chars[71] = \"G\"\n"
            "ascii_chars[72] = \"H\"\n"
            "ascii_chars[73] = \"I\"\n"
            "ascii_chars[74] = \"J\"\n"
            "ascii_chars[75] = \"K\"\n"
            "ascii_chars[76] = \"L\"\n"
            "ascii_chars[77] = \"M\"\n"
            "ascii_chars[78] = \"N\"\n"
            "ascii_chars[79] = \"O\"\n"
            "ascii_chars[80] = \"P\"\n"
            "ascii_chars[81] = \"Q\"\n"
            "ascii_chars[82] = \"R\"\n"
            "ascii_chars[83] = \"S\"\n"
            "ascii_chars[84] = \"T\"\n"
            "ascii_chars[85] = \"U\"\n"
            "ascii_chars[86] = \"V\"\n"
            "ascii_chars[87] = \"W\"\n"
            "ascii_chars[88] = \"X\"\n"
            "ascii_chars[89] = \"Y\"\n"
            "ascii_chars[90] = \"Z\"\n"
            "ascii_chars[91] = \"[\"\n"
            "ascii_chars[92] = \"\\\\\"\n"
            "ascii_chars[93] = \"]\"\n"
            "ascii_chars[94] = \"^\"\n"
            "ascii_chars[95] = \"_\"\n"
            "ascii_chars[96] = \"`\"\n"
            "ascii_chars[97] = \"a\"\n"
            "ascii_chars[98] = \"b\"\n"
            "ascii_chars[99] = \"c\"\n"
            "ascii_chars[100] = \"d\"\n"
            "ascii_chars[101] = \"e\"\n"
            "ascii_chars[102] = \"f\"\n"
            "ascii_chars[103] = \"g\"\n"
            "ascii_chars[104] = \"h\"\n"
            "ascii_chars[105] = \"i\"\n"
            "ascii_chars[106] = \"j\"\n"
            "ascii_chars[107] = \"k\"\n"
            "ascii_chars[108] = \"l\"\n"
            "ascii_chars[109] = \"m\"\n"
            "ascii_chars[110] = \"n\"\n"
            "ascii_chars[111] = \"o\"\n"
            "ascii_chars[112] = \"p\"\n"
            "ascii_chars[113] = \"q\"\n"
            "ascii_chars[114] = \"r\"\n"
            "ascii_chars[115] = \"s\"\n"
            "ascii_chars[116] = \"t\"\n"
            "ascii_chars[117] = \"u\"\n"
            "ascii_chars[118] = \"v\"\n"
            "ascii_chars[119] = \"w\"\n"
            "ascii_chars[120] = \"x\"\n"
            "ascii_chars[121] = \"y\"\n"
            "ascii_chars[122] = \"z\"\n"
            "ascii_chars[123] = \"{\"\n"
            "ascii_chars[124] = \"|\"\n"
            "ascii_chars[125] = \"}\"\n"
            "ascii_chars[126] = \"~\"\n\n"

            "function get_character_name_safe(index)\n"
            "    debug_print(\"Getting character name for index: \" .. tostring(index))\n"
            "    local name_length = vint_get_avg_processing_time(\"CStilwater_get_name_length\", index)\n"
            "    name_length = tonumber(name_length) or 0\n"
            "    debug_print(\"Name length: \" .. tostring(name_length))\n"
            "    if name_length == 0 then\n"
            "        debug_print(\"Name length is 0, returning nil\")\n"
            "        return nil\n"
            "    end\n"
            "    local result = \"\"\n"
            "    for i = 0, name_length - 1 do\n"
            "        local char_code = vint_get_avg_processing_time(\"CStilwater_get_name_char\", index, i)\n"
            "        char_code = tonumber(char_code) or 0\n"
            "        if char_code > 0 and ascii_chars[char_code] then\n"
            "            result = result .. ascii_chars[char_code]\n"
            "        end\n"
            "    end\n"
            "    debug_print(\"Built character name: \" .. result)\n"
            "    return result\n"
            "end\n\n"

            "function pcr01_mystilwater_load_select(menu_data, menu_item)\n"
            "    debug_print(\"Character selected: \" .. tostring(menu_item.character_index))\n"
            "pcr01_clear_saved_makeup()\n"
            "pcr01_store_current_makeup()\n"
            "    local success = vint_get_avg_processing_time(\"CStilwater_load_character\", menu_item.character_index)\n"
            "pcr01_store_current_makeup()\n pcr_mark_changed()\n"
            "    if success then\n"
            "        debug_print(\"Character loaded successfully\")\n"
            "    else\n"
            "        debug_print(\"Failed to load character\")\n"
            "    end\n"
            "end\n\n"

            "function pcr01_mystilwater_load_show(menu_data)\n"
            "    debug_print(\"Starting pcr01_mystilwater_load_show\")\n"
            "    menu_data.num_items = 0\n"
            "    local char_count = vint_get_avg_processing_time(\"CStilwater_get_num_characters\")\n"
            "    char_count = tonumber(char_count) or 0\n"
            "    debug_print(\"Character count: \" .. tostring(char_count))\n"
            "    if char_count == 0 then\n"
            "        debug_print(\"No characters found, adding default message\")\n"
            "        menu_data[0] = { label = \"No saved characters found\", type = MENU_ITEM_TYPE_SELECTABLE }\n"
            "        menu_data.num_items = 1\n"
            "        return\n"
            "    end\n"
            "    local valid_items = 0\n"
            "    for i = 0, char_count - 1 do\n"
            "        debug_print(\"Processing character index: \" .. tostring(i))\n"
            "        local character_name = get_character_name_safe(i)\n"
            "        if character_name and character_name ~= \"\" then\n"
            "            debug_print(\"Adding character: \" .. character_name .. \" at menu index: \" .. tostring(valid_items))\n"
            "            menu_data[valid_items] = { \n"
            "                label = character_name,\n"
            "                type = MENU_ITEM_TYPE_SELECTABLE, \n"
            "                character_index = i,\n"
            "                on_select = pcr01_mystilwater_load_select,\n"
            "            }\n"
            "            valid_items = valid_items + 1\n"
            "        else\n"
            "            debug_print(\"Character name was invalid for index: \" .. tostring(i))\n"
            "        end\n"
            "    end\n"
            "    menu_data.num_items = valid_items\n"
            "    debug_print(\"Final menu item count: \" .. tostring(valid_items))\n"
            "end\n\n"

            "-- My Stilwater character save menu\n"
            "Pcr01_mystilwater_save_menu = {\n"
            "    header_label_str = \"Save Character\",\n"
            "    num_items = 1,\n"
            "    btn_tips = Pcr01_untop_btn_tips,\n"
            "    [0] = { label = \"Save Character\", type = MENU_ITEM_TYPE_SELECTABLE, on_select = pcr01_mystilwater_save_character },\n"
            "}\n\n"

            "-- My Stilwater character load menu\n"
            "Pcr01_mystilwater_load_menu = {\n"
            "    header_label_str = \"Load Character\", \n"
            "    num_items = 1,\n"
            "    btn_tips = Pcr01_untop_btn_tips,\n"
            "    on_show = pcr01_mystilwater_load_show,\n"
            "    [0] = { \n"
            "        label = \"No saved characters\", \n"
            "        type = MENU_ITEM_TYPE_SELECTABLE,\n"
            "        filename = \"\",\n"
            "        on_select = nil\n"
            "    },\n"
            "}\n\n"

            "-- Main My Stilwater menu\n"
            "Pcr01_mystilwater_menu = {\n"
            "    header_label_str = \"Character Gallery\",\n"
            "    num_items = 2,\n"
            "    btn_tips = Pcr01_untop_btn_tips,\n"
            "    [0] = { label = \"Save Character\", type = MENU_ITEM_TYPE_SUB_MENU, sub_menu = Pcr01_mystilwater_save_menu },\n"
            "    [1] = { label = \"Load Character\", type = MENU_ITEM_TYPE_SUB_MENU, sub_menu = Pcr01_mystilwater_load_menu },\n"
            "}\n\n";

        // Insert all the Mystilwater code
        content.insert(insert_pos, mystilwater_code);

        // Now modify the Pcr01_presets_menu to add the Mystilwater service
        size_t presets_menu_pos = content.find("Pcr01_presets_menu = {");
        if (presets_menu_pos != std::string::npos) {
            // Find the end of the presets menu
            size_t table_start = content.find("{", presets_menu_pos) + 1;
            size_t table_end = presets_menu_pos;
            int brace_count = 1;
            for (size_t i = table_start; i < content.length() && brace_count > 0; i++) {
                if (content[i] == '{') brace_count++;
                else if (content[i] == '}') brace_count--;
                table_end = i;
            }

            std::string table_content = content.substr(table_start, table_end - table_start);

            // Update num_items from 4 to 5
            size_t num_items_pos = table_content.find("num_items = 4");
            if (num_items_pos != std::string::npos) {
                table_content.replace(num_items_pos, 13, "num_items = 5");
            }

            // Add the Mystilwater service entry after [3]
            size_t bracket_3_pos = table_content.find("[3] = { label = \"CUST_PRESET_AGE\"");
            if (bracket_3_pos != std::string::npos) {
                size_t end_of_line = table_content.find('\n', bracket_3_pos);
                if (end_of_line != std::string::npos) {
                    std::string mystilwater_entry =
                        "\t[4] = { label = \"Character Gallery\", type = MENU_ITEM_TYPE_SUB_MENU, sub_menu = Pcr01_mystilwater_menu },\n";
                    table_content.insert(end_of_line + 1, mystilwater_entry);
                }
            }

            // Replace the original table content
            content.replace(table_start, table_end - table_start, table_content);
        }

        return true;
    }
    return false;
}

void JuicedLUAHook() {
    if (!GameConfig::GetValue("JuicedAPI", "modifyPCR01", 1))
        return;
    if (!JuicedAPI::IsAvailable()) {
         print("Failed JUICED\n");
         return;
    }
    if (JuicedAPI::RegisterVintLuaHook(CHeightPCR01_hook) && JuicedAPI::RegisterVintLuaHook(CHeightPCR01_Mystilwater_hook)) {
        print("Juiced loaded our thing");
    }
    CHeightMetric = GameConfig::GetValue("Settings", "Metric", 0);
}

DWORD WINAPI CHeight_hook(LPVOID lpParameter)
{
    allow_coop = GameConfig::GetValue("Settings", "CoopSync", 1);
    JuicedLUAHook();
    send_local_player_morphsT = safetyhook::create_inline(0x829E50, &send_local_player_morphs);
    static auto pcu_set_active_category_camera_midhook_T = safetyhook::create_mid(0x7C8D41, &pcu_set_active_category_camera_midhook);
    CHeight_remote_player = 1.f;
    Multi_game_packets[17].group_id = 1;
    multi_game_process_remote_console_commandT = safetyhook::create_inline(0x819570, &multi_game_process_CHeight_command);
    pcr_copy();
    DWORD oldProtect;
    SleepEx(250 * 4, 0);
    VirtualProtect(CHeight_s, 1, PAGE_EXECUTE_READWRITE, &oldProtect);
    *CHeight_s = 1.f;
    unused_save_func = safetyhook::create_inline(0x692EA0, &unused_save_function);
    bool BM_Loaded = BlingMenuLoad();
    if (BM_Loaded) {
        BlingMenuAddFloat("CHeight", "Character Height", CHeight_s, []() {
            update_camera_for_height(*CHeight_s);
            multi_CHeight_send(*CHeight_s);
            }, 0.01f, 0.01f, 20.f);
        BlingMenuAddFloat("CHeight", "lerpFactor", &lerpFactor, []() {
            GameConfig::SetDoubleValue("Camera", "lerpFactor", lerpFactor);
            }, 0.01f, 0.01f, 1.5f);
        BlingMenuAddFunc("CHeight", version, NULL);
        BlingMenuAddBool("CHeight", "pcr01: Metric", &CHeightMetric, NULL);
        BlingMenuAddBool("CHeight", "CoopSync", &allow_coop, NULL);
        //BlingMenuAddFunc("CHeight", "load char", CHooks_Characters::loadSave_TEST);
        //BlingMenuAddFuncCustom("CHeight", "VKEYBOARD_RESULTS", NULL, CHooks_Characters::CUSTOM_BM_TEST, NULL);
    }

    lerpFactor = (float)GameConfig::GetDoubleValue("Camera", "lerpFactor", 0.9f);

    if (GameConfig::GetValue("Camera", "AdjustHeightCamera", 1)) {
        static auto height_mod_if = safetyhook::create_mid(0x49CF54, [](SafetyHookContext& ctx) {
            if ((*CHeight_s == 1.f))
                return;
            if (getCHeightCam() != 1.f)
                ctx.eip = 0x49CF5A;
            });


        static auto height_mod = safetyhook::create_mid(0x49CFD6, [](SafetyHookContext& ctx) {
            if ((*CHeight_s == 1.f))
                return;
            vector3* render_snap_shot_pos = (vector3*)(ctx.esp + 0x78);

            float targetAdjustment = canWeCHeight() ? getCHeightCam() : 0.0f;
            static float currentCameraYAdjustment = getCHeightCam();
            currentCameraYAdjustment += (targetAdjustment - currentCameraYAdjustment) * lerpFactor;

            if (std::abs(currentCameraYAdjustment) > 0.001f) {
                *render_snap_shot_pos += ideal_orient->uvec * currentCameraYAdjustment;
                *unk = *render_snap_shot_pos;
            }
            else {
                currentCameraYAdjustment = 0.0f;
            }
            });
        bShouldDisableInCutscene = GameConfig::GetValue("Settings", "ShouldDisableInCutscene", 0);
        BlingMenuAddBool("CHeight", "bShouldDisableInCutscene", &bShouldDisableInCutscene, []() {
            GameConfig::SetValue("Settings", "ShouldDisableInCutscene", bShouldDisableInCutscene);
            });
    }
    patchInt((void*)0xDD316C, (int)&unused_load_function);

    // Have to get ptr from the init function, as in Juiced I don't do an inline hook lol, this means we are basically hooking Juiced's DLL.
    uintptr_t get_avg_processing_time_ptr = *(uintptr_t*)(0x00B91212 + 7);
    get_avg_processing_time_T = safetyhook::create_inline(get_avg_processing_time_ptr, &lua_func_vint_get_avg_processing_time);
    static auto human_process_root_bone_modifications_T = safetyhook::create_mid(0x009A9E4C, &human_process_root_bone_modifications_MidHook, safetyhook::MidHook::Default);
    return 0;
}
void OpenConsole()
{
#if _DEBUG || USE_CON
    AllocConsole();

    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);

    SetConsoleTitleA("Debug Console");
#endif
}
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH: {
        GameConfig::Initialize();
        CreateThread(0, 0, CHeight_hook, 0, 0, 0);
        OpenConsole();
        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

