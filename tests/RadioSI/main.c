
#include <swilib.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <spl/process.h>
#include <spl/coreevent.h>
#include <spl/thread.h>
#include <spl/task.h>
#include <spl/csm.h>
#include <spl/gui.h>
#include <spl/queue.h>
#include <spl/lcd_primitives.h>
#include "decoder.h"
#include "audio.h"
#include "audio_control.h"



extern const unsigned int bufferizing;
extern const unsigned int task_priority;
extern const unsigned int cfg_volume;

int csm_id, gui_id;
RECT canvas;
LCDLAYER *mmi_layer;
char cwd[256];

char clrWhite[]= {0xFF,0xFF,0xFF,0x64};
char clrBlack[]= {0x00,0x00,0x00,0x64};
char clrGreen[]= {0x00,0xFF,0x00,0x64};
char clrRed[]  = {0xFF,0x00,0x00,0x64};
int scr_w,scr_h;
int player = -1;
int switches = 0;
int sample_fmt = 16;
int decode_thread = -1;
int queue_handle = -1;


#ifdef NEWSGOLD
#define max_volume_level 15
#else
#define max_volume_level 5
#endif

int volume = max_volume_level;


char fname[128];
int flush_buffer = 0;


#define thread_stack_size 40*1024

typedef struct
{
    void *proc;
    void *p[3];
    void *ret;
    NU_TASK *runner;
    int unloked;
}queue_struct __attribute__((aligned(4)));

#define queue_struct_size sizeof(queue_struct)/4
#define queue_size  10* sizeof(queue_struct)
char queue_data[queue_size];

char fon_img[128];
char icy_name[56] = "Unk";
char icy_description[56] = "Unk";
char icy_br[25];
WSHDR *icy_title = 0, *status = 0;

typedef struct
{
    char *buf;
    unsigned char *pos;
    char *end;
    size_t size;
    char flushing;
}AudioBuffer;

static char *l_buf = 0;
int stop_decode = 0, swithed = 1;
AudioBuffer buffer1 = {.buf = 0}, buffer2 = {.buf = 0}, *buffer = 0;

extern unsigned long samplerate;
extern int bitrate;
extern unsigned char channels;



void switch_buffers();
void decode_stream(AudioBuffer *buf);
void decode_into_next_buffer();

void send_update_status()
{
    queue_struct mess = {REDRAW, {0,0,0}, 0, 0, 0};
    NU_Send_To_Queue(getQueueDataByID(queue_handle), &mess, queue_struct_size, NU_NO_SUSPEND);
}

void send_switch_buffers()
{
    queue_struct mess = {switch_buffers, {0,0,0}, 0, 0, 0};
    NU_Send_To_Queue(getQueueDataByID(queue_handle), &mess, queue_struct_size, NU_NO_SUSPEND);
}

void send_need_decode()
{
    queue_struct mess = {decode_into_next_buffer, {0,0,0},0, 0, 0};
    NU_Send_To_Queue(getQueueDataByID(queue_handle), &mess, queue_struct_size, NU_NO_SUSPEND);
}


void audio_control_frame_request(int unk, unsigned short *pcmframe)
{
    if(stop_decode) return;

    if((char*)(buffer->pos) == buffer->buf) {
        /// need new data
        send_need_decode();
    }

    if(swithed < 1)
        return;

    memcpy(pcmframe, (char *)buffer->pos, 640*2);
    buffer->pos += 640*2;

    if(((char*)buffer->pos) >= buffer->end && swithed) {
        swithed--;

        if(flush_buffer)
            send_switch_buffers();
        else
            switch_buffers();
    }

}



int get_header_param(const char *head, const char *p, char *data)
{
    const char *name = strstr(head, p);

    if(name) {
        name += strlen(p);
        char *end = strchr(name, '\r');

        if(end)
        {
            while(isspace(*name) && *name) name++;
            if(!*name) return -1;

            memcpy(data, name, end-name);
            data[end-name] = 0;
            return 0;
        }
    }

    return -1;
}


void parse_info(const char *info)
{
    get_header_param(info, "icy-name:", icy_name);
    get_header_param(info, "icy-description:", icy_description);
    get_header_param(info, "icy-br:", icy_br);

    REDRAW();
}

void updateStreamTitle(const char *title)
{
    utf8_2ws(icy_title, title, 256);
    REDRAW();
}


void switch_buffers()
{
    switches++;
    AudioBuffer *new = buffer->buf == buffer1.buf? &buffer2 : &buffer1;

    if(new->buf == new->end) {
        PlayMelody_StopPlayback(player);
        wsprintf(status, "stoped");
        send_update_status();
    }

    buffer->end = buffer->buf;
    buffer = new;
    swithed++;
}


void decode_into_next_buffer()
{
    decode_stream(buffer->buf == buffer1.buf? &buffer2 : &buffer1);
}


