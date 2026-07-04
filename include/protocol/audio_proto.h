#ifndef PROTOCOL_AUDIO_PROTO_H
#define PROTOCOL_AUDIO_PROTO_H

typedef void (*audio_proto_callback_t)(const char *wav_file_path);

int audio_proto_start_listen(int port, const char *save_dir, audio_proto_callback_t cb);
void audio_proto_stop_listen(void);

#endif // PROTOCOL_AUDIO_PROTO_H