#ifndef LIBRETRO_H__
#define LIBRETRO_H__

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
#if defined(_MSC_VER) && !defined(SN_TARGET_PS3)
/* Hack applied for MSVC when compiling in C89 mode
 * as it isn't C99-compliant. */
#define bool unsigned char
#define true 1
#define false 0
#else
#include <stdbool.h>
#endif
#endif

#ifndef RETRO_CALLCONV
#  if defined(__GNUC__) && defined(__i386__) && !defined(__x86_64__)
#    define RETRO_CALLCONV __attribute__((cdecl))
#  elif defined(_MSC_VER) && defined(_M_X86) && !defined(_M_X64)
#    define RETRO_CALLCONV __cdecl
#  else
#    define RETRO_CALLCONV /* all other platforms only have one calling convention each */
#  endif
#endif

#ifndef RETRO_API
#  if defined(_WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
#    ifdef RETRO_IMPORT_SYMBOLS
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllimport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllimport)
#      endif
#    else
#      ifdef __GNUC__
#        define RETRO_API RETRO_CALLCONV __attribute__((__dllexport__))
#      else
#        define RETRO_API RETRO_CALLCONV __declspec(dllexport)
#      endif
#    endif
#  else
#      if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__CELLOS_LV2__)
#        define RETRO_API RETRO_CALLCONV __attribute__((__visibility__("default")))
#      else
#        define RETRO_API RETRO_CALLCONV
#      endif
#  endif
#endif

//#undef RETRO_API
//#define RETRO_API

struct offset_byte_t {
    int offset;
    char byte;
    int bitCount;
};
typedef struct offset_byte_t offset_byte_t;

struct offset_button_t {
	int offset;
	int duration;
	int button;
	bool down;
};
typedef struct offset_button_t offset_button_t;

RETRO_API void* sameboy_init(void* user_data, const char* rom_data, size_t rom_size, int model, bool fast_boot);
RETRO_API void sameboy_update_rom(void* state, const char* rom_data, size_t rom_size);
RETRO_API void sameboy_free(void* state);
RETRO_API void sameboy_reset(void* state, int model, bool fast_boot);

RETRO_API void sameboy_set_link_targets(void* state, void** linkTargets, size_t count);
RETRO_API void sameboy_set_sample_rate(void* state, double sample_rate);
RETRO_API void sameboy_set_setting(void* state, const char* name, int value);
RETRO_API void sameboy_disable_rendering(void* state, bool disabled);

RETRO_API void sameboy_send_serial_byte(void* state, int offset, char byte, size_t bitCount);
RETRO_API void sameboy_set_midi_bytes(void* state, int offset, const char* byte, size_t count);
RETRO_API void sameboy_set_button(void* state, int duration, int buttonId, bool down);

RETRO_API size_t sameboy_battery_size(void* state);
RETRO_API size_t sameboy_save_battery(void* state, const char* target, size_t size);
RETRO_API void sameboy_load_battery(void* state, const char* source, size_t size);

RETRO_API size_t sameboy_save_state_size(void* state);
RETRO_API size_t sameboy_save_state(void* state, char* target, size_t size);
RETRO_API void sameboy_load_state(void* state, const char* source, size_t size);

RETRO_API void sameboy_update(void* state, size_t requiredAudioFrames);
RETRO_API void sameboy_update_multiple(void** states, size_t stateCount, size_t requiredAudioFrames);

RETRO_API size_t sameboy_fetch_audio(void* state, int16_t* audio);
RETRO_API size_t sameboy_fetch_video(void* state, uint32_t* video);

#ifdef __cplusplus
}
#endif

#endif
