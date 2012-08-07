
#include <swilib.h>
#include <nu_swilib.h>
#include "processbridge.h"


void MopiBridgeMessenger();


void bridgeInit()
{
    KillGBSproc(BridgeMOPI_ID);
    CreateGBSproc(BridgeMOPI_ID, "NU2MOPI_IOBRIDGE", MopiBridgeMessenger, 0x24, 0);
}


void bridgeFini()
{
    KillGBSproc(BridgeMOPI_ID);
}



/* arguments manipulation code */

void * pack_args(int argc, ...)
{
    void **data = (void **)malloc((argc+3) * sizeof(void *));
    data[0] = (void*)argc;

    va_list va;
    va_start(va, argc);

    argc++;
    for(int i=1; i < argc; ++i)
    {
        data[i] = va_arg(va, void *);
    }

    return data;
}


uint32_t unpack_args(void *_data, void ***_args)
{
    uint32_t *data = _data;
    unsigned int cnt = *data;

    if(!cnt) {
        *_args = 0;
        return 0;
    }

    ++data;
    *_args = (void*)data;
    return cnt;
}




/* Bridge of mopi and nucleus */
void MopiBridgeMessenger()
{
    GBS_MSG msg;
    if (GBS_RecActDstMessage(&msg))
    {
        if (msg.msg == MOPI_THREAD_PROC)
        {
            if(msg.submess)
            {
                ThreadInfo *inf = (ThreadInfo *)msg.data0;

                uint32_t pcnt;
                void **args, *pret = 0, *arg_ptr = msg.data1;
                void *(*ptr_func)(void *, ...) = (void *(*)(void *, ...))msg.submess;

                /* нету параметров */
                if(!arg_ptr)  {
                    ptr_func(0);
                    pcnt = 0;

                } else
                    pcnt = unpack_args(arg_ptr, &args);

                switch(pcnt)
                {
                    case 0:
                        break;

                    case 1:
                        pret = ptr_func(args[0]);
                        break;

                    case 2:
                        pret = ptr_func(args[0], args[1]);
                        break;

                    case 3:
                        pret = ptr_func(args[0], args[1], args[2]);
                        break;

                    case 4:
                        pret = ptr_func(args[0], args[1], args[2], args[3]);
                        break;

                    case 5:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4]);
                        break;

                    case 6:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5]);
                        break;

                    case 7:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6]);
                        break;

                    case 8:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6]);
                        break;

                    case 9:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6]);
                        break;

                    case 10:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6], args[7], args[8], args[9]);
                        break;

                    case 11:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6], args[7], args[8], args[9], args[10]);
                        break;

                    case 12:
                        pret = ptr_func(args[0], args[1], args[2], args[3], args[4], args[5],
                                        args[6], args[7], args[8], args[9], args[10], args[11]);
                        break;

                    default:
                        ShowMSG(1, (int)"Bridge invalid param!");
                        break;
                }

                if(inf->sync) {
                    inf->ret = pret;
                    inf->loked = 0;
                    NU_Release_Semaphore(&inf->loker);
                }
                else {
                    free(arg_ptr);
                    free(inf);
                }
            }
        }
        else
        {
            GBS_SendMessage(MMI_CEPID,MSG_HELPER_TRANSLATOR,msg.pid_from,msg.msg,msg.submess);
        }
    }
}



void *BridgeMessageSend(void *func_ptr, int type, void *packed_args)
{
    ThreadInfo *i = (ThreadInfo*) malloc(sizeof(ThreadInfo));
    i->sync = (type == NU_SYNCHRONIZED_PROC);
    i->loked = 1;
    i->ret = 0;


    if(i->sync && NU_Create_Semaphore(&i->loker, "mopi", 0, NU_PRIORITY) != NU_SUCCESS) {
        ShowMSG(1, (int)"ProcB: Semaphore init failed");
        free(i);
        return 0;
    }

    GBS_SendMessage(BridgeMOPI_ID, MOPI_THREAD_PROC, func_ptr, i, packed_args);

    switch(type)
    {
        case NU_SYNCHRONIZED_PROC:
            NU_Obtain_Semaphore(&i->loker, NU_SUSPEND);
            break;

        case NU_ASYNC_PROC:
            /* in async mode return not support */
            return 0;
    }

    if(i->sync)
        NU_Delete_Semaphore(&i->loker);

    /* возвращаемое значение */
    void *ret = i->ret;

    /* больше не нужно оно */
    free(i);

    /* чистим стек аргументов */
    free(packed_args);

    /* возвращаем результат выполнения */
    return ret;
}


#define SafeProcessRun(func, ret_type, run_type, args_cnt, ...)         \
    if(GBS_GetCurCepid() == -1) {                                   \
        return (ret_type)func(__VA_ARGS__);                             \
    }                                                                   \
    void *args = pack_args(args_cnt, __VA_ARGS__);                      \
    void *retrn = BridgeMessageSend((void *)func, run_type, args);\
    return (ret_type)retrn;

#ifndef UNUSED
#define UNUSED(x) ((void)x)
#endif

void _sync_ShowMSG(int type, int lang)
{
    SafeProcessRun(ShowMSG, void, NU_SYNCHRONIZED_PROC, 2, type, lang);
}



void _async_ShowMSG(int type, int lang)
{
    SafeProcessRun(ShowMSG, void, NU_ASYNC_PROC, 2, type, lang);
}


/* Files std */

int sync_open(const char * cFileName, unsigned int iFileFlags, unsigned int iFileMode, unsigned int *ErrorNumber)
{
    SafeProcessRun(_open, int, NU_SYNCHRONIZED_PROC, 4, cFileName, iFileFlags, iFileMode, ErrorNumber);
}


