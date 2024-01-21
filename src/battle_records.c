#include "global.h"
#include "gflib.h"
#include "event_data.h"
#include "task.h"
#include "text_window.h"
#include "battle.h"
#include "trainer_tower.h"
#include "trainer_pokemon_sprites.h"
#include "scanline_effect.h"
#include "link.h"
#include "menu.h"
#include "overworld.h"
#include "strings.h"
#include "trainer_card.h"
#include "text.h"
#include "new_menu_helpers.h"
#include "constants/battle.h"
#include "constants/songs.h"
#include "constants/maps.h"

static EWRAM_DATA u16 * sBg3TilemapBuffer_p = NULL;

static void MainCB2_SetUp(void);
static void VBlankCB(void);
static void MainCB2(void);
static void Task_WaitFadeIn(u8 taskId);
static void Task_WaitButton(u8 taskId);
static void Task_FadeOut(u8 taskId);
static void Task_DestroyAndReturnToField(u8 taskId);
static void ClearWindowCommitAndRemove(u8 windowId);
static void ResetGpu(void);
static void StopAllRunningTasks(void);
static void EnableDisplay(void);
static void ResetBGPos(void);
static void PrintBattleRecords(void);
static void CommitWindow(u8 windowId);
static void LoadFrameGfxOnBg(u8 bgId);
void ShowBattleTowerRecords(void);
void RemoveRecordsWindow(void);

static const u16 sTiles[] = INCBIN_U16("graphics/battle_records/bg_tiles.4bpp");
static const u16 sPalette[] = INCBIN_U16("graphics/battle_records/bg_tiles.gbapal");
static const u16 sTilemap[] = INCBIN_U16("graphics/battle_records/bg_tiles.bin");

static const struct WindowTemplate sWindowTemplates[] = {
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 1,
        .width = 27,
        .height = 18,
        .paletteNum = 0xF,
        .baseBlock = 0x014
    }, DUMMY_WIN_TEMPLATE
};

static const u8 sTextColor[3] = {
    0, 2, 3
};

static const struct BgTemplate sBgTemplates[2] = {
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0, // 4bpp
        .priority = 0,
        .baseTile = 0x000
    }, {
        .bg = 3,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0, // 4bpp
        .priority = 3,
        .baseTile = 0x000
    }
};

static const struct WindowTemplate sFrontierResultsWindowTemplate =
{
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 0x1c,
    .height = 0x12,
    .paletteNum = 15,
    .baseBlock = 1
};

static u8 *const sStringVars[3] = {
    gStringVar1,
    gStringVar2,
    gStringVar3
};

void ShowBattleRecords(void)
{
    SetVBlankCallback(NULL);
    SetMainCallback2(MainCB2_SetUp);
}

static void MainCB2_SetUp(void)
{
    switch (gMain.state)
    {
    case 0:
        SetVBlankCallback(NULL);
        ResetGpu();
        gMain.state++;
        break;
    case 1:
        StopAllRunningTasks();
        gMain.state++;
        break;
    case 2:
        sBg3TilemapBuffer_p = AllocZeroed(0x800);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
        SetBgTilemapBuffer(3, sBg3TilemapBuffer_p);
        ResetBGPos();
        gMain.state++;
        break;
    case 3:
        LoadFrameGfxOnBg(3);
        LoadPalette(stdpal_get(0), 0xF0, 0x20);
        gMain.state++;
        break;
    case 4:
        if (IsDma3ManagerBusyWithBgCopy() != TRUE)
        {
            ShowBg(0);
            ShowBg(3);
            CopyBgTilemapBufferToVram(3);
            gMain.state++;
        }
        break;
    case 5:
        InitWindows(sWindowTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        EnableDisplay();
        SetVBlankCallback(VBlankCB);
        if (gSpecialVar_0x8004)
            PrintTrainerTowerRecords();
        else
            PrintBattleRecords();
        CreateTask(Task_WaitFadeIn, 8);
        SetMainCallback2(MainCB2);
        gMain.state = 0;
        break;
    }
}

static void VBlankCB(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void Task_WaitFadeIn(u8 taskId)
{
    if (!gPaletteFade.active)
        gTasks[taskId].func = Task_WaitButton;
}

static void Task_WaitButton(u8 taskId)
{
    struct Task *task = &gTasks[taskId];

    if (JOY_NEW(A_BUTTON) || JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        task->func = Task_FadeOut;
    }
}

static void Task_FadeOut(u8 taskId)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_DestroyAndReturnToField;
}

static void Task_DestroyAndReturnToField(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        Free(sBg3TilemapBuffer_p);
        ClearWindowCommitAndRemove(0);
        FreeAllWindowBuffers();
        DestroyTask(taskId);
    }
}

