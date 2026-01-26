#ifndef AUDIO_H
#define AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include <portaudio.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// CONSTANTS AND CONFIGURATION
// =============================================================================

#define AUDIO_DEVICE_CHANNELS        2      // Device output channels: stereo
#define AUDIO_DEVICE_SAMPLE_RATE     44100  // Device output sample rate

// Audio buffer usage types
#define AUDIO_BUFFER_USAGE_STATIC    0      // Static audio buffer (for sounds)
#define AUDIO_BUFFER_USAGE_STREAM    1      // Streaming audio buffer (for music/streams)

// =============================================================================
// TYPE DEFINITIONS
// =============================================================================

// Forward declaration of internal audio buffer structure
struct audio_buffer;

/**
 * Wave structure - represents audio data loaded from a file
 */
typedef struct wave {
    unsigned int frameCount;    // Total number of frames (considering channels)
    unsigned int sampleRate;    // Frequency (samples per second)
    unsigned int sampleSize;    // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    unsigned int channels;      // Number of channels (1-mono, 2-stereo, ...)
    void *data;                 // Buffer data pointer
} wave;

/**
 * AudioStream - custom audio stream for real-time audio processing
 */
typedef struct audio_stream {
    struct audio_buffer *buffer;    // Pointer to internal data used by the audio system
    unsigned int sampleRate;        // Frequency (samples per second)
    unsigned int sampleSize;        // Bit depth (bits per sample): 8, 16, 32 (24 not supported)
    unsigned int channels;          // Number of channels (1-mono, 2-stereo, ...)
} audio_stream;

/**
 * Sound - represents a short audio clip loaded into memory
 * Suitable for sound effects and short audio clips (~10 seconds or less)
 */
typedef struct sound {
    audio_stream stream;         // Audio stream
    unsigned int frameCount;     // Total number of frames (considering channels)
} sound;

/**
 * Music - represents a streaming audio source
 * Suitable for background music and longer audio files
 */
typedef struct music {
    audio_stream stream;         // Audio stream
    unsigned int frameCount;     // Total number of frames (considering channels)
    void *ctxData;              // Internal context data (file handle, decoder state, etc.)
} music;

void set_log_level(int level);

// =============================================================================
// DEVICE MANAGEMENT
// =============================================================================

/**
 * Print available host APIs to the console
 */
void list_host_apis(void);
/**
 * Get the name of a host API by index
 */
const char* get_host_api_name(PaHostApiIndex hostApi);
/**
 * Initialize the audio device and system
 * Must be called before using any other audio functions
 */
void init_audio_device(PaHostApiIndex host_api, double sample_rate, unsigned long buffer_size);

/**
 * Close the audio device and cleanup resources
 * Should be called when done using audio functionality
 */
void close_audio_device(void);

/**
 * Check if the audio device is ready and initialized
 * @return true if audio device is ready, false otherwise
 */
bool is_audio_device_ready(void);

/**
 * Set the master volume for all audio output
 * @param volume Volume level (0.0f = silent, 1.0f = full volume)
 */
void set_master_volume(float volume);

/**
 * Get the current master volume
 * @return Current master volume (0.0f to 1.0f)
 */
float get_master_volume(void);

// =============================================================================
// AUDIO BUFFER MANAGEMENT (Internal/Advanced)
// =============================================================================

/**
 * Load an audio buffer with specified parameters
 * @param channels Number of channels
 * @param sampleRate Sample rate in Hz
 * @param size_in_frames Size of buffer in frames
 * @param usage Buffer usage type (AUDIO_BUFFER_USAGE_STATIC or AUDIO_BUFFER_USAGE_STREAM)
 * @return Pointer to audio buffer, or NULL on failure
 */
struct audio_buffer *load_audio_buffer(uint32_t channels, uint32_t sampleRate, uint32_t size_in_frames, int usage);

/**
 * Unload and free an audio buffer
 * @param buffer Pointer to audio buffer to unload
 */
void unload_audio_buffer(struct audio_buffer *buffer);

/**
 * Check if an audio buffer is currently playing
 * @param buffer Pointer to audio buffer
 * @return true if playing, false otherwise
 */
bool is_audio_buffer_playing(struct audio_buffer *buffer);

/**
 * Start playing an audio buffer
 * @param buffer Pointer to audio buffer
 */
void play_audio_buffer(struct audio_buffer *buffer);

/**
 * Stop playing an audio buffer
 * @param buffer Pointer to audio buffer
 */