int sync_read(int FileHandler, void *cBuffer, int iByteCount, unsigned int *ErrorNumber)
{
    SafeProcessRun(_read, int, NU_SYNCHRONIZED_PROC, 4, FileHandler, cBuffer, iByteCount, ErrorNumber);
}


int sync_write(int FileHandler, void const * cBuffer, int iByteCount, unsigned int *ErrorNumber)
{
    SafeProcessRun(_write, int, NU_SYNCHRONIZED_PROC, 4, FileHandler, cBuffer, iByteCount, ErrorNumber);
}


int sync_close(int FileHandler, unsigned int *ErrorNumber)
{
    SafeProcessRun(_close, int, NU_SYNCHRONIZED_PROC, 2, FileHandler, ErrorNumber);
}


int sync_flush(int FileHandler, unsigned int *ErrorNumber)
{
    SafeProcessRun(_flush, int, NU_SYNCHRONIZED_PROC, 2, FileHandler, ErrorNumber);
}


int sync_truncate(int FileHandler, int length, unsigned int *errornumber)
{
    SafeProcessRun(_truncate, int, NU_SYNCHRONIZED_PROC, 3, FileHandler, length, errornumber);
}


int sync_unlink(const char *cFileName, unsigned int *errornumber)
{
    SafeProcessRun(_unlink, int, NU_SYNCHRONIZED_PROC, 2, cFileName, errornumber);
}


int sync_lseek(int FileHandler, unsigned int offset, unsigned int origin, unsigned int *ErrorNumber, unsigned int *ErrorNumber2)
{
    SafeProcessRun(_lseek, int, NU_SYNCHRONIZED_PROC, 5, FileHandler, offset, origin, ErrorNumber, ErrorNumber2);
}


/* Dirs */

int sync_mkdir(const char * cDirectory, unsigned int *ErrorNumber)
{
    SafeProcessRun(_mkdir, int, NU_SYNCHRONIZED_PROC, 2, cDirectory, ErrorNumber);
}


int sync_rmdir(const char * cDirectory, unsigned int *ErrorNumber)
{
    SafeProcessRun(_rmdir, int, NU_SYNCHRONIZED_PROC, 2, cDirectory, ErrorNumber);
}


int sync_isdir(const char * cDirectory, unsigned int *ErrorNumber)
{
    SafeProcessRun(isdir, int, NU_SYNCHRONIZED_PROC, 2, cDirectory, ErrorNumber);
}


/* Find files */

int sync_FindFirstFile(DIR_ENTRY *DIRENTRY, const char *mask, unsigned int *ErrorNumber)
{
    SafeProcessRun(FindFirstFile, int, NU_SYNCHRONIZED_PROC, 3, DIRENTRY, mask, ErrorNumber);
}

int sync_FindNextFile(DIR_ENTRY *DIRENTRY, unsigned int *ErrorNumber)
{
    SafeProcessRun(FindNextFile, int, NU_SYNCHRONIZED_PROC, 2, DIRENTRY, ErrorNumber);
}

int sync_FindClose(DIR_ENTRY *DIRENTRY, unsigned int *ErrorNumber)
{
    SafeProcessRun(FindClose, int, NU_SYNCHRONIZED_PROC, 2, DIRENTRY, ErrorNumber);
}



/* File Stats && attributes */

int sync_GetFileStats(const char *cFileName, FSTATS * StatBuffer, unsigned int *errornumber)
{
    SafeProcessRun(GetFileStats, int, NU_SYNCHRONIZED_PROC, 3, cFileName, StatBuffer, errornumber);
}


int sync_GetFileAttrib(const char *cFileName, unsigned char *cAttribute, unsigned int *ErrorNumber)
{
    SafeProcessRun(GetFileAttrib, int, NU_SYNCHRONIZED_PROC, 3, cFileName, cAttribute, ErrorNumber);
}


int sync_SetFileAttrib(const char *cFileName, unsigned char cAttribute, unsigned int *ErrorNumber)
{
    SafeProcessRun(SetFileAttrib, int, NU_SYNCHRONIZED_PROC, 3, cFileName, cAttribute, ErrorNumber);
}



IMGHDR *crtimgfrompng(const char *file, int type)
{
    return CreateIMGHDRFromPngFile(file, type);
}

IMGHDR *sync_CreateIMGHDRFromPngFile(const char *file, int type)
{
    SafeProcessRun(crtimgfrompng, IMGHDR *, NU_SYNCHRONIZED_PROC, 2, file, type);
}


int sync_setfilesize(int FileHandler, unsigned int iNewFileSize, unsigned int *ErrorNumber)
{
    SafeProcessRun(setfilesize, int, NU_SYNCHRONIZED_PROC, 3, FileHandler, iNewFileSize, ErrorNumber);
}


int sync_fmove(const char * SourceFileName, const char * DestFileName, unsigned int *ErrorNumber)
{
    SafeProcessRun(fmove, int, NU_SYNCHRONIZED_PROC, 3, SourceFileName, DestFileName, ErrorNumber);
}


Elf32_Exec* sync_elfopen(const char* filename)
{
    SafeProcessRun(elfopen, Elf32_Exec *, NU_SYNCHRONIZED_PROC, 1, filename);
}


int sync_ExecuteFile(WSHDR *file, WSHDR *mime, void *d)
{
    SafeProcessRun(ExecuteFile, int, NU_SYNCHRONIZED_PROC, 3, file, mime, d);
}