static void ClearWindowCommitAndRemove(u8 windowId)
{
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
    ClearWindowTilemap(windowId);
    CopyWindowToVram(windowId, COPYWIN_GFX);
    RemoveWindow(windowId);
}

static void ResetGpu(void)
{
    {
    void *dest = (void *)VRAM;
    u32 size = VRAM_SIZE;
    DmaClearLarge16(3, dest, size, 0x1000);
    }

    {
    void *dest = (void *)OAM;
    u32 size = OAM_SIZE;
    DmaClear32(3, dest, size);
    }

    {
    void *dest = (void *)PLTT;
    u32 size = PLTT_SIZE;
    DmaClear16(3, dest, size);
    }

    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
}

static void StopAllRunningTasks(void)
{
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetAllPicSprites();
    ResetPaletteFade();
    FreeAllSpritePalettes();
}

static void EnableDisplay(void)
{
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_BG3_ON);
}

static void ResetBGPos(void)
{
    ChangeBgX(0, 0, 0);
    ChangeBgY(0, 0, 0);
    ChangeBgX(1, 0, 0);
    ChangeBgY(1, 0, 0);
    ChangeBgX(2, 0, 0);
    ChangeBgY(2, 0, 0);
    ChangeBgX(3, 0, 0);
    ChangeBgY(3, 0, 0);
}

static void ClearLinkBattleRecord(struct LinkBattleRecord *record)
{
    CpuFill16(0, record, sizeof(*record));
    record->name[0] = EOS;
    record->trainerId = 0;
    record->wins = 0;
    record->losses = 0;
    record->draws = 0;
}

static void ClearLinkBattleRecords(struct LinkBattleRecords *records)
{
    s32 i;

    for (i = 0; i < LINK_B_RECORDS_COUNT; i++)
        ClearLinkBattleRecord(&records->entries[i]);
    SetGameStat(GAME_STAT_LINK_BATTLE_WINS, 0);
    SetGameStat(GAME_STAT_LINK_BATTLE_LOSSES, 0);
    SetGameStat(GAME_STAT_LINK_BATTLE_DRAWS, 0);
}

static s32 GetLinkBattleRecordTotalBattles(struct LinkBattleRecord * record)
{
    return record->wins + record->losses + record->draws;
}

static s32 IndexOfOpponentLinkBattleRecord(struct LinkBattleRecords * records, const u8 * name, u16 trainerId)
{
    s32 i;

    for (i = 0; i < LINK_B_RECORDS_COUNT; i++)
    {
        if (StringCompareN(records->entries[i].name, name, PLAYER_NAME_LENGTH) == 0 && records->entries[i].trainerId == trainerId)
            return i;
    }

    return LINK_B_RECORDS_COUNT;
}

static void SortLinkBattleRecords(struct LinkBattleRecords * records)
{
    struct LinkBattleRecord tmp;
    s32 i;
    s32 j;

    for (i = LINK_B_RECORDS_COUNT - 1; i > 0; i--)
    {
        for (j = i - 1; j >= 0; j--)
        {
            if (GetLinkBattleRecordTotalBattles(&records->entries[i]) > GetLinkBattleRecordTotalBattles(&records->entries[j]))
            {
                tmp = records->entries[i];
                records->entries[i] = records->entries[j];
                records->entries[j] = tmp;
            }
        }
    }
}

static void UpdateLinkBattleRecord(struct LinkBattleRecord * record, s32 outcome)
{
    switch (outcome)
    {
    case B_OUTCOME_WON:
        record->wins++;
        if (record->wins > 9999)
            record->wins = 9999;
        break;
    case B_OUTCOME_LOST:
        record->losses++;
        if (record->losses > 9999)
            record->losses = 9999;
        break;
    case B_OUTCOME_DREW:
        record->draws++;
        if (record->draws > 9999)
            record->draws = 9999;
        break;
    }
}

