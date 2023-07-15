
#include "Tools.h"
#include "Rocket/sync.h"

#include "soundtrack.h"

// For streaming (WIP/TODO)
u8* music_bin_play;
u32 music_bin_play_block;

u8* music_bin_preload;
//u32 music_bin_preload_block;

#include "minivorbis.h"

static int32_t sample_pos = 0;
static ndspWaveBuf wave_buffer[2];
static uint8_t fill_buffer = 0;
static uint8_t audio_playing = 1;

double audio_get_row() {
    double row = (double)(sample_pos + SYNC_OFFSET_SEC * SONG_SPS) / ((double)SAMPLES_PER_ROW);
    return row;
}

// For audio streaming (WIP)
static FILE* audioFile;
static OggVorbis_File vorbis;
static long readBytes;
static long playBytes;

static bool music_preload(u32 need_at_least) {
    static bool end_reached = false;
    if (end_reached) return false;

    if (need_at_least == 0) need_at_least = AUDIO_BLOCKSIZE/2;

    /*int load_pos = block_id * AUDIO_BLOCKSIZE;
    printf("Preloading block %d != %d @ %d\n", block_id, music_bin_preload_block, load_pos);
    fseek(audioFile, load_pos * sizeof(int16_t), SEEK_SET);
    fread(music_bin_preload, AUDIO_BLOCKSIZE * sizeof(int16_t), 1, audioFile);*/

    while (readBytes < playBytes + need_at_least) {
        int section = 0;
        long bytes = ov_read(&vorbis, &music_bin_preload[readBytes], MUSIC_SIZE, 0, 2, 1, &section);
        printf("preload bytes read = %ld\n", bytes);
        readBytes += bytes;
        if (bytes == 0) {
            end_reached = true;
            return false;
        }
        //music_bin_preload_block = block_id;
    }

    return readBytes < MUSIC_SIZE;
}

#ifndef SYNC_PLAYER
void audio_pause(void *ignored, int flag) {
    ignored;
    audio_playing = !flag;
    #ifndef SYNC_PLAYER
    if (audio_playing) {
        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = 1.0;
        mix[1] = 1.0;
        ndspChnSetMix(0, mix);
    }
    else {
        float mix[12];
        memset(mix, 0, sizeof(mix));
        mix[0] = 0.0;
        mix[1] = 0.0;
        ndspChnSetMix(0, mix);
    }
    #endif
}

void audio_set_row(void *ignored, int row) {
    printf("Set row: %d\n", row);
    ignored;
    sample_pos = row * SAMPLES_PER_ROW;
}

int audio_is_playing(void *d) {
    return audio_playing;
}

struct sync_cb rocket_callbakcks = {
    audio_pause,
    audio_set_row,
    audio_is_playing
};
#endif

static FILE *audioFile;
static u8 audioTempBuf[AUDIO_BUFSIZE * 4];

static void audio_callback(void* ignored) {
    ignored;
    if(wave_buffer[fill_buffer].status == NDSP_WBUF_DONE && (sample_pos + AUDIO_BUFSIZE) * sizeof(int16_t) < MUSIC_SIZE) {
        if(audio_playing == 1) {
            sample_pos += AUDIO_BUFSIZE;
        }
        uint8_t *dest = (uint8_t*)wave_buffer[fill_buffer].data_pcm16;

        // For audio streaming (WIP)
        /*memcpy(dest, audioTempBuf, AUDIO_BUFSIZE * sizeof(int16_t));
        //wantAudioData = 1;
        int block_id = (sample_pos - AUDIO_BUFSIZE) / AUDIO_BLOCKSIZE;
        int play_pos = (sample_pos - AUDIO_BUFSIZE) % AUDIO_BLOCKSIZE;
        if(block_id != music_bin_play_block) {
            printf("Copying audio %d != %d @ %d ;; %d\n", block_id, music_bin_play_block, sample_pos, play_pos);
            memcpy(music_bin_play, music_bin_preload, AUDIO_BLOCKSIZE * sizeof(int16_t));
            music_bin_play_block = block_id;
        }

        memcpy(dest, &music_bin_play[play_pos * sizeof(int16_t)], AUDIO_BUFSIZE * sizeof(int16_t));*/
        memcpy(dest, &music_bin_preload[(sample_pos - AUDIO_BUFSIZE) * sizeof(int16_t)], AUDIO_BUFSIZE * sizeof(int16_t));
        playBytes = sample_pos * sizeof(int16_t);
        //printf("play preload @ %d (%d)\n", sample_pos, sample_pos - AUDIO_BUFSIZE);
        DSP_FlushDataCache(dest, AUDIO_BUFSIZE * sizeof(int16_t));
        ndspChnWaveBufAdd(0, &wave_buffer[fill_buffer]);
        fill_buffer = !fill_buffer;
    }
}

int audio_init_preload(void) {
    // new and lukewarm: predecode an ogg file. saves file size, but precalc sucks.
    // TODO: Convert this to streaming the data in as needed
    audioFile = fopen("romfs:/music.ogg", "rb");
    if (ov_open_callbacks(audioFile, &vorbis, NULL, 0, OV_CALLBACKS_DEFAULT) != 0) {
        printf("music.ogg invalid.");
        return -1;
    }

    // Preload music
    printf("Preloading, please wait...\n");
    printf("This can take a while, especially on an old 3DS, sorry.\n");
    music_bin_preload = (u8*)malloc((1 + MUSIC_SIZE)*sizeof(u8));
    //vorbis_info* info = ov_info(&vorbis, -1);
    //printf("Ogg file %d Hz, %d channels, %d kbit/s, total audio data %d bytes.\n", info->rate, info->channels, info->bitrate_nominal / 1024, MUSIC_SIZE);

    //music_bin_preload_block = 0;
    readBytes = 0;
    playBytes = 0;
    music_preload(3*AUDIO_BLOCKSIZE); // preload one block
    //while (music_preload(music_bin_preload_block + 1)) ;
    /*int readBytes = 0;
    while (readBytes < MUSIC_SIZE) {
        int section = 0;
        long bytes = ov_read(&vorbis, &music_bin_preload[readBytes], MUSIC_SIZE, 0, 2, 1, &section);
        readBytes += bytes;
    }*/

    return 0;
}

static uint8_t* audio_buffer;
void audio_init_mixer(void) {
    // Sound on
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SONG_SPS);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

    audio_buffer = (uint8_t*)linearAlloc(AUDIO_BUFSIZE * sizeof(int16_t) * 2);
    memset(wave_buffer,0,sizeof(wave_buffer));
    wave_buffer[0].data_vaddr = &audio_buffer[0];
    wave_buffer[0].nsamples = AUDIO_BUFSIZE / CHANNELS;
    wave_buffer[1].data_vaddr = &audio_buffer[AUDIO_BUFSIZE * sizeof(int16_t)];
    wave_buffer[1].nsamples = AUDIO_BUFSIZE / CHANNELS;
}

void audio_init_play(void) {
    // Play music
    ndspSetCallback(&audio_callback, 0);
    ndspChnWaveBufAdd(0, &wave_buffer[0]);
    ndspChnWaveBufAdd(0, &wave_buffer[1]);
}

void audio_deinit(void) {
    // Sound off
    ndspExit();
    linearFree(audio_buffer);
    fclose(audioFile);
    ov_clear(&vorbis);
}

void audio_update(void) {
    music_preload(0);
}

