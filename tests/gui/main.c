
#include <swilib.h>
#include <process.h>
#include <coreevent.h>
#include <csm.h>
#include <gui.h>
#include <lcd_primitives.h>


int csm_id, gui_id;
RECT canvas;
LCDLAYER *mmi_layer;


void onRedrawGUI(int id)
{
    printf("%s(%d)\n", __FUNCTION__, id);

    uint32_t red = 0xFF0000FF;
    lcd_draw_fillrect(mmi_layer, 0, 0, canvas.x2, canvas.y2, (char*)&red);

    displayLayer(mmi_layer);
}


void onCreateGUI(int id)
{
    printf("%s(%d)\n", __FUNCTION__, id);
    mmi_layer = *RamMMILCDLayer();
}


void onCloseGUI(int id)
{
    printf("%s(%d)\n", __FUNCTION__, id);
}


void onFocusGUI(int id)
{
    printf("%s(%d)\n", __FUNCTION__, id);
}


void onUnfocusGUI(int id)
{
    printf("%s(%d)\n", __FUNCTION__, id);
}


void onKeyGUI(int id, GUI_MSG *msg)
{
    printf("%s(%d)\n", __FUNCTION__, msg->gbsmsg->submess);
    char cc[128];
    ShowMSG(1, (int)"кн наж");
    sprintf(cc, "sub: %x", msg->gbsmsg->submess);
    ShowMSG(1, (int)cc);
    sprintf(cc, "msg: %x", msg->gbsmsg->msg);
    ShowMSG(1, (int)cc);

    if ((msg->gbsmsg->msg == KEY_DOWN || msg->gbsmsg->msg == LONG_PRESS))
    {
        switch(msg->gbsmsg->submess)
        {
        case RIGHT_SOFT:
            guiClose(id);
            return;
        }
    }
}



void onCreateCSM(CSM_RAM *ram)
{
    canvas.x = 0;
    canvas.y = 0;
    canvas.x2 = ScreenW();
    canvas.y2 = ScreenH();

    gui_id = guiCreate(&canvas, onRedrawGUI,
                       onCreateGUI,
                       onCloseGUI,
                       onFocusGUI,
                       onUnfocusGUI,
                       onKeyGUI,
                       NULL);

    printf("csm_id: %d | gui_id: %d\n", csm_id, gui_id);
    csmBindGUI(csm_id, gui_id);
}


void onCloseCSM(CSM_RAM *ram)
{
    quit();
}




int main()
{
    initUsart();
    printf(" [+] main: pid: %d\n", getpid());

    csm_id = csmCreate("test", CoreCSM_GUI, onCreateCSM, onCloseCSM, 0);

    processEvents();
    return 0;
}