static void UpdateLinkBattleGameStats(s32 outcome)
{
    u8 statId;

    switch (outcome)
    {
    case B_OUTCOME_WON:
        statId = GAME_STAT_LINK_BATTLE_WINS;
        break;
    case B_OUTCOME_LOST:
        statId = GAME_STAT_LINK_BATTLE_LOSSES;
        break;
    case B_OUTCOME_DREW:
        statId = GAME_STAT_LINK_BATTLE_DRAWS;
        break;
    default:
        return;
    }

    if (GetGameStat(statId) < 9999)
        IncrementGameStat(statId);
}

static void AddOpponentLinkBattleRecord(struct LinkBattleRecords * records, const u8 * name, u16 trainerId, s32 outcome, u32 language)
{
    u8 namebuf[PLAYER_NAME_LENGTH + 1];
    s32 i;
    struct LinkBattleRecord * record;

    if (language == LANGUAGE_JAPANESE)
    {
        namebuf[0] = EXT_CTRL_CODE_BEGIN;
        namebuf[1] = EXT_CTRL_CODE_JPN;
        StringCopy(&namebuf[2], name);
    }
    else
        StringCopy(namebuf, name);
    UpdateLinkBattleGameStats(outcome);
    SortLinkBattleRecords(records);
    i = IndexOfOpponentLinkBattleRecord(records, namebuf, trainerId);
    if (i == LINK_B_RECORDS_COUNT)
    {
        i = LINK_B_RECORDS_COUNT - 1;
        record = &records->entries[LINK_B_RECORDS_COUNT - 1];
        ClearLinkBattleRecord(record);
        StringCopyN(record->name, namebuf, PLAYER_NAME_LENGTH);
        record->trainerId = trainerId;
    }
    UpdateLinkBattleRecord(&records->entries[i], outcome);
    SortLinkBattleRecords(records);
}

void ClearPlayerLinkBattleRecords(void)
{
    ClearLinkBattleRecords(&gSaveBlock2Ptr->linkBattleRecords);
}

static void IncTrainerCardWinCount(s32 battlerId)
{
    u16 *wins = &gTrainerCards[battlerId].rse.linkBattleWins;
    (*wins)++;
    if (*wins > 9999)
        *wins = 9999;
}

static void IncTrainerCardLossCount(s32 battlerId)
{
    u16 *losses = &gTrainerCards[battlerId].rse.linkBattleLosses;
    (*losses)++;
    if (*losses > 9999)
        *losses = 9999;
}

static void UpdateBattleOutcomeOnTrainerCards(s32 battlerId)
{
    switch (gBattleOutcome)
    {
    case B_OUTCOME_WON:
        IncTrainerCardWinCount(battlerId ^ 1);
        IncTrainerCardLossCount(battlerId);
        break;
    case B_OUTCOME_LOST:
        IncTrainerCardLossCount(battlerId ^ 1);
        IncTrainerCardWinCount(battlerId);
        break;
    }
}

void TryRecordLinkBattleOutcome(s32 battlerId)
{
    if (gSaveBlock1Ptr->location.mapGroup != MAP_GROUP(UNION_ROOM) || gSaveBlock1Ptr->location.mapNum != MAP_NUM(UNION_ROOM))
    {
        UpdateBattleOutcomeOnTrainerCards(battlerId);
        AddOpponentLinkBattleRecord(&gSaveBlock2Ptr->linkBattleRecords, gTrainerCards[battlerId].rse.playerName, gTrainerCards[battlerId].rse.trainerId, gBattleOutcome, gLinkPlayers[battlerId].language);
    }
}

static void PrintTotalRecord(struct LinkBattleRecords * records)
{
    u32 nwins = GetGameStat(GAME_STAT_LINK_BATTLE_WINS);
    u32 nlosses = GetGameStat(GAME_STAT_LINK_BATTLE_LOSSES);
    u32 ndraws = GetGameStat(GAME_STAT_LINK_BATTLE_DRAWS);
    s32 i;
    s32 j;
    bool32 foundEnd;
    u8 * strvar;

    if (nwins > 9999)
        nwins = 9999;
    if (nlosses > 9999)
        nlosses = 9999;
    if (ndraws > 9999)
        ndraws = 9999;

    ConvertIntToDecimalStringN(gStringVar1, nwins, STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar2, nlosses, STR_CONV_MODE_LEFT_ALIGN, 4);
    ConvertIntToDecimalStringN(gStringVar3, ndraws, STR_CONV_MODE_LEFT_ALIGN, 4);

    for (i = 0; i < NELEMS(sStringVars); i++)
    {
        strvar = sStringVars[i];
        foundEnd = FALSE;
        for (j = 0; j < 4; j++)
        {
            if (!foundEnd && *strvar == EOS)
                foundEnd = TRUE;
            if (foundEnd)
                *strvar = CHAR_SPACE;
            strvar++;
        }
        *strvar = 0xFF;
    }

    StringExpandPlaceholders(gStringVar4, gString_BattleRecords_TotalRecord);
    AddTextPrinterParameterized4(0, FONT_2, 12, 24, 0, 2, sTextColor, 0, gStringVar4);
}