void decode_stream(AudioBuffer *buf)
{
    if(stop_decode) return;

    printf(" =====> decode_stream\n");

    static char *l_pos = 0;
    static int l_buf_sz = 0;


    buf->flushing = 1;
    flush_buffer = 1;

    int seek = 0;
    int _samples, _channels, _channel_mask;
    void *data = request_frame(&_samples, &_channels, &_channel_mask);

    //memset(buf->buf, 0, buf->size);
    if(l_buf)
    {
        memcpy(buf->buf, l_pos, l_buf_sz);
        seek += l_buf_sz;
        free(l_buf);
        l_buf = 0;
        l_buf_sz = 0;
    }

    while(data)
    {
        if(stop_decode) break;
        if(!_samples ) goto dec;

        int data_size = _samples*(sample_fmt/8);
        if(seek + data_size <= (int)buf->size)
        {
            //printf("decode: Data has fully placed in buffer\n");
            get_audio_16bit(data, buf->buf+seek, _samples, sample_fmt, _channels, _channel_mask);
            seek += data_size;
        } else {

            printf("decode: Data has parted placed in buffer\n");
            int cp_bytes = data_size;
            int need_write = buf->size - seek;
            l_buf = malloc(cp_bytes+1);

            get_audio_16bit(data, l_buf, _samples, sample_fmt, _channels, _channel_mask);
            memcpy(buf->buf+seek, l_buf, need_write);
            seek += need_write;
            l_pos = l_buf + need_write;

            l_buf_sz = cp_bytes - need_write;
            printf("decode: buffer left %d bytes\n", l_buf_sz);
            break;
        }

dec:
        if(stop_decode) break;
        data = request_frame(&_samples, &_channels, &_channel_mask);
    }

    buf->pos = buf->buf;
    buf->end = buf->buf+seek;

    printf("seek: %d\n", seek);

    buf->flushing = 0;
    flush_buffer = 0;
}


int decode_thread_handle(void *argv)
{
    unsigned long recv_size;
    queue_struct message = {0, {0,0,0},0,0,0};
    while(NU_Receive_From_Queue(getQueueDataByID(queue_handle), &message, queue_struct_size, &recv_size, NU_SUSPEND) == NU_SUCCESS)
    {
        if(recv_size != queue_struct_size) {
            ShowMSG(1, (int)"It`s a o_O");
            return -1;
        }

        if(message.runner)
            NU_Suspend_Task(message.runner);

        if(message.proc)
            message.ret = ((void *(*)(void*, void*, void*))message.proc)(message.p[0], message.p[1], message.p[2]);

        message.unloked = 1;
        if(message.runner)
            NU_Resume_Task(message.runner);
    }

    return 0;
}


HObj ToObs(int handle)
{
    if(handle == -1) return 0;
    HObj Tobj;
    Tobj = (HObj)GetPlayObjById(handle);
    if(!Tobj) return 0;
    int ret = (( int*)Tobj)[0x3d0/4];
    return ret;
}


void start_decoder()
{
    stop_decode = 0;
    if(init_decoder(fname))
    {
        ShowMSG(1, (int)"Decoder init error");
        return;
    }

    int buf_size = (640*2) * bufferizing;


    buffer1.buf = malloc(buf_size+1);
    buffer1.pos = buffer1.buf;
    buffer1.size = buf_size;
    buffer1.flushing = 0;

    buffer2.buf = malloc(buf_size+1);
    buffer2.pos = buffer2.buf;
    buffer2.size = buf_size;
    buffer2.flushing = 0;

    decode_stream(&buffer1);
    //decode_stream(&buffer2);

    buffer = &buffer1;


    player = audio_control_play(samplerate, channels, 16, audio_control_frame_request);

#ifdef NEWSGOLD
    int perc1 = volume*100 / max_volume_level;
    int vol = perc1*5/100;
    PlayMelody_ChangeVolume(player, vol);
#else
    PlayMelody_ChangeVolume(player, volume);
#endif
    wsprintf(status, "playing");
    REDRAW();
}


void stopDecode()
{
    stop_decode = 1;

    while(flush_buffer)
        NU_Sleep(4);
}


void terminate_audio_system()
{
    decoder_destroy();

    if(buffer1.buf)
        free(buffer1.buf);

    if(buffer1.buf)
        free(buffer2.buf);

    if(l_buf)
        free(l_buf);
}


