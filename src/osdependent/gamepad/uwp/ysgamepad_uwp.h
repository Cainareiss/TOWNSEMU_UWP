#ifndef YSGAMEPAD_UWP_IS_INCLUDED
#define YSGAMEPAD_UWP_IS_INCLUDED

#include "../ysgamepad.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Host -> emulator.  Any thread.  gamePadId 0..3.  Mapping the emulator expects (see fssimplewindow_connection.cpp):
   buttons[0]=A, buttons[1]=B, buttons[2]=RUN, buttons[3]=SELECT/PAUSE, dirs[0].upDownLeftRight = {up,down,left,right}. */
void YsGamePadUwpInject(int gamePadId,const struct YsGamePadReading *reading);

#ifdef __cplusplus
}
#endif

#endif
