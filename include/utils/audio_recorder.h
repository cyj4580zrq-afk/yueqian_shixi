#ifndef UTILS_AUDIO_RECORDER_H
#define UTILS_AUDIO_RECORDER_H

typedef enum {
    AUDIO_RECORDER_IDLE = 0,
    AUDIO_RECORDER_RECORDING = 1,
    AUDIO_RECORDER_SENDING = 2,
    AUDIO_RECORDER_WAITING_RESPONSE = 3,
    AUDIO_RECORDER_PLAYING = 4,
    AUDIO_RECORDER_DONE = 5,
    AUDIO_RECORDER_ERROR = 6,
} audio_recorder_state_t;

typedef void (*audio_recorder_state_cb_t)(audio_recorder_state_t state);

void audio_recorder_set_state_callback(audio_recorder_state_cb_t cb);
audio_recorder_state_t audio_recorder_get_state(void);
void audio_recorder_mark_done(void);
int audio_recorder_start_record(int duration, const char *wsl_ip, const char *wsl_save_path);
int audio_recorder_stop_record(void);
int audio_recorder_record_and_send(int duration, const char *wsl_ip, const char *wsl_save_path);
int audio_recorder_record(int duration);
int audio_recorder_send(const char *wsl_ip, const char *wsl_save_path);

#endif // UTILS_AUDIO_RECORDER_H
