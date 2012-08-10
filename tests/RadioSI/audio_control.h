
#ifndef __AUDIO_HOOK__
#define __AUDIO_HOOK__


typedef struct
{
    char riff[4];
    unsigned int file_size;
    char wave[4];
    char fmt[4];
    unsigned int wave_section_size;
    unsigned short format;
    unsigned short channels;
    unsigned int samplerate;
    unsigned int bytes_per_sec;
    unsigned short align;
    unsigned short bits_per_sample;
    char data[4];
    unsigned int data_size;
}wave_header;


int audio_control_play(int samplerate, short channels, int bits_per_sample, void (*frame_request)(int unk, unsigned short *pcmframe));
void audio_control_destroy(int player_id);

int sync_PlayMelodyInMem(char Unk_0x11, void * MelAddr, int MelSize, int CepId, int Msg, int Unk_0);
int sync_PlayMelody_StopPlayback(int player_id);
int sync_PlayMelody_ChangeVolume(int player_id, int vol);



#endif

