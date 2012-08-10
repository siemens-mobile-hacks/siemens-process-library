
#include <swilib.h>
#include "audio_control.h"
#include <spl/processbridge.h>

/*
const unsigned char WavHdr[44] =
{
    0x52, 0x49, 0x46, 0x46,
    0x26, 0xbd, 0x17, 0x01,
    0x57, 0x41, 0x56, 0x45,
    0x66, 0x6d, 0x74, 0x20,
    0x12, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x02, 0x00,
    0x44, 0xac, 0x00, 0x00,
    0x88, 0x58, 0x01, 0x00,
    0x02, 0x00, 0x10, 0x00,
    0x00, 0x00, 0x64, 0x61,
    0x74, 0x61, 0x00, 0xbd
};
*/

unsigned char WavHdr[44] =
{
    0x52, 0x49, 0x46, 0x46, 0x6A, 0x88, 0x04, 0x00, 0x57, 0x41, 0x56, 0x45, 0x66, 0x6D, 0x74, 0x20,
    0x10, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x40, 0x1F, 0x00, 0x00, 0x80, 0x3E, 0x00, 0x00,
    0x02, 0x00, 0x08, 0x00, 0x64, 0x61, 0x74, 0x61, 0x06, 0x88, 0x04, 0x00
};

char audio_control_fake_audio[8192] = {0};


#define PCM_HOOK_FUNC *((unsigned int *)RamAudioHookProc())

#define SafeProcessRun(func, ret_type, run_type, args_cnt, ...)         \
    void *args = pack_args(args_cnt, __VA_ARGS__);                      \
    void *retrn = BridgeMessageSend((void *)func, run_type, args);\
    return (ret_type)retrn;


int sync_PlayMelodyInMem(char Unk_0x11, void * MelAddr, int MelSize, int CepId, int Msg, int Unk_0)
{
    SafeProcessRun(PlayMelodyInMem, int, NU_SYNCHRONIZED_PROC, 6, Unk_0x11, MelAddr, MelSize, CepId, Msg, Unk_0);
    //return PlayMelodyInMem(Unk_0x11, MelAddr, MelSize, CepId, Msg, Unk_0);
}


int sync_PlayMelody_StopPlayback(int player_id)
{
    SafeProcessRun(PlayMelody_StopPlayback, int, NU_SYNCHRONIZED_PROC, 1, player_id);
    //return PlayMelody_StopPlayback(player_id);
}


int sync_PlayMelody_ChangeVolume(int player_id, int vol)
{
    SafeProcessRun(PlayMelody_ChangeVolume, int, NU_SYNCHRONIZED_PROC, 2, player_id, vol);
    //return PlayMelody_ChangeVolume(player_id, vol);
}


int audio_control_play(int samplerate, short channels, int bits_per_sample, void (*frame_request)(int unk, unsigned short *pcmframe))
{
    PCM_HOOK_FUNC = (unsigned int)(frame_request);

    memcpy((char *)audio_control_fake_audio, ( void * ) ( WavHdr ), 44 );

    wave_header *header = (wave_header*)audio_control_fake_audio;
    header->samplerate = samplerate;
    header->channels = channels;
    header->bits_per_sample = bits_per_sample;
    header->bytes_per_sec = bits_per_sample * channels * samplerate / 8;
    header->data_size = 30*1024*1024;
    header->file_size = header->data_size+44;

    /* Запускаем буффер на проигрывание */
    return sync_PlayMelodyInMem( 0x11, audio_control_fake_audio, sizeof(audio_control_fake_audio), MMI_CEPID, 0, 0 );
}


void audio_control_destroy(int player_id)
{
    sync_PlayMelody_StopPlayback(player_id);
    PCM_HOOK_FUNC = 0;
}


