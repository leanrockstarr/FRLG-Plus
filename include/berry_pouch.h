#ifndef GUARD_BERRY_POUCH_H
#define GUARD_BERRY_POUCH_H

#include "task.h"

enum BerryPouchType
{
    BERRYPOUCH_FROMFIELD,
    BERRYPOUCH_FROMPARTYGIVE,
    BERRYPOUCH_FROMMARTSELL,
    BERRYPOUCH_FROMPOKEMONSTORAGEPC,
    BERRYPOUCH_FROMBATTLE,
    BERRYPOUCH_FROMBERRYBLENDER,
    BERRYPOUCH_FROMBERRYCRUSH,
    BERRYPOUCH_NA
};
struct BerryPouchStruct_203F370
{
    void (*savedCallback)(void);
    u8 type;
    u8 allowSelect;
    u8 unused_06;
    u16 listMenuSelectedRow;
    u16 listMenuScrollOffset;
};

extern struct BerryPouchStruct_203F370 sStaticCnt;

void BerryPouch_StartFadeToExitCallback(u8 taskId);
void BerryPouch_SetExitCallback(void (*exitCallback)(void));
void InitBerryPouch(u8 type, void (*savedCallback)(void), u8 allowSelect);
void DisplayItemMessageInBerryPouch(u8 taskId, u8 fontId, const u8 * str, TaskFunc followUpFunc);
void Task_BerryPouch_DestroyDialogueWindowAndRefreshListMenu(u8 taskId);
void BerryPouch_CursorResetToTop(void);
bool32 PlayerHasBerries(void);

#endif //GUARD_BERRY_POUCH_H
