#pragma once

#include <windows.h>
#include <string>

#ifdef __cplusplus
extern "C" {
#endif

    // Forward declare the callback type (must match JuicedAPI.h's definition)
    typedef bool (*LuaHookString)(const char* filename, std::string& content, size_t size);

    typedef bool (*RegisterVintLuaHookStringFunc)(LuaHookString callback);
    typedef bool (*UnregisterVintLuaHookStringFunc)(LuaHookString callback);
    typedef void (*ClearVintLuaHooksFunc)();

    class JuicedAPI {
    private:
        static HMODULE s_dfEngineModule;
        static RegisterVintLuaHookStringFunc s_registerFunc;
        static UnregisterVintLuaHookStringFunc s_unregisterFunc;
        static ClearVintLuaHooksFunc s_clearFunc;
        static bool s_initialized;

        JuicedAPI() = delete;
        ~JuicedAPI() = delete;

        // Initialize function pointers
        static bool Initialize() {
            if (s_initialized) {
                return s_dfEngineModule != NULL;
            }

            s_initialized = true;

            s_dfEngineModule = GetModuleHandleA("DFEngine.dll");
            if (!s_dfEngineModule) {
                return false;
            }

            // Try to get the function addresses
            s_registerFunc = (RegisterVintLuaHookStringFunc)GetProcAddress(s_dfEngineModule, "RegisterVintLuaHookString");
            s_unregisterFunc = (UnregisterVintLuaHookStringFunc)GetProcAddress(s_dfEngineModule, "UnregisterVintLuaHookString");

            // All functions must be available
            if (!s_registerFunc || !s_unregisterFunc) {
                s_registerFunc = nullptr;
                s_unregisterFunc = nullptr;
                return false;
            }

            return true;
        }

    public:
        // Check if DFEngine.dll is loaded and has our functions
        static bool IsAvailable() {
            return Initialize();
        }

        // Register a Vint Lua hook callback
        static bool RegisterVintLuaHook(LuaHookString callback) {
            if (!Initialize() || !callback) {
                return false;
            }
            return s_registerFunc(callback);
        }

        // Unregister a Vint Lua hook callback
        static bool UnregisterVintLuaHook(LuaHookString callback) {
            if (!Initialize() || !callback) {
                return false;
            }
            return s_unregisterFunc(callback);
        }

        // Get the DFEngine module handle (for advanced usage)
        static HMODULE GetDFEngineModule() {
            Initialize();
            return s_dfEngineModule;
        }
    };
    inline HMODULE JuicedAPI::s_dfEngineModule = NULL;
    inline RegisterVintLuaHookStringFunc JuicedAPI::s_registerFunc = nullptr;
    inline UnregisterVintLuaHookStringFunc JuicedAPI::s_unregisterFunc = nullptr;
    inline bool JuicedAPI::s_initialized = false;

#ifdef __cplusplus
}
#endif