static void PrintOpponentBattleRecord(struct LinkBattleRecord * record, u8 y)
{
    u8 i = 0;
    s32 x;

    if (record->wins == 0 && record->losses == 0 && record->draws == 0)
    {
        AddTextPrinterParameterized4(0, FONT_2, 0, y, 0, 2, sTextColor, 0, gString_BattleRecords_7Dashes);
        for (i = 0; i < 3; i++)
        {
            if (i == 0)
                x = 0x54;
            else if (i == 1)
                x = 0x84;
            else
                x = 0xB4;
            AddTextPrinterParameterized4(0, FONT_2, x, y, 0, 2, sTextColor, 0, gString_BattleRecords_4Dashes);
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            if (i == 0)
            {
                x = 0;
                StringFillWithTerminator(gStringVar1, PLAYER_NAME_LENGTH + 1);
                StringCopyN(gStringVar1, record->name, PLAYER_NAME_LENGTH);
            }
            else if (i == 1)
            {
                x = 0x54;
                ConvertIntToDecimalStringN(gStringVar1, record->wins, STR_CONV_MODE_RIGHT_ALIGN, 4);
            }
            else if (i == 2)
            {
                x = 0x84;
                ConvertIntToDecimalStringN(gStringVar1, record->losses, STR_CONV_MODE_RIGHT_ALIGN, 4);
            }
            else
            {
                x = 0xB4;
                ConvertIntToDecimalStringN(gStringVar1, record->draws, STR_CONV_MODE_RIGHT_ALIGN, 4);
            }
            AddTextPrinterParameterized4(0, FONT_2, x, y, 0, 2, sTextColor, 0, gStringVar1);
        }
    }
}

static void PrintBattleRecords(void)
{
    u32 left;
    s32 i;

    FillWindowPixelRect(0, PIXEL_FILL(0), 0, 0, 0xD8, 0x90);
    StringExpandPlaceholders(gStringVar4, gString_BattleRecords_PlayersBattleResults);
    left = 0xD0 - GetStringWidth(FONT_2, gStringVar4, -1);
    AddTextPrinterParameterized4(0, FONT_2, left / 2, 4, 0, 2, sTextColor, 0, gStringVar4);
    PrintTotalRecord(&gSaveBlock2Ptr->linkBattleRecords);
    AddTextPrinterParameterized4(0, FONT_2, 0x54, 0x30, 0, 2, sTextColor, 0, gString_BattleRecords_ColumnHeaders);
    for (i = 0; i < LINK_B_RECORDS_COUNT; i++)
        PrintOpponentBattleRecord(&gSaveBlock2Ptr->linkBattleRecords.entries[i], 0x3D + 14 * i);
    CommitWindow(0);
}

static void CommitWindow(u8 windowId)
{
    PutWindowTilemap(windowId);
    CopyWindowToVram(windowId, COPYWIN_FULL);
}

static void LoadFrameGfxOnBg(u8 bg)
{
    LoadBgTiles(bg, sTiles, 0xC0, 0);
    CopyToBgTilemapBufferRect(bg, sTilemap, 0, 0, 32, 32);
    LoadPalette(sPalette, 0, 0x20);
}

// Battle Tower Stuff

static void PrintHyphens(s32 y, u8 gRecordsWindowId)
{
    s32 i;
    u8 text[37];

    for (i = 0; i < 36; i++)
        text[i] = CHAR_HYPHEN;
    text[i] = EOS;

    y = (y * 8) + 1;
    AddTextPrinterParameterized(gRecordsWindowId, 1, text, 4, y, TEXT_SKIP_DRAW, NULL);
}

