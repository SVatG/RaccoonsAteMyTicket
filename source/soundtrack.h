
#ifndef SOUNDTRACK_H_
#define SOUNDTRACK_H_

#include "music_meta.h"

#define ROWS_PER_BEAT 4
#define SAMPLES_PER_ROW (SONG_SPB / ROWS_PER_BEAT)

#ifdef SYNC_PLAYER
#define SYNC_OFFSET_SEC 0.35
#else
#define SYNC_OFFSET_SEC 0.0
#endif

#define AUDIO_BLOCKSIZE 16384

#define CHANNELS 2
#define AUDIO_BUFSIZE (512 * CHANNELS)
#define SONG_BPM 136.36363636
#define SONG_BPS (SONG_BPM / 60.0)
#define SONG_SPS 32000
#define SONG_SPB (SONG_SPS / SONG_BPS) 
#define MUSIC_SIZE ((size_t)(MUSIC_LEN_SEC * CHANNELS * SONG_SPS * sizeof(int16_t)))

double audio_get_row();
extern struct sync_cb rocket_callbakcks;

int audio_init_preload();
void audio_init_mixer();
void audio_init_play();
void audio_deinit();
void audio_update();

#endif