void onRedrawGUI(int id)
{
    uint32_t red = 0xFF0000FF;
    lcd_draw_fillrect(mmi_layer, 0, 0, canvas.x2, canvas.y2, (char*)&red);

    WSHDR *ws = AllocWS(256);

    if(!*icy_br)
        sprintf(icy_br, "%d", bitrate);

    wsprintf(ws, "Station: %t\nDescr.: %t\nTitle: %w\nBitrate: %s\nSamplerate: %d\nChannels: %d\nVolume: %d\nStatus: %w",
             icy_name, icy_description, icy_title,
             icy_br, samplerate, channels, volume, status);

    lcd_draw_text(mmi_layer, ws, 4, 10+GetFontYSIZE(FONT_SMALL)*2, scr_w-1,scr_h-1,FONT_SMALL, 0, clrWhite, 0);

    FreeWS(ws);
    displayLayer(mmi_layer);
}


void onCreateGUI(int id)
{
    mmi_layer = *RamMMILCDLayer();
    SetCpuClockTempHi(2);

    icy_title = AllocWS(256);
    status = AllocWS(56);
    sprintf(fon_img, "%sfon.png", cwd);

    volume = cfg_volume;

    queue_handle = createQueue("dec", queue_data, queue_size, NU_FIXED_SIZE, queue_struct_size, NU_FIFO);

    TaskConf config;
    initTaskConf(&config);

    config.prio = task_priority;
    setTaskConfigStackSize(&config, thread_stack_size);

    decode_thread = createConfigurableThread(&config, decode_thread_handle, 0, 1);

    queue_struct mess = {start_decoder, {0,0,0}, 0,0,0};
    NU_Send_To_Queue(getQueueDataByID(queue_handle), &mess, queue_struct_size, NU_NO_SUSPEND);
}


void onCloseGUI(int id)
{
    stopDecode();
    destroyThread(decode_thread);

    audio_control_destroy(player);
    terminate_audio_system();

    destroyQueue(queue_handle);
    FreeWS(icy_title);
    FreeWS(status);
}


void onFocusGUI(int id)
{
}


void onUnfocusGUI(int id)
{
}


void onKeyGUI(int id, GUI_MSG *msg)
{
    if ((msg->gbsmsg->msg==KEY_DOWN || msg->gbsmsg->msg==LONG_PRESS))
    {
        switch(msg->gbsmsg->submess)
        {
        case RIGHT_SOFT:
            guiClose(id);
            return;

        case '1':
            stopDecode();
            audio_control_destroy(player);
            terminate_audio_system();
            queue_struct mess = {start_decoder, {0,0,0}, 0,0,0};
            NU_Send_To_Queue(getQueueDataByID(queue_handle), &mess, queue_struct_size, NU_NO_SUSPEND);
            break;

        case '5':
            PlayMelody_PausePlayback(player);
            wsprintf(status, "paused");
            break;

        case '6':
            PlayMelody_ResumePlayBack(player);
            wsprintf(status, "playing");
            break;

        case VOL_UP_BUTTON:
            if(volume+1 <= max_volume_level)
#ifdef NEWSGOLD
                Obs_Sound_SetVolumeEx((HObj)ToObs(player), ++volume, 1);
#else
                PlayMelody_ChangeVolume(player, ++volume);
#endif
            break;

        case VOL_DOWN_BUTTON:
            if(volume-1 > -1)
#ifdef NEWSGOLD
                Obs_Sound_SetVolumeEx((HObj)ToObs(player), --volume, 1);
#else
                PlayMelody_ChangeVolume(player, --volume);
#endif
            break;

        }
    }

    REDRAW();
    return;
}



void onCreateCSM(CSM_RAM *ram)
{
    canvas.x = 0;
    canvas.y = 0;
    scr_w = canvas.x2 = ScreenW();
    scr_h = canvas.y2 = ScreenH();

    gui_id = guiCreate(&canvas, onRedrawGUI,
                       onCreateGUI,
                       onCloseGUI,
                       onFocusGUI,
                       onUnfocusGUI,
                       onKeyGUI,
                       NULL);

    csmBindGUI(csm_id, gui_id);
}


void onCloseCSM(CSM_RAM *ram)
{
    quit();
}



int main(int argc, char **argv)
{
    if(argc > 1)
        strcpy(fname, argv[1]);
    else {
        strcpy(fname, "http://195.95.206.17/HitFM_32");
        //strcpy(fname, "http://urg.adamant.net:8080/radio-stilnoe48k");
    }

    /*char *end = strrchr(argv[0], '\\');
    memcpy(cwd, argv[0], end-argv[0]);
    cwd[end-argv[0]] = 0;*/

    initUsart();

    void InitConfig();
    InitConfig();

    csm_id = csmCreate("RadioSI", CoreCSM_GUI, onCreateCSM, onCloseCSM, 0);

    processEvents();
    return 0;
}



void *__aeabi_memcpy(void *d, const void *s, size_t n)
{
    return memcpy(d, s, n);
}

void *__aeabi_memset(void *d, int c, size_t n)
{
    return memset(d, c, n);
}

void *__aeabi_memmove(void *d, const void *s, size_t n)
{
    return memmove(d, s, n);
}



