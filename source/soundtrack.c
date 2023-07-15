
#include "Tools.h"
#include "Rocket/sync.h"

#include "soundtrack.h"

#include "minivorbis.h"


static u8* audio_preload;

static int32_t sample_pos = 0;
static ndspWaveBuf wave_buffer[2];
static uint8_t fill_buffer = 0;
static uint8_t audio_playing = 1;

// For audio streaming (WIP)
static FILE* audioFile;
static OggVorbis_File vorbis;
static long readBytes, playBytes;

static uint8_t* audio_buffer;
static FILE *audioFile;


#define THREAD_AFFINITY (-1)
#define THREAD_STACKSZ (16384)

static volatile bool thread_quit = false;
static LightEvent event;
static Thread thread;


static bool music_preload(u32 need_at_least) {
    static bool end_reached = false;
    if (end_reached) return false;

    if (need_at_least == 0) need_at_least = AUDIO_BLOCKSIZE;

    __asm__ volatile("":::"memory");
    while (readBytes < playBytes + need_at_least) {
        int section = 0;
        long bytes = ov_read(&vorbis, &audio_preload[readBytes], MUSIC_SIZE, 0, 2, 1, &section);
        //printf("bytes read = %ld\n", bytes);
        if (bytes <= 0) {
            printf("music ended\n");
            end_reached = true;
            return false;
        }
        readBytes += bytes;
    }

    //printf("[ok]\n");
    return readBytes < MUSIC_SIZE;
}


double audio_get_row() {
    double row = (double)(sample_pos + SYNC_OFFSET_SEC * SONG_SPS) / ((double)SAMPLES_PER_ROW);
    return row;
}

void audio_pause(void *ignored, int flag) {
    ignored;
    audio_playing = !flag;
    float mix[12];
    memset(mix, 0, sizeof(mix));
    if (audio_playing) {
        mix[0] = 1.0;
        mix[1] = 1.0;
    }
    ndspChnSetMix(0, mix);
}

#ifndef SYNC_PLAYER
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


static void audio_thread(void* bleh) {
    (void)bleh;

    audioFile = fopen("romfs:/music.ogg", "rb");
    if (ov_open_callbacks(audioFile, &vorbis, NULL, 0, OV_CALLBACKS_DEFAULT) != 0) {
        printf("music.ogg invalid.");
        return;
    }
    //vorbis_info* info = ov_info(&vorbis, -1);
    //printf("Ogg file %d Hz, %d channels, %d kbit/s, total audio data %d bytes.\n", info->rate, info->channels, info->bitrate_nominal / 1024, MUSIC_SIZE);

    music_preload(5*AUDIO_BLOCKSIZE);
    printf("audio preload ready\n");

    while (!thread_quit) {
        __asm__ volatile("":::"memory");
        LightEvent_Wait(&event);
        __asm__ volatile("":::"memory");
        //printf("read=%ld play=%ld\n", readBytes, playBytes);
        if (!music_preload(0)) break;
    }
    printf("audio thread done!\n");
}


static void audio_callback(void* ignored) {
    ignored;
    if (wave_buffer[fill_buffer].status != NDSP_WBUF_DONE || (sample_pos + AUDIO_BUFSIZE) * sizeof(int16_t) >= MUSIC_SIZE)
        return;

    uint8_t *dest = (uint8_t*)wave_buffer[fill_buffer].data_pcm16;

    memcpy(dest, &audio_preload[sample_pos * sizeof(int16_t)], AUDIO_BUFSIZE * sizeof(int16_t));
    DSP_FlushDataCache(dest, AUDIO_BUFSIZE * sizeof(int16_t));
    ndspChnWaveBufAdd(0, &wave_buffer[fill_buffer]);
    fill_buffer = !fill_buffer;

    if (audio_playing) sample_pos += AUDIO_BUFSIZE;
    playBytes = sample_pos * sizeof(int16_t);
    //printf("play @ %d, playbytes=%ld\n", sample_pos, playBytes);
    __asm__ volatile("":::"memory");
    LightEvent_Signal(&event);
}

int audio_init_preload(void) {
    audio_preload = (u8*)malloc((1 + MUSIC_SIZE)*sizeof(u8));

    readBytes = 0;
    playBytes = 0;

    LightEvent_Init(&event, RESET_ONESHOT);

    int32_t prio = 0x30;
    svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
    prio -= 1;
    if (prio < 0x18) prio = 0x18;
    if (prio > 0x3f) prio = 0x3f;
    thread = threadCreate(audio_thread, NULL, THREAD_STACKSZ, prio, THREAD_AFFINITY, false);
    printf("created audio thread %p\n", thread);

    return 0;
}

void audio_init_mixer(void) {
    ndspInit();

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);

    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SONG_SPS);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    audio_pause(NULL, false);

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
    thread_quit = true;

    threadJoin(thread, UINT64_MAX);
    threadFree(thread);

    // Sound off
    ndspExit();
    linearFree(audio_buffer);
    free(audio_preload);
    ov_clear(&vorbis);
    fclose(audioFile);
}

void audio_update(void) {
    //music_preload(AUDIO_BLOCKSIZE);
}