void stop_audio_buffer(struct audio_buffer *buffer);

/**
 * Pause an audio buffer
 * @param buffer Pointer to audio buffer
 */
void pause_audio_buffer(struct audio_buffer *buffer);

/**
 * Resume a paused audio buffer
 * @param buffer Pointer to audio buffer
 */
void resume_audio_buffer(struct audio_buffer *buffer);

/**
 * Set the volume of an audio buffer
 * @param buffer Pointer to audio buffer
 * @param volume Volume level (0.0f = silent, 1.0f = full volume)
 */
void set_audio_buffer_volume(struct audio_buffer *buffer, float volume);

/**
 * Set the pitch of an audio buffer
 * @param buffer Pointer to audio buffer
 * @param pitch Pitch multiplier (1.0f = normal, 2.0f = double speed/octave higher)
 */
void set_audio_buffer_pitch(struct audio_buffer *buffer, float pitch);

/**
 * Set the pan (stereo positioning) of an audio buffer
 * @param buffer Pointer to audio buffer
 * @param pan Pan position (0.0f = full left, 0.5f = center, 1.0f = full right)
 */
void set_audio_buffer_pan(struct audio_buffer *buffer, float pan);

/**
 * Add an audio buffer to the internal tracking system
 * @param buffer Pointer to audio buffer
 */
void track_audio_buffer(struct audio_buffer *buffer);

/**
 * Remove an audio buffer from the internal tracking system
 * @param buffer Pointer to audio buffer
 */
void untrack_audio_buffer(struct audio_buffer *buffer);

// =============================================================================
// WAVE MANAGEMENT
// =============================================================================

/**
 * Load wave data from file
 * Supports WAV, OGG, FLAC and other formats supported by libsndfile
 * @param filename Path to audio file
 * @return Wave structure containing audio data
 */
wave load_wave(const char* filename);

/**
 * Check if a wave structure contains valid audio data
 * @param wave Wave structure to validate
 * @return true if wave is valid, false otherwise
 */
bool is_wave_valid(wave wave);

/**
 * Unload wave data and free memory
 * @param wave Wave structure to unload
 */
void unload_wave(wave wave);

// =============================================================================
// SOUND MANAGEMENT
// =============================================================================

/**
 * Create a sound from existing wave data
 * @param wave Wave data to create sound from
 * @return Sound structure
 */
sound load_sound_from_wave(wave wave);

/**
 * Load a sound directly from file
 * Suitable for sound effects and short audio clips
 * @param filename Path to audio file
 * @return Sound structure
 */
sound load_sound(const char* filename);

/**
 * Check if a sound structure is valid
 * @param sound Sound structure to validate
 * @return true if sound is valid, false otherwise
 */
bool is_sound_valid(sound sound);

/**
 * Unload sound and free resources
 * @param sound Sound structure to unload
 */
void unload_sound(sound sound);

/**
 * Play a sound
 * @param sound Sound to play
 */
void play_sound(sound sound);

/**
 * Pause a sound
 * @param sound Sound to pause
 */
void pause_sound(sound sound);

/**
 * Resume a paused sound
 * @param sound Sound to resume
 */
void resume_sound(sound sound);

/**
 * Stop a sound
 * @param sound Sound to stop
 */
void stop_sound(sound sound);

/**
 * Check if a sound is currently playing
 * @param sound Sound to check
 * @return true if playing, false otherwise
 */
bool is_sound_playing(sound sound);

/**
 * Set the volume of a sound
 * @param sound Sound to modify
 * @param volume Volume level (0.0f = silent, 1.0f = full volume)
 */
void set_sound_volume(sound sound, float volume);

/**
 * Set the pitch of a sound
 * @param sound Sound to modify
 * @param pitch Pitch multiplier (1.0f = normal, 2.0f = double speed/octave higher)
 */
void set_sound_pitch(sound sound, float pitch);

/**
 * Set the pan (stereo positioning) of a sound
 * @param sound Sound to modify
 * @param pan Pan position (0.0f = full left, 0.5f = center, 1.0f = full right)
 */
void set_sound_pan(sound sound, float pan);

// =============================================================================
// AUDIO STREAM MANAGEMENT
// =============================================================================

/**
 * Create an audio stream for real-time audio processing
 * @param sample_rate Sample rate in Hz
 * @param sample_size Sample size in bits (8, 16, or 32)
 * @param channels Number of channels (1 = mono, 2 = stereo)
 * @return Audio stream structure
 */