// Battle Tower records.
static bool32 sub_8110494(u8 level)
{
    switch (gSaveBlock2Ptr->battleTower.var_4AE[level])
    {
    case 0:
        return FALSE;
    case 1:
        return FALSE;
    case 2:
        return TRUE;
    case 4:
        return FALSE;
    case 3:
        return TRUE;
    case 5:
        return FALSE;
    case 6:
        return TRUE;
    default:
        return FALSE;
    }
}

static void TowerPrintStreak(const u8 *str, u16 num, u8 x1, u8 x2, u8 y, u8 gRecordsWindowId)
{
    AddTextPrinterParameterized(gRecordsWindowId, 1, str, x1, y, TEXT_SKIP_DRAW, NULL);
    if (num > 9999)
        num = 9999;
    ConvertIntToDecimalStringN(gStringVar1, num, STR_CONV_MODE_RIGHT_ALIGN, 4);
    StringExpandPlaceholders(gStringVar4, gOtherText_WinStreak);
    AddTextPrinterParameterized(gRecordsWindowId, 1, gStringVar4, x2, y, TEXT_SKIP_DRAW, NULL);
}

static void TowerPrintRecordStreak(u8 battleMode, u8 lvlMode, u8 x1, u8 x2, u8 y, u8 gRecordsWindowId)
{
    u16 num = gSaveBlock2Ptr->battleTower.recordWinStreaks[lvlMode];
    TowerPrintStreak(gOtherText_Record, num, x1, x2, y, gRecordsWindowId);
}

static u16 TowerGetWinStreak(u8 battleMode, u8 lvlMode)
{
    u16 winStreak = gSaveBlock2Ptr->battleTower.currentWinStreaks[lvlMode];
    if (winStreak > 9999)
        return 9999;
    else
        return winStreak;
}

static void TowerPrintPrevOrCurrentStreak(u8 battleMode, u8 lvlMode, u8 x1, u8 x2, u8 y, u8 gRecordsWindowId)
{
    bool8 isCurrent;
    u16 winStreak = TowerGetWinStreak(battleMode, lvlMode);
    if (lvlMode != 0)
        isCurrent = sub_8110494(lvlMode);
    else
        isCurrent = sub_8110494(lvlMode);
    if (isCurrent == TRUE)
        TowerPrintStreak(gOtherText_Current, winStreak, x1, x2, y, gRecordsWindowId);
    else
        TowerPrintStreak(gOtherText_Prev, winStreak, x1, x2, y, gRecordsWindowId);
}

static void PrintAligned(const u8 *str, s32 y, u8 gRecordsWindowId)
{
    s32 x = GetStringCenterAlignXOffset(1, str, 224);
    y = (y * 8) + 1;
    AddTextPrinterParameterized(gRecordsWindowId, 1, str, x, y, TEXT_SKIP_DRAW, NULL);
}

void ShowBattleTowerRecords(void)
{
    u8 battleMode = 0;
    u8 gRecordsWindowId;
    gRecordsWindowId = AddWindow(&sFrontierResultsWindowTemplate);
    DrawStdWindowFrame(gRecordsWindowId, FALSE);
    FillWindowPixelBuffer(gRecordsWindowId, PIXEL_FILL(1));
    StringExpandPlaceholders(gStringVar4, gOtherText_BattleTowerResults);

    PrintAligned(gStringVar4, 2, gRecordsWindowId);
    AddTextPrinterParameterized(gRecordsWindowId, 1, gText_Lv50, 8, 49, TEXT_SKIP_DRAW, NULL);
    AddTextPrinterParameterized(gRecordsWindowId, 1, gText_OpenLv, 8, 97, TEXT_SKIP_DRAW, NULL);
    PrintHyphens(10, gRecordsWindowId);
    TowerPrintPrevOrCurrentStreak(battleMode, 0, 72, 121, 49, gRecordsWindowId);
    TowerPrintRecordStreak(battleMode, 0, 72, 121, 65, gRecordsWindowId);
    TowerPrintPrevOrCurrentStreak(battleMode, 1, 72, 121, 97, gRecordsWindowId);
    TowerPrintRecordStreak(battleMode, 1, 72, 121, 113, gRecordsWindowId);
    PutWindowTilemap(gRecordsWindowId);
    CopyWindowToVram(gRecordsWindowId, 3);
    gSpecialVar_Result = gRecordsWindowId;
}

void RemoveRecordsWindow(void)
{
    ClearStdWindowAndFrame(gSpecialVar_Result, FALSE);
    RemoveWindow(gSpecialVar_Result);
}