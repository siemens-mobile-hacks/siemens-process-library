
#include "coreevent.h"
#include "process.h"
#include "csm.h"
#include "gui.h"



typedef struct {
    GUI gui;

    int id;
    int cg_id; // Id of csm, not of CoreCSM
    int used;
    int pid;
    int dt_id;

    void (*onRedraw)(int );
    void (*onCreate)(int );
    void (*onClose)(int );
    void (*onFocus)(int );
    void (*onUnfocus)(int );
    void (*onKey)(int, GUI_MSG *);
}CoreGUI;


typedef struct {
    CoreEvent head;
    void (*callback)(int, void *);
    CoreGUI *gui;
    GUI_MSG msg;
    GBS_MSG gbs_msg;

}CoreGUIEvent __attribute__((aligned(4)));



CoreGUI gui_list[128];



void guiInit()
{
    for(int i=0; i<128; ++i) {
        gui_list[i].cg_id = -1;
        gui_list[i].used = 0;
    }
}


static CoreGUI *getCoreGUIData(int id)
{
    if(id < 0 || id >= 128)
        return 0;

    return &gui_list[id];
}


static int find_best_id()
{
    for(int i=0; i<128; ++i)
        if(gui_list[i].cg_id < 0 && gui_list[i].used == 0)
            return i;
    return -1;
}





static void onRedraw(CoreGUI *data)
{
    if(!data->onRedraw)
        return;

    void core_gui_redraw_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_redraw_disp;
    event.callback = (void (*)(int, void *))data->onRedraw;
    event.gui = data;

    sendEvent(data->pid, &event, sizeof event);
}


static void onCreate(CoreGUI *data, void *(*malloc_adr)(int))
{
    data->gui.state=1;

    if(!data->onCreate)
        return;

    void core_gui_create_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_create_disp;
    event.callback = (void (*)(int, void *))data->onCreate;
    event.gui = data;

    sendEvent(data->pid, &event, sizeof event);
}


static void onClose(CoreGUI *data, void (*mfree_adr)(void *))
{
    data->gui.state=0;

    if(!data->onClose)
        return;

    void core_gui_close_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_close_disp;
    event.callback = (void (*)(int, void *))data->onClose;
    event.gui = data;

    sendEvent(data->pid, &event, sizeof event);
}


static void onFocus(CoreGUI *data, void *(*malloc_adr)(int), void (*mfree_adr)(void *))
{
    data->gui.state=2;

    if(!data->onFocus)
        return;

    void core_gui_focus_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_focus_disp;
    event.callback = (void (*)(int, void *))data->onFocus;
    event.gui = data;

    sendEvent(data->pid, &event, sizeof event);
}


static void onUnfocus(CoreGUI *data, void (*mfree_adr)(void *))
{
    if (data->gui.state!=2) return;
    data->gui.state=1;

    if(!data->onUnfocus)
        return;

    void core_gui_unfocus_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_unfocus_disp;
    event.callback = (void (*)(int, void *))data->onUnfocus;
    event.gui = data;

    sendEvent(data->pid, &event, sizeof event);
}


static int onKey(CoreGUI *data, GUI_MSG *msg)
{
    int id = msg->gbsmsg->submess;
    uint32_t command = (uint32_t)msg->gbsmsg->data0;

    if(id == data->id && command == 0xFFFFDEAD ) {
        return 1;
    }

    if(!data->onKey)
        return 0;

    void core_gui_key_disp(CoreGUIEvent *event)
    {
        event->callback(event->head.id, &event->msg);
    }

    CoreGUIEvent event;

    event.head.id = data->id;
    event.head.type = GUI_EVENT;
    event.head.dispatcher = (void (*)(void *))core_gui_key_disp;
    event.callback = (void (*)(int, void *))data->onKey;
    event.gui = data;
    memcpy(&event.msg, msg, sizeof *msg);
    memcpy(&event.gbs_msg, msg->gbsmsg, sizeof(GBS_MSG));
    event.msg.gbsmsg = &event.gbs_msg;

    sendEvent(data->pid, &event, sizeof event);
    return (0);
}


static void onDestroy(CoreGUI *data) {
    data->id = -1;
    data->pid = -1;
    data->cg_id = -1;
    data->dt_id = -1;
    data->used = 0;
}


static void method() {

}


const void * const coreGUIMethods[11]=
{
    (void *)onRedraw,
    (void *)onCreate,
    (void *)onClose,
    (void *)onFocus,
    (void *)onUnfocus,
    (void *)onKey,
    0,
    (void *)onDestroy,
    (void *)method,
    (void *)method,
    0
};



int guiCreate(RECT *canvas,
              void (*onRedraw)(int id),
              void (*onCreate)(int id),
              void (*onClose)(int id),
              void (*onFocus)(int id),
              void (*onUnfocus)(int id),
              void (*onKey)(int id, GUI_MSG *),
               void *userdata)
{
    int id;
    CoreGUI *data = getCoreGUIData((id = find_best_id()));
    if(!data)
        return id;

    memset(data, 0, sizeof *data);
    data->used = 1;

    data->pid = pid();
    data->gui.canvas = canvas;
    data->gui.methods = (void *)coreGUIMethods;
    data->gui.item_ll.data_mfree = (void (*)(void *))mfree_adr();
    data->onRedraw = onRedraw;
    data->onCreate = onCreate;
    data->onClose = onClose;
    data->onFocus = onFocus;
    data->onUnfocus = onUnfocus;
    data->onKey = onKey;
    data->id = id;

    data->cg_id = CreateGUI(data);
    if(data->cg_id < 0) {
        printf("Cant create gui\n");
        data->used = 0;
        data->id = -1;
        data->cg_id = -1;
    } else {
        data->dt_id = addProcessDtors(data->pid, (void (*)(void*, void*))guiClose, (void *)id, 0);
    }

    return data->id;
}


int guiClose(int id)
{
    CoreGUI *data = getCoreGUIData(id);
    if(!data)
        return -1;

    if(data->cg_id > -1)
        GBS_SendMessage(MMI_CEPID, KEY_DOWN, id, 0xFFFFDEAD);

    eraseProcessDtor(data->pid, data->dt_id);
    return 0;
}


int guiID(int id)
{
    CoreGUI *data = getCoreGUIData(id);
    if(!data)
        return -1;

    return data->cg_id;
}