audio_stream load_audio_stream(unsigned int sample_rate, unsigned int sample_size, unsigned int channels);

/**
 * Unload an audio stream and free resources
 * @param stream Audio stream to unload
 */
void unload_audio_stream(audio_stream stream);

/**
 * Start playing an audio stream
 * @param stream Audio stream to play
 */
void play_audio_stream(audio_stream stream);

/**
 * Pause an audio stream
 * @param stream Audio stream to pause
 */
void pause_audio_stream(audio_stream stream);

/**
 * Resume a paused audio stream
 * @param stream Audio stream to resume
 */
void resume_audio_stream(audio_stream stream);

/**
 * Check if an audio stream is currently playing
 * @param stream Audio stream to check
 * @return true if playing, false otherwise
 */
bool is_audio_stream_playing(audio_stream stream);

/**
 * Stop an audio stream
 * @param stream Audio stream to stop
 */
void stop_audio_stream(audio_stream stream);

/**
 * Set the volume of an audio stream
 * @param stream Audio stream to modify
 * @param volume Volume level (0.0f = silent, 1.0f = full volume)
 */
void set_audio_stream_volume(audio_stream stream, float volume);

/**
 * Set the pitch of an audio stream
 * @param stream Audio stream to modify
 * @param pitch Pitch multiplier (1.0f = normal, 2.0f = double speed/octave higher)
 */
void set_audio_stream_pitch(audio_stream stream, float pitch);

/**
 * Set the pan (stereo positioning) of an audio stream
 * @param stream Audio stream to modify
 * @param pan Pan position (0.0f = full left, 0.5f = center, 1.0f = full right)
 */
void set_audio_stream_pan(audio_stream stream, float pan);

/**
 * Update an audio stream with new audio data
 * Used for real-time audio processing and procedural audio
 * @param stream Audio stream to update
 * @param data Pointer to audio data (format should match stream parameters)
 * @param frame_count Number of frames to update
 */
void update_audio_stream(audio_stream stream, const void *data, int frame_count);

// =============================================================================
// MUSIC MANAGEMENT
// =============================================================================

/**
 * Load a music stream from file
 * Suitable for background music and longer audio files
 * Music is streamed from disk to save memory
 * @param filename Path to audio file
 * @return Music structure
 */
music load_music_stream(const char* filename);

/**
 * Check if a music structure is valid
 * @param music Music structure to validate
 * @return true if music is valid, false otherwise
 */
bool is_music_valid(music music);

/**
 * Unload music stream and free resources
 * @param music Music structure to unload
 */
void unload_music_stream(music music);

/**
 * Start playing music
 * @param music Music to play
 */
void play_music_stream(music music);

/**
 * Pause music playback
 * @param music Music to pause
 */
void pause_music_stream(music music);

/**
 * Resume paused music
 * @param music Music to resume
 */
void resume_music_stream(music music);

/**
 * Stop music playback
 * @param music Music to stop
 */
void stop_music_stream(music music);

/**
 * Seek to a specific position in music
 * @param music Music to seek
 * @param position Position in seconds to seek to
 */
void seek_music_stream(music music, float position);

/**
 * Update music stream buffers
 * Must be called regularly when playing music to maintain continuous playback
 * @param music Music stream to update
 */
void update_music_stream(music music);

/**
 * Check if music is currently playing
 * @param music Music to check
 * @return true if playing, false otherwise
 */
bool is_music_stream_playing(music music);

/**
 * Set the volume of music
 * @param music Music to modify
 * @param volume Volume level (0.0f = silent, 1.0f = full volume)
 */
void set_music_volume(music music, float volume);

/**
 * Set the pitch of music
 * @param music Music to modify
 * @param pitch Pitch multiplier (1.0f = normal, 2.0f = double speed/octave higher)
 */
void set_music_pitch(music music, float pitch);

/**
 * Set the pan (stereo positioning) of music
 * @param music Music to modify
 * @param pan Pan position (0.0f = full left, 0.5f = center, 1.0f = full right)
 */
void set_music_pan(music music, float pan);

/**
 * Get the total length of music in seconds
 * @param music Music to query
 * @return Total length in seconds
 */
float get_music_time_length(music music);

/**
 * Get the current playback position in seconds
 * @param music Music to query
 * @return Current position in seconds
 */
float get_music_time_played(music music);

#ifdef __cplusplus
}
#endif

#endif // AUDIO_H
