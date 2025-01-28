#include "lab.h"

#include <stddef.h>

#define LOW_ANALOG_TRIGGER_THRESHOLD 24

// Static Variables
static char nullString[] = " ";
static DIDraw didraws[6];
static GOBJ *infodisp_gobj_hmn;
static GOBJ *infodisp_gobj_cpu;
static RecData rec_data;
static Savestate *rec_state;
static _HSD_ImageDesc snap_image = {0};
static _HSD_ImageDesc resized_image = {
    .format = 4,
    .height = RESIZE_HEIGHT,
    .width = RESIZE_WIDTH,
};
static u8 snap_status;
static u8 export_status;
static Arch_LabData *stc_lab_data;
static char *tm_filename = "TMREC_%02d%02d%04d_%02d%02d%02d";
static char stc_save_name[32] = "Training Mode Input Recording   ";
static DevText *stc_devtext;
static u8 stc_hmn_controller;             // making this static so importing recording doesnt overwrite
static u8 stc_cpu_controller;             // making this static so importing recording doesnt overwrite
static u8 stc_null_controller;            // making this static so importing recording doesnt overwrite

// Aitch: not really a better way to do this that I can think of.
// Feel free to change if you find a way to implement playback takeover without a global.
static int stc_playback_cancelled_hmn = false;
static int stc_playback_cancelled_cpu = false;

static u8 stc_tdi_val_num;                // number of custom tdi values set
static CustomTDI stc_tdi_vals[TDI_HITNUM]; // contains the custom tdi values
static u8 stc_custom_osd_state_num = 0;
static int stc_custom_osd_states[OPTCUSTOMOSD_MAX_ADDED]; // contains the custom osd action states

// Static Export Variables
static RecordingSave *stc_rec_save;
static u32 stc_transfer_buf_size;
static u8 *stc_transfer_buf;
static MemcardSave memcard_save;
static int chunk_num;
static int save_pre_tick;
static char *slots_names[] = {"A", "B"};

// timer variables
static int lockout_timer = 0;
const int LOCKOUT_DURATION = 30;

static float cpu_locked_percent = 0;
static float hmn_locked_percent = 0;

// Menu Callbacks

void Lab_AddCustomOSD(GOBJ *menu_gobj) {
    int row = OPTCUSTOMOSD_FIRST_CUSTOM + stc_custom_osd_state_num;
    if (row == OPTCUSTOMOSD_MAX_COUNT) {
        SFX_PlayCommon(3);
        return;
    }

    char state_buf[64];
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    stc_custom_osd_states[stc_custom_osd_state_num++] = hmn_data->state_id;
    int found = GetCurrentStateName(hmn, state_buf);
    if (!found) strcpy(state_buf, "Unknown");

    char *row_text = HSD_MemAlloc(128);
    sprintf(row_text, "Remove OSD: %s", state_buf);

    LabOptions_CustomOSDs[row]= (EventOption) {
        .option_kind = OPTKIND_FUNC,
        .option_name = row_text,
        .desc = "Remove this Custom OSD.",
        .onOptionSelect = Lab_RemoveCustomOSD,
    };
}

void Lab_RemoveCustomOSD(GOBJ *menu_gobj) {
    int remove_idx = LabMenu_CustomOSDs.cursor;
    int osd_idx = remove_idx - OPTCUSTOMOSD_FIRST_CUSTOM;
    HSD_Free(LabOptions_CustomOSDs[remove_idx].option_name);
    int move_count = OPTCUSTOMOSD_MAX_COUNT - remove_idx - 1;
    memmove(&LabOptions_CustomOSDs[remove_idx], &LabOptions_CustomOSDs[remove_idx+1], move_count * sizeof(EventOption));
    memmove(&stc_custom_osd_states[osd_idx], &stc_custom_osd_states[osd_idx+1], move_count * sizeof(*stc_custom_osd_states));
    LabOptions_CustomOSDs[OPTCUSTOMOSD_MAX_COUNT-1] = (EventOption) { .option_name = 0, .disable = true };
    stc_custom_osd_state_num--;
    if (LabMenu_CustomOSDs.cursor - OPTCUSTOMOSD_FIRST_CUSTOM >= stc_custom_osd_state_num)
        LabMenu_CustomOSDs.cursor = OPTCUSTOMOSD_FIRST_CUSTOM + stc_custom_osd_state_num - 1;

    SFX_PlayCommon(0);
}

void Lab_CustomOSDsThink(void) {
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;

    for (int i = 0; i < stc_custom_osd_state_num; ++i) {
        int state_id = stc_custom_osd_states[i];
        EventOption *option = &LabOptions_CustomOSDs[i + OPTCUSTOMOSD_FIRST_CUSTOM];

        if (hmn_data->state_id == state_id) {
            char *state_buf[128];
            int found = GetCurrentStateName(hmn, state_buf);
            if (!found) strcpy(state_buf, "Unknown");

            ((*event_vars_ptr)->Message_Display)(
                15, hmn_data->ply, 0, 
                "%s:\n%i Frames", state_buf, hmn_data->TM.state_frame
            );

            break;
        }
    }
}

void Lab_ChangeStadiumTransformation(GOBJ *menu_gobj, int value) {
    // Transformation decision making is entirely located in one HUGE function (PokemonStadium_TransformationDecide),
    // so instead of just having a simple "SetTransformation(Fire)" function call,
    // we need to directly set the timers and overwrite the randomness call to set the transformation.
    // Incredibly hacky, but no better way I can think of.

    if (value == 0) return; // TODO

    GOBJ *gobj = Stage_GetMapGObj(2);

    u8 *tform_data = gobj->userdata;
    *(u32*)(tform_data + 0xD8) = -1; // set transformation timer

    s32 *timers = *TRANSFORMATION_TIMER_PTR;
    timers[2] = 0x1000001; // max transformation length
    timers[3] = 0x1000000; // default transformation length
    timers[4] = 0;         // screen flash length

    // We overwrite the HSD_Randi call in Event_Init before PokemonStadium_TransformationDecide is run
    // to prevent dolphin from jitting before we run the modified asm. It's overwritten to instead read
    // from the global address modified here.
    int transformation_id = value - 1;
    *TRANSFORMATION_ID_PTR = transformation_id;
}

void Lab_ChangeInputDisplay(GOBJ *menu_gobj, int value) {
    Memcard *memcard = R13_PTR(MEMCARD);
    memcard->TM_LabCPUInputDisplay = value;
}

void Lab_ChangeTauntEnabled(GOBJ *menu_gobj, int value) {
    Memcard *memcard = R13_PTR(MEMCARD);
    memcard->TM_LabTauntEnabled = value;
}

void Lab_ChangeOverlays(GOBJ *menu_gobj, int value) {
    Memcard *memcard = R13_PTR(MEMCARD);

    memset(&memcard->TM_LabSavedOverlays_HMN, 0, sizeof(memcard->TM_LabSavedOverlays_HMN));
    memset(&memcard->TM_LabSavedOverlays_CPU, 0, sizeof(memcard->TM_LabSavedOverlays_CPU));

    int overlay_save_count = sizeof(memcard->TM_LabSavedOverlays_HMN) / sizeof(OverlaySave);
    int overlay_save_idx_hmn = 0;
    int overlay_save_idx_cpu = 0;
    for (u8 group = 0; group < OVERLAY_COUNT; ++group) {
        u8 overlay_hmn = LabOptions_OverlaysHMN[group].option_val;
        u8 overlay_cpu = LabOptions_OverlaysCPU[group].option_val;

        if (overlay_hmn != 0 && overlay_save_idx_hmn < overlay_save_count) {
            memcard->TM_LabSavedOverlays_HMN[overlay_save_idx_hmn] = (OverlaySave) { group, overlay_hmn };
            overlay_save_idx_hmn += 1;
        }

        if (overlay_cpu != 0 && overlay_save_idx_cpu < overlay_save_count) {
            memcard->TM_LabSavedOverlays_CPU[overlay_save_idx_cpu] = (OverlaySave) { group, overlay_cpu };
            overlay_save_idx_cpu += 1;
        }
    }
}

void Lab_ChangeOSDs(GOBJ *menu_gobj, int value) {
    Memcard *memcard = R13_PTR(MEMCARD);
    int new_osds_value = memcard->TM_OSDEnabled;

    for (int i = 0; i < OPTOSD_MAX; i++) {
        int bit_value = osd_memory_bit_position[i];

        if (LabOptions_OSDs[i].option_val == 1) {
            // Set OSD bit to 1
            new_osds_value = new_osds_value | (1 << bit_value);
        } else {
            // Set OSD bit to 0
            new_osds_value = new_osds_value & ~(1 << bit_value);
        }
    }

    memcard->TM_OSDEnabled = new_osds_value;
    // Write OSD bitfield to backup address
    RTOC_INT(-0xDA8) = new_osds_value;
}

void Lab_ChangePlayerPercent(GOBJ *menu_gobj, int value)
{
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *fighter_data = fighter->userdata;

    fighter_data->dmg.percent = value;
    if (LabOptions_General[OPTGEN_HMNPCNTLOCK].option_val)
        hmn_locked_percent = fighter_data->dmg.percent;

    Fighter_SetHUDDamage(0, value);

    return;
}
void Lab_ChangePlayerLockPercent(GOBJ *menu_gobj, int value)
{
    GOBJ *fighter = Fighter_GetGObj(0);
    FighterData *fighter_data = fighter->userdata;

    if (value)
        hmn_locked_percent = fighter_data->dmg.percent;

    return;
}

void Lab_StartMoveCPU(GOBJ *menu_gobj) {
    LabOptions_CPU[OPTCPU_SET_POS] = LabOptions_CPU_FinishMoveCPU;

    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    FighterData *hmn_data = hmn->userdata;

    cpu_data->cpu.ai = 15;
    hmn_data->pad_index = stc_null_controller;
}

void Lab_FinishMoveCPU(GOBJ *menu_gobj) {
    LabOptions_CPU[OPTCPU_SET_POS] = LabOptions_CPU_MoveCPU;

    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    FighterData *hmn_data = hmn->userdata;

    hmn_data->pad_index = stc_hmn_controller;
}

void Lab_ChangeFrameAdvance(GOBJ *menu_gobj, int value)
{
    // remove colanim if toggling off
    if (value == 0)
        LabOptions_General[OPTGEN_FRAMEBTN].disable = 1;
    // apply colanim
    else
        LabOptions_General[OPTGEN_FRAMEBTN].disable = 0;

    return;
}

void Lab_ChangeFrameAdvanceButton(GOBJ *menu_gobj, int value) {
    Memcard *memcard = R13_PTR(MEMCARD);
    memcard->TM_LabFrameAdvanceButton = (u8)value;
}

void Lab_ChangeRandom(GOBJ *menu_gobj, int value)
{

    // remove colanim if toggling off
    if (value == 0)
        LabOptions_General[OPTGEN_FRAMEBTN].disable = 1;
    // apply colanim
    else
        LabOptions_General[OPTGEN_FRAMEBTN].disable = 0;

    return;
}
void Lab_ChangeCPUPercent(GOBJ *menu_gobj, int value)
{
    GOBJ *fighter = Fighter_GetGObj(1);
    FighterData *fighter_data = fighter->userdata;

    fighter_data->dmg.percent = value;
    Fighter_SetHUDDamage(1, value);

    if (LabOptions_CPU[OPTCPU_LOCKPCNT].option_val)
        cpu_locked_percent = fighter_data->dmg.percent;

    return;
}
void Lab_ChangeCPULockPercent(GOBJ *menu_gobj, int value)
{
        
    GOBJ *fighter = Fighter_GetGObj(1);
    FighterData *fighter_data = fighter->userdata;

    if (value)
        cpu_locked_percent = fighter_data->dmg.percent;

    return;
}
void Lab_ChangeTech(GOBJ *menu_gobj, int value)
{
    LabOptions_Tech[OPTTECH_TECHINPLACECHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_TECHAWAYCHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_TECHTOWARDCHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_MISSTECHCHANCE].disable = value == 0 ? 0 : 1;
}
void Lab_ChangeGetup(GOBJ *menu_gobj, int value)
{
    LabOptions_Tech[OPTTECH_GETUPSTANDCHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_GETUPAWAYCHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_GETUPTOWARDCHANCE].disable = value == 0 ? 0 : 1;
    LabOptions_Tech[OPTTECH_GETUPATTACKCHANCE].disable = value == 0 ? 0 : 1;
}

static int tech_option_menu_lookup[4] = {
    OPTTECH_TECHINPLACECHANCE,
    OPTTECH_TECHAWAYCHANCE,
    OPTTECH_TECHTOWARDCHANCE,
    OPTTECH_MISSTECHCHANCE,
};

static int getup_option_menu_lookup[4] = {
    OPTTECH_GETUPSTANDCHANCE,
    OPTTECH_GETUPAWAYCHANCE,
    OPTTECH_GETUPTOWARDCHANCE,
    OPTTECH_GETUPATTACKCHANCE,
};

static void rebound_tech_chances(EventOption tech_menu[4], int menu_lookup[], int slot_idx_changed) {
    u16 *chances[4];
    int enabled_slots = 0;

    for (int i = 0; i < 4; i++) {
        int tech_menu_idx = menu_lookup[i];
        chances[enabled_slots] = &tech_menu[tech_menu_idx].option_val;
        enabled_slots++;
    }

    rebound_chances(chances, 4, slot_idx_changed);
}

void Lab_ChangeTechInPlaceChance (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, tech_option_menu_lookup, 0); }
void Lab_ChangeTechAwayChance    (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, tech_option_menu_lookup, 1); }
void Lab_ChangeTechTowardChance  (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, tech_option_menu_lookup, 2); }
void Lab_ChangeMissTechChance    (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, tech_option_menu_lookup, 3); }

void Lab_ChangeStandChance       (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, getup_option_menu_lookup, 0); }
void Lab_ChangeRollAwayChance    (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, getup_option_menu_lookup, 1); }
void Lab_ChangeRollTowardChance  (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, getup_option_menu_lookup, 2); }
void Lab_ChangeGetupAttackChance (GOBJ *menu_gobj, int _new_val) { rebound_tech_chances(LabOptions_Tech, getup_option_menu_lookup, 3); }

static int enabled_slot_chances(EventOption slot_menu[REC_SLOTS], u16 *chances[REC_SLOTS]) {
    int enabled_slots = 0;
    for (int i = 0; i < REC_SLOTS; i++) {
        int slot_menu_idx = OPTSLOTCHANCE_1 + i;

        if (!slot_menu[slot_menu_idx].disable) {
            chances[enabled_slots] = &slot_menu[slot_menu_idx].option_val;
            enabled_slots++;
        }
    }

    return enabled_slots;
}

static void rebound_slot_chances(EventOption slot_menu[REC_SLOTS], int slot_idx_changed) {
    u16 *chances[REC_SLOTS];
    int chance_count = enabled_slot_chances(slot_menu, chances);
    rebound_chances(chances, chance_count, slot_idx_changed);
}

void Lab_ChangeSlot1ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 0); }
void Lab_ChangeSlot2ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 1); }
void Lab_ChangeSlot3ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 2); }
void Lab_ChangeSlot4ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 3); }
void Lab_ChangeSlot5ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 4); }
void Lab_ChangeSlot6ChanceHMN (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesHMN, 5); }

void Lab_ChangeSlot1ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 0); }
void Lab_ChangeSlot2ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 1); }
void Lab_ChangeSlot3ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 2); }
void Lab_ChangeSlot4ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 3); }
void Lab_ChangeSlot5ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 4); }
void Lab_ChangeSlot6ChanceCPU (GOBJ *menu_gobj, int _new_val) { rebound_slot_chances(LabOptions_SlotChancesCPU, 5); }

void Lab_ChangeCPUIntang(GOBJ *menu_gobj, int value)
{
    GOBJ *fighter = Fighter_GetGObj(1);
    FighterData *fighter_data = fighter->userdata;

    if (value == 0)
        Fighter_ColAnim_Remove(fighter_data, INTANG_COLANIM);
    else
        Fighter_ColAnim_Apply(fighter_data, INTANG_COLANIM, 0);

    return;
}

void Lab_ChangeModelDisplay(GOBJ *menu_gobj, int value)
{
    bool hide_chars = !LabValues_CharacterModelDisplay[value];

    // loop through all fighters
    GOBJ *this_fighter = (*stc_gobj_lookup)[MATCHPLINK_FIGHTER];
    while (this_fighter != 0)
    {
        // get data
        FighterData *thisFighterData = this_fighter->userdata;

        // toggle
        thisFighterData->show_model = !hide_chars;

        // get next fighter
        this_fighter = this_fighter->next;
    }

    bool hide_stage = !LabValues_StageModelDisplay[value];
    stc_matchcam->hide_stage = hide_stage;
    stc_matchcam->hide_stage2 = hide_stage;
    stc_matchcam->hide_background = hide_stage;

    bool hide_misc = hide_stage || hide_chars;

    stc_matchcam->hide_effects = hide_misc;

    return;
}
void Lab_ChangeHitDisplay(GOBJ *menu_gobj, int value)
{
    // loop through all fighters
    GOBJ *this_fighter = (*stc_gobj_lookup)[MATCHPLINK_FIGHTER];
    while (this_fighter != 0)
    {
        // get data
        FighterData *thisFighterData = this_fighter->userdata;

        // toggle
        thisFighterData->show_hit = value;

        // get next fighter
        this_fighter = this_fighter->next;
    }

    return;
}
void Lab_ChangeEnvCollDisplay(GOBJ *menu_gobj, int value)
{
    stc_matchcam->show_coll = value;
    return;
}
void Lab_ChangeItemGrabDisplay(GOBJ *menu_gobj, int value)
{
    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    FighterData *hmn_data = hmn->userdata;
    cpu_data->show_item_pickup = value;
    hmn_data->show_item_pickup = value;
}
void Lab_ChangeCamMode(GOBJ *menu_gobj, int value)
{
    MatchCamera *cam = MATCH_CAM;

    // normal cam
    if (value == 0)
    {
        Match_SetNormalCamera();
    }
    // zoom cam
    else if (value == 1)
    {
        Match_SetFreeCamera(0, 3);
        cam->freecam_fov.X = 140;
        cam->freecam_rotate.Y = 10;
    }
    // fixed
    else if (value == 2)
    {
        Match_SetFixedCamera();
    }
    else if (value == 3)
    {
        Match_SetDevelopCamera();
    }
    Match_CorrectCamera();

    return;
}

static void Lab_ChangeInfoPreset(EventOption options[], int preset_id)
{
    int *preset = LabValues_InfoPresetStates[preset_id];

    for (int i = 0; i < OPTINF_ROW_COUNT; ++i)
        options[OPTINF_ROW1 + i].option_val = preset[i];
}

void Lab_ChangeInfoPresetHMN(GOBJ *menu_gobj, int preset_id)
{
    Lab_ChangeInfoPreset(LabOptions_InfoDisplayHMN, preset_id);
}

void Lab_ChangeInfoPresetCPU(GOBJ *menu_gobj, int preset_id)
{
    Lab_ChangeInfoPreset(LabOptions_InfoDisplayCPU, preset_id);
}

void Lab_ChangeHUD(GOBJ *menu_gobj, int value)
{
    // toggle HUD
    u8 *hideHUD = (u8 *)(R13 + -0x4948);
    if (value == 0)
        *hideHUD = 1;
    else
        *hideHUD = 0;
}

void Lab_Exit(int value)
{
    Match *match = MATCH;

    // end game
    match->state = 3;

    // cleanup
    Match_EndVS();

    // Unfreeze
    LabOptions_General[OPTGEN_FRAME].option_val = 0;
    //HSD_Update *update = stc_hsd_update;
    return;
}

// Event Functions
GOBJ *InfoDisplay_Init(int ply)
{
    // Create Info Display GOBJ
    GOBJ *idGOBJ = GObj_Create(0, 0, 0);
    InfoDisplayData *idData = calloc(sizeof(InfoDisplayData));
    GObj_AddUserData(idGOBJ, 4, HSD_Free, idData);
    // Load jobj
    evMenu *menuAssets = event_vars->menu_assets;
    JOBJ *menu = JOBJ_LoadJoint(menuAssets->popup);
    idData->menuModel = menu;
    // Add to gobj
    GObj_AddObject(idGOBJ, 3, menu);
    // Add gxlink
    GObj_AddGXLink(idGOBJ, GXLink_Common, GXLINK_INFDISP, GXPRI_INFDISP);
    // Save pointers to corners
    JOBJ *corners[4];
    JOBJ_GetChild(menu, &corners, 2, 3, 4, 5, -1);
    idData->botLeftEdge = corners[0];
    idData->botRightEdge = corners[1];

    menu->trans.X = INFDISP_X;
    menu->trans.Y = INFDISP_Y;
    corners[0]->trans.X = 0;
    corners[1]->trans.X = INFDISP_WIDTH;
    corners[2]->trans.X = 0;
    corners[3]->trans.X = INFDISP_WIDTH;
    corners[0]->trans.Y = 0;
    corners[1]->trans.Y = 0;
    corners[2]->trans.Y = 0;
    corners[3]->trans.Y = 0;

    //JOBJ_SetFlags(menu, JOBJ_HIDDEN);
    menu->dobj->next->mobj->mat->alpha = 0.6;

    // Create text object
    int canvas_index = Text_CreateCanvas(2, idGOBJ, 14, 15, 0, GXLINK_INFDISPTEXT, GXPRI_INFDISPTEXT, 19);
    Text *text = Text_CreateText(2, canvas_index);
    text->kerning = 1;
    text->use_aspect = 1;
    text->aspect.X = 545;

    // Create subtexts for each row
    for (int i = 0; i < 8; i++)
    {
        Text_AddSubtext(text, 0, (INFDISPTEXT_YOFFSET * i), &nullString);
    }
    idData->text = text;

    // adjust size based on the console / settings
    //if ((OSGetConsoleType() == OS_CONSOLE_DEVHW3) || (stc_HSD_VI->is_prog == 1)) // 480p / dolphin uses medium by default
    //    LabOptions_InfoDisplay[OPT_SCALE].option_val = 1;
    //else // 480i on wii uses large (shitty composite!)
    //    LabOptions_InfoDisplay[OPT_SCALE].option_val = 2;

    // background scale

    Vec2 pos = {-26.5, 21.5};
    menu->trans.X = pos.X;
    menu->trans.Y = pos.Y;

    GXColor shield_color = (*stc_shieldcolors)[ply];
    shield_color.a = 255;
    GXColor *border_color = &menu->dobj->mobj->mat->diffuse;
    *border_color = shield_color;

    return idGOBJ;
}

// Returns length of string, including null terminator. Zero if not found.
static int GetCurrentStateName(GOBJ *fighter, char *buf) {
    FighterData *fighter_data = fighter->userdata;

    if (fighter_data->action_id == -1) return 0;

    FtAction *action = Fighter_GetFtAction(fighter_data, fighter_data->action_id);
    // extract state name from symbol
    char *symbol = action->anim_symbol;

    // extract from the opponent fighter actions if there is no symbol in the fighter action
    // (e.g. getting Yoshi's Neutral-B, Mewtwo's Side-B, Bowser's Side-B, etc.)
    if (symbol == NULL) {
        // loop through all humans
        for (int i = 0; i < 6; i++) {
            if (i == fighter_data->ply) { continue; }

            GOBJ *other_fighter = Fighter_GetGObj(i);
            if (other_fighter == 0) { continue; }

            FighterData *other_fighter_data = other_fighter->userdata;
            action = Fighter_GetFtAction(other_fighter_data, fighter_data->action_id);
            symbol = action->anim_symbol;
            if (symbol != NULL) { break; }
        }
    }

    if (symbol == NULL) return 0;

    // remove mangling
    int pos = 0;
    int posStart;
    int nameSize = 0;
    for (; pos < 50; pos++)
    {
        // search for "N_"
        if ((symbol[pos] == 'N') && (symbol[pos + 1] == '_'))
        {
            // posStart = beginning of state name
            pos++;
            posStart = pos + 1;
            break;
        }
    }

    // search for "_"
    for (pos = posStart+1; pos < 50; pos++)
    {
        if (symbol[pos] == '_')
        {
            nameSize = pos - posStart;
            break;
        }
    }

    memcpy(buf, &symbol[posStart], nameSize);
    buf[nameSize] = 0;

    return nameSize + 1;
}

void InfoDisplay_Update(GOBJ *menu_gobj, EventOption menu[], GOBJ *fighter, GOBJ *below)
{
    InfoDisplayData *idData = menu_gobj->userdata;
    Text *text = idData->text;
    if (Pause_CheckStatus(1) != 2)
    {
        // get the last row enabled
        int rowsEnabled = 8;
        while (rowsEnabled > 0)
        {
            if (menu[rowsEnabled - 1 + OPTINF_ROW1].option_val != 0)
                break;
            rowsEnabled--;
        }

        // scale window Y based on rows enabled
        JOBJ *leftCorner = idData->botLeftEdge;
        JOBJ *rightCorner = idData->botRightEdge;
        float yPos = (rowsEnabled * INFDISP_BOTYOFFSET) + INFDISP_BOTY;
        leftCorner->trans.Y = yPos;
        rightCorner->trans.Y = yPos;
        JOBJ_SetMtxDirtySub(idData->menuModel);

        // if a row is enabled, display
        if (rowsEnabled != 0)
        {
            // show model and text
            JOBJ_ClearFlags(idData->menuModel, JOBJ_HIDDEN);
            idData->text->hidden = 0;

            // update info display strings
            FighterData *fighter_data = fighter->userdata;

            for (int i = 0; i < 8; i++)
            {
                int value = menu[i + OPTINF_ROW1].option_val;

                // hide text if set to 0 or fighter DNE
                if (menu[i + OPTINF_ROW1].option_val == 0) {
                    Text_SetText(text, i, "");
                    continue;
                }

                // display info
                switch (value)
                {
                case (INFDISP_POS):
                {
                    Text_SetText(text, i, "Pos: (%+.3f , %+.3f)", fighter_data->phys.pos.X, fighter_data->phys.pos.Y);
                    break;
                }
                case (INFDISP_STATE):
                {
                    char buf[64];
                    int found = GetCurrentStateName(fighter, buf);
                    if (found) {
                        Text_SetText(text, i, "State: %s", buf);
                    } else {
                        Text_SetText(text, i, "State: Unknown");
                    }

                    break;
                }
                case (INFDISP_FRAME):
                {
                    Figatree *animStruct = fighter_data->figatree_curr;
                    int frameCurr = 0;
                    int frameTotal = 0;

                    // if exists
                    if (animStruct != 0)
                    {
                        // determine how many frames shield stun is
                        float animFrameTotal = animStruct->frame_num;
                        float animFrameCurr = fighter_data->state.frame;
                        float animSpeed = fighter_data->state.rate;
                        frameTotal = (animFrameTotal / animSpeed);
                        frameCurr = (animFrameCurr / animSpeed);
                    }

                    Text_SetText(text, i, "State Frame: %d/%d", frameCurr, frameTotal);
                    break;
                }
                case (INFDISP_SELFVEL):
                {
                    Text_SetText(text, i, "SelfVel: (%+.3f , %+.3f)", fighter_data->phys.self_vel.X, fighter_data->phys.self_vel.Y);
                    break;
                }
                case (INFDISP_KBVEL):
                {
                    Text_SetText(text, i, "KBVel: (%+.3f , %+.3f)", fighter_data->phys.kb_vel.X, fighter_data->phys.kb_vel.Y);
                    break;
                }
                case (INFDISP_TOTALVEL):
                {
                    Text_SetText(text, i, "TotalVel: (%+.3f , %+.3f)", fighter_data->phys.self_vel.X + fighter_data->phys.kb_vel.X, fighter_data->phys.self_vel.Y + fighter_data->phys.kb_vel.Y);
                    break;
                }
                case (INFDISP_ENGLSTICK):
                {
                    Text_SetText(text, i, "LStick:      (%+.4f , %+.4f)", fighter_data->input.lstick.X, fighter_data->input.lstick.Y);
                    break;
                }
                case (INFDISP_SYSLSTICK):
                {
                    HSD_Pad *pad = PadGet(fighter_data->ply, PADGET_MASTER);
                    Text_SetText(text, i, "LStick Sys: (%+.4f , %+.4f)", pad->fstickX, pad->fstickY);
                    break;
                }
                case (INFDISP_ENGCSTICK):
                {
                    Text_SetText(text, i, "CStick:     (%+.4f , %+.4f)", fighter_data->input.cstick.X, fighter_data->input.cstick.Y);
                    break;
                }
                case (INFDISP_SYSCSTICK):
                {
                    HSD_Pad *pad = PadGet(fighter_data->ply, PADGET_MASTER);
                    Text_SetText(text, i, "CStick Sys: (%+.4f , %+.4f)", pad->fsubstickX, pad->fsubstickY);
                    break;
                }
                case (INFDISP_ENGTRIGGER):
                {
                    Text_SetText(text, i, "Trigger:     (%+.3f)", fighter_data->input.trigger);
                    break;
                }
                case (INFDISP_SYSTRIGGER):
                {
                    HSD_Pad *pad = PadGet(fighter_data->ply, PADGET_MASTER);
                    Text_SetText(text, i, "Trigger Sys: (%+.3f , %+.3f)", pad->ftriggerLeft, pad->ftriggerRight);
                    break;
                }
                case (INFDISP_LEDGECOOLDOWN):
                {
                    Text_SetText(text, i, "Ledgegrab Timer: %d", fighter_data->ledge_cooldown);
                    break;
                }
                case (INFDISP_INTANGREMAIN):
                {
                    int intang = fighter_data->hurt.intang_frames.respawn;
                    if (fighter_data->hurt.intang_frames.ledge > fighter_data->hurt.intang_frames.respawn)
                        intang = fighter_data->hurt.intang_frames.ledge;

                    Text_SetText(text, i, "Intangibility Timer: %d", intang);
                    break;
                }
                case (INFDISP_HITSTOP):
                {
                    Text_SetText(text, i, "Hitlag: %.0f", fighter_data->dmg.hitlag_frames);
                    break;
                }
                case (INFDISP_HITSTUN):
                {
                    // get hitstun
                    float hitstun = 0;
                    if (fighter_data->flags.hitstun == 1)
                        hitstun = AS_FLOAT(fighter_data->state_var.state_var1);

                    Text_SetText(text, i, "Hitstun: %.0f", hitstun);
                    break;
                }
                case (INFDISP_SHIELDHEALTH):
                {
                    Text_SetText(text, i, "Shield Health: %.3f", fighter_data->shield.health);
                    break;
                }
                case (INFDISP_SHIELDSTUN):
                {
                    int stunTotal = 0;
                    int stunLeft = 0;

                    // check if taking shield stun
                    if (fighter_data->state_id == ASID_GUARDSETOFF)
                    {
                        // determine how many frames shield stun is
                        float frameTotal = JOBJ_GetJointAnimFrameTotal(fighter->hsd_object);
                        float frameCurr = fighter_data->state.frame;
                        float animSpeed = fighter_data->state.rate;
                        stunTotal = (frameTotal / animSpeed);
                        stunLeft = stunTotal - (frameCurr / animSpeed);
                        // 0 index
                        stunTotal++;
                        stunLeft++;
                    }

                    Text_SetText(text, i, "Shield Stun: %d/%d", stunLeft, stunTotal);
                    break;
                }
                case (INFDISP_GRIP):
                {
                    float grip = 0;
                    if (fighter_data->grab.victim != 0)
                    {
                        GOBJ *victim = fighter_data->grab.victim;
                        FighterData *victim_data = victim->userdata;
                        grip = victim_data->grab.grab_timer;
                    }

                    Text_SetText(text, i, "Grip Strength: %.0f", grip);
                    break;
                }
                case (INFDISP_ECBLOCK):
                {
                    Text_SetText(text, i, "ECB Lock: %d", fighter_data->coll_data.u.ecb_bot_lock_frames);
                    break;
                }
                case (INFDISP_ECBBOT):
                {
                    Text_SetText(text, i, "ECB Bottom: %.3f", fighter_data->coll_data.ecbCurr_bot.Y);
                    break;
                }
                case (INFDISP_JUMPS):
                {
                    Text_SetText(text, i, "Jumps: %d/%d", fighter_data->jump.jumps_used, fighter_data->attr.max_jumps);
                    break;
                }
                case (INFDISP_WALLJUMPS):
                {
                    Text_SetText(text, i, "Walljumps: %d", fighter_data->jump.walljumps_used);
                    break;
                }
                case (INFDISP_CANWALLJUMP):
                {
                    // no boolean specifier in c
                    const char *str;
                    if (can_walljump(fighter))
                        str = "Can Walljump: true";
                    else
                        str = "Can Walljump: false";
                    Text_SetText(text, i, str);
                    break;
                }
                case (INFDISP_JAB):
                {
                    Text_SetText(text, i, "Jab Counter: IDK");
                    break;
                }
                case (INFDISP_LINE):
                {
                    CollData *colldata = &fighter_data->coll_data;
                    int ground = -1;
                    int ceil = -1;
                    int left = -1;
                    int right = -1;

                    if ((colldata->envFlags & ECB_GROUND) != 0)
                        ground = colldata->ground_index;
                    if ((colldata->envFlags & ECB_CEIL) != 0)
                        ceil = colldata->ceil_index;
                    if ((colldata->envFlags & ECB_WALLLEFT) != 0)
                        left = colldata->leftwall_index;
                    if ((colldata->envFlags & ECB_WALLRIGHT) != 0)
                        right = colldata->rightwall_index;

                    Text_SetText(text, i, "Lines: G:%d, C:%d, L:%d, R:%d,", ground, ceil, left, right);
                    break;
                }
                case (INFDISP_BLASTLR):
                {
                    Stage *stage = STAGE;
                    Text_SetText(text, i, "Blastzone L/R: (%+.3f,%+.3f)", stage->blastzoneLeft, stage->blastzoneRight);
                    break;
                }
                case (INFDISP_BLASTUD):
                {
                    Stage *stage = STAGE;
                    Text_SetText(text, i, "Blastzone U/D: (%.2f,%.2f)", stage->blastzoneTop, stage->blastzoneBottom);
                    break;
                }
                }
            }

            if (below != NULL) {
                InfoDisplayData *belowData = below->userdata;
                float y_above = belowData->menuModel->trans.Y + belowData->botLeftEdge->trans.Y*belowData->menuModel->scale.X;
                float new_y = y_above + INFDISP_BOTYOFFSET*3;
                idData->menuModel->trans.Y = new_y;
            }

            float scale = LabValues_InfoSizes[menu[OPTINF_SIZE].option_val];
            idData->menuModel->scale.Y = scale * INFDISP_SCALE;
            idData->menuModel->scale.X = scale * INFDISP_SCALE;
            idData->text->viewport_scale.X = scale * INFDISPTEXT_SCALE;
            idData->text->viewport_scale.Y = scale * INFDISPTEXT_SCALE;
            idData->text->trans.X =  idData->menuModel->trans.X + scale * INFDISPTEXT_X;
            idData->text->trans.Y = -idData->menuModel->trans.Y + scale * INFDISPTEXT_Y;

            JOBJ_SetMtxDirtySub(idData->menuModel);
        }
        else
        {
            // hide model and text
            JOBJ_SetFlags(idData->menuModel, JOBJ_HIDDEN);
            idData->text->hidden = 1;
        }
    }
    else
    {
        // hide model and text
        JOBJ_SetFlags(idData->menuModel, JOBJ_HIDDEN);
        idData->text->hidden = 1;
    }
}
float Fighter_GetOpponentDir(FighterData *from, FighterData *to)
{
    float dir = -1;
    Vec3 *from_pos = &from->phys.pos;
    Vec3 *to_pos = &to->phys.pos;

    if (from_pos->X <= to_pos->X)
        dir = 1;

    return dir;
}

static int in_hitstun_anim(int state) {
    return ASID_DAMAGEHI1 <= state && state <= ASID_DAMAGEFLYROLL;
}

static int hitstun_ended(GOBJ *character) {
    FighterData *data = character->userdata;
    float hitstun = *((float*)&data->state_var.state_var1);
    return hitstun == 0.0;
}

static int in_tumble_anim(int state) {
    return state == ASID_DAMAGEFALL || (ASID_DAMAGEFLYHI <= state && state <= ASID_DAMAGEFLYROLL);
}

static int check_IASA(FighterData *data) {
    // For some reason, the iasa flag isn't reset when the state changes, only when a new action that has iasa starts.
    // So we need to do some bs tracking to figure out if the character is in iasa.
    return data->TM.iasa_frames && data->TM.iasa_frames <= data->TM.state_frame;
}

static int CheckOverlay(GOBJ *character, OverlayGroup overlay)
{
    FighterData *data = character->userdata;
    int state = data->state_id;
    int in_air = data->phys.air_state;
    int kind = data->kind;

    switch (overlay)
    {
        case (OVERLAY_ACTIONABLE):
        {
            if (in_hitstun_anim(state) && hitstun_ended(character))
                return true;

            if (state == ASID_LANDING && data->state.frame >= data->attr.normal_landing_lag)
                return true;

            if (check_IASA(data)) return true;

            if (state == ASID_CLIFFWAIT && data->TM.state_frame > 1) return true;

            if (ASID_WAIT <= state && state <= ASID_WALKFAST) return true;
            if (ASID_GUARD <= state && state <= ASID_GUARDREFLECT) return true;
            if (ASID_JUMPF <= state && state <= ASID_FALLAERIALB) return true;

            return state == ASID_TURN
                || state == ASID_PASS
                || state == ASID_SQUATWAIT
                || state == ASID_OTTOTTOWAIT;
        }

        case (OVERLAY_HITSTUN):
            return in_hitstun_anim(state) && !hitstun_ended(character);

        case (OVERLAY_LEDGE_ACTIONABLE):
            return state == ASID_CLIFFWAIT && data->TM.state_frame > 1;

        case (OVERLAY_MISSED_LCANCEL):
        {
            if (state < ASID_LANDINGAIRN || ASID_LANDINGAIRLW < state)
                return false;

            int frames_from_first_possible_l = data->TM.state_frame + 7;
            return data->input.timer_trigger_any_ignore_hitlag >= frames_from_first_possible_l;
        }

        case (OVERLAY_CAN_FASTFALL):
        {
            if (!in_air)
                return false;

            if (in_hitstun_anim(state) && !hitstun_ended(character))
                return false;

            if ((state < ASID_JUMPF || state > ASID_DAMAGEFALL)
                    && (state < ASID_DAMAGEHI1 || state > ASID_DAMAGEFLYROLL)
                    && (state < ASID_ATTACKAIRN || state > ASID_ATTACKAIRLW)
                    && state != ASID_PASS)
                return false;

            // peach, yoshi, ness, mewtwo cannot fastfall in dj animation for whatever reason.
            if ((kind == FTKIND_PEACH || kind == FTKIND_YOSHI || kind == FTKIND_NESS || kind == FTKIND_MEWTWO)
                    && (state == ASID_JUMPAERIALF || state == ASID_JUMPAERIALB))
                return false;

            if (data->flags.is_fastfall) 
                return false;

            // We can only fastfall on second frame with negative velocity.
            // This check prunes the first frame.
            if (data->phys.pos_delta.Y >= 0.0)
                return false;

            return data->phys.self_vel.Y < 0.0f;
        }

        case (OVERLAY_INVINCIBLE):
        {
            return data->hurt.kind_script != 0 || data->hurt.kind_game != 0;
        }

        case (OVERLAY_AUTOCANCEL):
        {
            if (state < ASID_ATTACKAIRN || ASID_ATTACKAIRLW < state)
                return false;

            return data->ftcmd_var.flag0 == 0;
        }

        case (OVERLAY_CROUCH):
            return state >= ASID_SQUAT && state <= ASID_SQUATRV;

        case (OVERLAY_WAIT):
            return check_IASA(data) || state == ASID_WAIT;

        case (OVERLAY_WALK):
            return state == ASID_WALKSLOW
                || state == ASID_WALKMIDDLE
                || state == ASID_WALKFAST;

        case (OVERLAY_DASH): return state == ASID_DASH;
        case (OVERLAY_RUN): return state == ASID_RUN;

        case (OVERLAY_JUMPS_USED):
            return data->jump.jumps_used > 1;

        case (OVERLAY_FULLHOP):
        {
            if (state != ASID_JUMPF && state != ASID_JUMPB)
                return false;

            return data->state_var.state_var1 == 0;
        }

        case (OVERLAY_SHORTHOP):
        {
            if (state != ASID_JUMPF && state != ASID_JUMPB)
                return false;

            return data->state_var.state_var1 == 1;
        }

        case (OVERLAY_IASA):
            return check_IASA(data);
    }

    char * err = HSD_MemAlloc(64);
    sprintf(err, "unhandled overlay idx %i", overlay);
    assert(err);
    return false;
}

// returns true if the cpu should counter in this asid state
int CPUAction_CheckASID(GOBJ *cpu, int asid_kind)
{
    FighterData *cpu_data = cpu->userdata;
    int cpu_state = cpu_data->state_id;
    int in_air = cpu_data->phys.air_state;

    switch (asid_kind)
    {
        // check custom ASID groups
        case (ASID_ANY): return true;
        case (ASID_DAMAGEAIR):
            return in_air
                && (in_hitstun_anim(cpu_state) || cpu_state == ASID_DAMAGEFALL)
                && hitstun_ended(cpu);

        case (ASID_ACTIONABLE):
        {
            if (in_air)
                goto ACTIONABLEAIR;
            else
                goto ACTIONABLEGROUND;
        }

        case (ASID_ACTIONABLEAIR):
        ACTIONABLEAIR:
        {
            if (!in_air)
                return false;

            if (in_hitstun_anim(cpu_state) && hitstun_ended(cpu))
                return true;

            return cpu_state == ASID_JUMPF 
                || cpu_state == ASID_JUMPB
                || cpu_state == ASID_JUMPAERIALF
                || cpu_state == ASID_JUMPAERIALB
                || cpu_state == ASID_FALL
                || cpu_state == ASID_FALLAERIALF
                || cpu_state == ASID_FALLAERIALB
                || cpu_state == ASID_DAMAGEFALL
                || cpu_state == ASID_DAMAGEFLYROLL
                || cpu_state == ASID_DAMAGEFLYTOP;
        }

        case (ASID_ACTIONABLEGROUND):
        ACTIONABLEGROUND:
        {
            if (in_air)
                return false;

            if (in_hitstun_anim(cpu_state) && hitstun_ended(cpu))
                return true;

            if (cpu_state == ASID_LANDING && cpu_data->state.frame >= cpu_data->attr.normal_landing_lag)
                return true;

            return cpu_state == ASID_WAIT
                || cpu_state == ASID_WALKSLOW
                || cpu_state == ASID_WALKMIDDLE
                || cpu_state == ASID_WALKFAST
                || cpu_state == ASID_RUN
                || cpu_state == ASID_SQUATWAIT
                || cpu_state == ASID_OTTOTTOWAIT
                || cpu_state == ASID_GUARD;
        }

        // check normal action states
        default: return cpu_state == asid_kind;
    }
}

int IsThrown(GOBJ *fighter) {
    FighterData *data = fighter->userdata;

    int state = data->state_id;

    GOBJ *attacker = data->grab.attacker;
    if (attacker) {
        FighterData *attacker_data = attacker->userdata;
        int attacker_kind = attacker_data->kind;

        // hardcode DK cargo throw
        if (attacker_kind == FTKIND_DK && (state == ASID_THROWNF || state == ASID_THROWNFF)) return 0;
    }

    return (ASID_THROWNF <= state && state <= ASID_THROWNLW)
        || (ASID_THROWNFF <= state && state <= ASID_THROWNFLW)
        || state == ASID_CAPTURECAPTAIN
        || state == ASID_THROWNKOOPAF
        || state == ASID_THROWNKOOPAB
        || state == ASID_THROWNKOOPAAIRF
        || state == ASID_THROWNKOOPAAIRB;
}

int CPU_IsGrabbed(GOBJ *cpu, GOBJ *hmn)
{
    FighterData *cpu_data = cpu->userdata;
    FighterData *hmn_data = hmn->userdata;

    int cpu_state = cpu_data->state_id;
    int hmn_kind = hmn_data->kind;

    return (ASID_CAPTUREPULLEDHI <= cpu_state && cpu_state <= ASID_CAPTURECUT)
        || cpu_state == ASID_CAPTUREWAITKOOPA
        || cpu_state == ASID_CAPTUREWAITKOOPAAIR
        || cpu_state == ASID_CAPTUREWAITKIRBY
        || cpu_state == ASID_DAMAGEICE
        || cpu_state == ASID_CAPTUREMASTERHAND
        || cpu_state == ASID_YOSHIEGG
        || cpu_state == ASID_CAPTUREKIRBYYOSHI
        || cpu_state == ASID_KIRBYYOSHIEGG
        || cpu_state == ASID_CAPTURELEADEAD
        || cpu_state == ASID_CAPTURELIKELIKE
        || cpu_state == ASID_CAPTUREWAITCRAZYHAND
        // hardcode DK cargo throw
        || (ASID_SHOULDEREDWAIT <= cpu_state && cpu_state <= ASID_SHOULDEREDTURN)
        || (hmn_kind == 3 && (cpu_state == ASID_THROWNF || cpu_state == ASID_THROWNFF));

    return 0;
}

int Lab_CPUPerformAction(GOBJ *cpu, int action_id, GOBJ *hmn)
{
    FighterData *cpu_data = cpu->userdata;
    FighterData *hmn_data = hmn->userdata;

    // get CPU action
    int action_done = 0;
    CPUAction *action_list = Lab_CPUActions[action_id];

    int recSlot = action_list[0].recSlot;
    if (recSlot != 0) {
        LabData *eventData = event_vars->event_gobj->userdata;
        int frame = eventData->counter_slot_frame;
        if (frame == 0 && !CPUAction_CheckASID(cpu, ASID_ACTIONABLE)) return false; // wait until actionable
        RecInputData *data = rec_data.cpu_inputs[recSlot-1];

        if (data->start_frame < 0) return true;
        if (frame >= data->num) {
            eventData->counter_slot_frame = 0;
            return true;
        }

        // cpu inputs have a different range than normal inputs, so we need to convert them.
        // This will be slightly lossy, but there's not much we can do.
        RecInputs *inputs = &data->inputs[frame];
        cpu_data->cpu.lstickX = (s8)((float)inputs->stickX * 1.5875f);
        cpu_data->cpu.lstickY = (s8)((float)inputs->stickY * 1.5875f);
        cpu_data->cpu.cstickX = (s8)((float)inputs->substickX * 1.5875f);
        cpu_data->cpu.cstickY = (s8)((float)inputs->substickY * 1.5875f);
        cpu_data->cpu.ltrigger = (u8)((float)inputs->trigger / 140.f * 255.f);
        cpu_data->cpu.held = Record_RearrangeButtons(inputs);
        eventData->counter_slot_frame = frame + 1;
        return false;
    }

    int cpu_state = cpu_data->state_id;
    s16 cpu_frame = cpu_data->state.frame;
    if (cpu_frame == -1)
        cpu_frame = 0;

    // clear inputs
    Fighter_ZeroCPUInputs(cpu_data);

    if (action_list == 0) return 1;

    // perform command
    // loop through all inputs
    for (int action_i = 0; ; action_i++)
    {
        CPUAction *action_input = &action_list[action_i];
        if (action_input->state == 0xFFFF)
            break;

        if (!CPUAction_CheckASID(cpu, action_input->state))
            continue;

        if (action_input->custom_check != 0 && !action_input->custom_check(cpu))
            continue;

        // check if im on the right frame
        if (cpu_frame < action_input->frameLow)
            continue;

        // perform this action
        int held = action_input->input;
        s8 lstickX = action_input->stickX;
        s8 lstickY = action_input->stickY;
        s8 cstickX = action_input->cstickX;
        s8 cstickY = action_input->cstickY;

        // stick direction
        switch (action_input->stickDir)
        {
        case (STCKDIR_NONE):
        {
            break;
        }
        case (STCKDIR_TOWARD):
        {
            s8 dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
            lstickX *= dir;
            cstickX *= dir;
            break;
        }
        case (STCKDIR_AWAY):
        {
            s8 dir = Fighter_GetOpponentDir(cpu_data, hmn_data) * -1;
            lstickX *= dir;
            cstickX *= dir;
            break;
        }
        case (STCKDIR_FRONT):
        {
            s8 dir = cpu_data->facing_direction;
            lstickX *= dir;
            cstickX *= dir;
            break;
        }
        case (STCKDIR_BACK):
        {
            s8 dir = cpu_data->facing_direction;
            lstickX *= (dir * -1);
            cstickX *= (dir * -1);
            break;
        }
        case (STICKDIR_RDM):
        {
            // random direction
            s8 dir = HSD_Randi(2) == 0 ? 1 : -1;
            lstickX *= dir;
            cstickX *= dir;
            break;
        }
        }

        // perform this action
        cpu_data->cpu.held = held;
        cpu_data->cpu.lstickX = lstickX;
        cpu_data->cpu.lstickY = lstickY;
        cpu_data->cpu.cstickX = cstickX;
        cpu_data->cpu.cstickY = cstickY;

        // check if this was the last action
        if (action_input->isLast == 1)
            action_done = 1;

        break;
    }

    return action_done;
}

void CPUThink(GOBJ *event, GOBJ *hmn, GOBJ *cpu)
{
    // get gobjs data
    LabData *eventData = event->userdata;
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;
    GOBJ **gobj_lookup = (*stc_gobj_lookup);
    int cpu_state = cpu_data->state_id;
    eventData->cpu_countering = false;

    // noact
    cpu_data->cpu.ai = 15;

    int last_state = cpu_data->TM.state_prev[0];
    bool is_cpu_no_longer_in_captur_cut = last_state == ASID_CAPTURECUT && cpu_state != ASID_CAPTURECUT;
    bool is_cpu_no_longer_in_captur_jump = last_state == ASID_CAPTUREJUMP && cpu_state != ASID_CAPTUREJUMP;

    if((is_cpu_no_longer_in_captur_cut || is_cpu_no_longer_in_captur_jump) && cpu_data->TM.state_frame == 1)
    {
        eventData->cpu_state = CPUSTATE_COUNTER;
        goto CPULOGIC_COUNTER;
    }

    // if first throw frame, advance hitnum
    int is_thrown = IsThrown(cpu);
    if (is_thrown == 1 && eventData->cpu_isthrown == 0)
        eventData->cpu_hitnum++;
    eventData->cpu_isthrown = is_thrown;

    // ALWAYS CHECK FOR X AND OVERRIDE STATE

    // Simulate tech lockouts
    if (ASID_PASSIVE <= cpu_state && cpu_state <= ASID_PASSIVESTANDB && cpu_data->TM.state_frame == 1) {
        int lockout;
        int lockout_type = LabOptions_Tech[OPTTECH_LOCKOUT].option_val;

        if (lockout_type == TECHLOCKOUT_EARLIEST) {
            // If we have passed the lockout window (cpu_tech_lockout <= 0),
            // We can say the CPU could have teched right after the lockout ended.
            lockout = 40 + eventData->cpu_tech_lockout;
            if (lockout < 20) lockout = 20;
            if (lockout > 40) lockout = 40;
        } else {
            lockout = 40;
        }

        eventData->cpu_tech_lockout = lockout;
    }

    // check if damaged
    if (cpu_data->flags.hitstun == 1)
    {
        eventData->cpu_hitkind = HITKIND_DAMAGE;
        // go to SDI state
        eventData->cpu_state = CPUSTATE_SDI;
        Fighter_ZeroCPUInputs(cpu_data);
    }
    // check if being held in a grab
    if (CPU_IsGrabbed(cpu, hmn) == 1)
    {
        eventData->cpu_state = CPUSTATE_GRABBED;
    }
    // check if being thrown
    if (is_thrown == 1)
    {
        eventData->cpu_state = CPUSTATE_TDI;
    }
    // check for shield hit
    if ((cpu_state == ASID_GUARDSETOFF) || ((cpu_data->kind == 0xE) && (cpu_state == 344)))
    {
        Fighter_ZeroCPUInputs(cpu_data);

        // check if new shield hit
        if (eventData->cpu_lastshieldstun != cpu_data->atk_instance)
        {
            eventData->cpu_lastshieldstun = cpu_data->atk_instance;
            eventData->cpu_hitshieldnum++;
        }

        // Keep holding shield during hitstop/hitlag. Prevents nana from dropping shield.
        cpu_data->cpu.held |= PAD_TRIGGER_R;
        cpu_data->input.held |= PAD_TRIGGER_R;
        cpu_data->input.trigger = 1.0f;
        cpu_data->cpu.ai = 15; // Ensure AI doesn't override this input

        eventData->cpu_hitkind = HITKIND_SHIELD;
        eventData->cpu_state = CPUSTATE_COUNTER;
    }
    // check for missed tech
    if ((cpu_state == ASID_DOWNBOUNDD) || (cpu_state == ASID_DOWNBOUNDU) || (cpu_state == ASID_DOWNWAITU) || (cpu_state == ASID_DOWNWAITD) || (cpu_state == ASID_PASSIVE) || (cpu_state == ASID_PASSIVESTANDB) || (cpu_state == ASID_PASSIVESTANDF))
        eventData->cpu_state = CPUSTATE_GETUP;
    // check for cliffgrab
    if ((cpu_state == ASID_CLIFFWAIT))
        eventData->cpu_state = CPUSTATE_RECOVER;
    // check if dead
    if (cpu_data->flags.dead == 1)
        goto CPUSTATE_ENTERSTART;

    // run CPU state logic
    switch (eventData->cpu_state)
    {
    // Initial State, hasn't been hit yet
    case (CPUSTATE_START):
    CPULOGIC_START:
    {
        // if in the air somehow, enter recovery
        if (cpu_data->phys.air_state == 1)
        {
            eventData->cpu_state = CPUSTATE_RECOVER;
            goto CPULOGIC_RECOVER;
        }

        // clear held inputs
        Fighter_ZeroCPUInputs(cpu_data);

        // perform default behavior
        int behavior = LabOptions_CPU[OPTCPU_BEHAVE].option_val;
        switch (behavior)
        {
        case (CPUBEHAVE_STAND):
        {
            break;
        }

        case (CPUBEHAVE_SHIELD):
        {
            // hold R
            cpu_data->cpu.held |= PAD_TRIGGER_R;
            cpu_data->input.held |= PAD_TRIGGER_R;
            cpu_data->input.trigger = 1.0f;
            cpu_data->cpu.ai = 15; // Ensure AI doesn't override this input

            // input shield angle if appropriate
            u16 shield_ang = LabOptions_CPU[OPTCPU_SHIELDDIR].option_val;
            // if you do not check action state before inputting a shield angle, it can cause slight
            // bugs with OOS options, like trying to input shield angle while doing a grounded up-b OOS.
            if (shield_ang != CPUSHIELDANG_NONE && (CPUAction_CheckASID(cpu, ASID_ACTIONABLEGROUND) || CPUAction_CheckASID(cpu, ASID_GUARD))) {
                s8 stickX = 0;
                s8 stickY = 0;
                if (shield_ang == CPUSHIELDANG_TOWARD || shield_ang == CPUSHIELDANG_AWAY) {
                    int dir = (int)Fighter_GetOpponentDir(cpu_data, hmn_data);
                    stickX = 127 * (dir * (shield_ang == CPUSHIELDANG_AWAY ? -1 : 1));
                } else {
                    stickY = 127 * (shield_ang == CPUSHIELDANG_DOWN ? -1 : 1);
                }
                // overwrite the last-frame stick inputs, so 
                // rolls/jumps/spotdodges arent triggered by the shield tilt input.
                cpu_data->input.lstick.X = stickX;
                cpu_data->input.lstick.Y = stickY;
                cpu_data->cpu.lstickX = stickX;
                cpu_data->cpu.lstickY = stickY;
            }
            break;
        }

        case (CPUBEHAVE_CROUCH):
        {
            // hold down
            cpu_data->cpu.lstickY = -127;
            break;
        }

        case (CPUBEHAVE_JUMP):
        {
            // run jump command
            Lab_CPUPerformAction(cpu, 12, hmn);
            break;
        }
        }

        break;
    }

    case (CPUSTATE_GRABBED):
    CPULOGIC_GRABBED:
    {
        // if no longer being grabbed, exit
        if (CPU_IsGrabbed(cpu, hmn) == 0)
        {
            eventData->cpu_state = CPUSTATE_RECOVER;
            goto CPULOGIC_RECOVER;
        }

        switch (LabOptions_CPU[OPTCPU_MASH].option_val)
        {
        case (CPUMASH_NONE):
        {
            Fighter_ZeroCPUInputs(cpu_data);
            break;
        }
        case (CPUMASH_MED):
        {
            if (HSD_Randi(100) <= CPUMASHRNG_MED)
            {
                // remove last frame inputs
                cpu_data->input.held = 0;
                cpu_data->input.lstick.X = 0;
                cpu_data->input.lstick.Y = 0;

                // input
                cpu_data->cpu.held = PAD_BUTTON_A;
                cpu_data->cpu.lstickX = 127;
            }
            break;
        }
        case (CPUMASH_HIGH):
        {
            if (HSD_Randi(100) <= CPUMASHRNG_HIGH)
            {
                // remove last frame inputs
                cpu_data->input.held = 0;
                cpu_data->input.lstick.X = 0;
                cpu_data->input.lstick.Y = 0;

                // input
                cpu_data->cpu.held = PAD_BUTTON_A;
                cpu_data->cpu.lstickX = 127;
            }
            break;
        }
        case (CPUMASH_PERFECT):
        {
            // remove last frame inputs
            cpu_data->input.held = 0;
            cpu_data->input.lstick.X = 0;
            cpu_data->input.lstick.Y = 0;

            // input
            cpu_data->cpu.held = PAD_BUTTON_A;
            cpu_data->cpu.lstickX = 127;
            break;
        }
        }

        if (LabOptions_CPU[OPTCPU_GRABRELEASE].option_val == CPUGRABRELEASE_AIRBORN)
        {
            cpu_data->cpu.held |= HSD_BUTTON_X;
        }

        break;
    }

    case (CPUSTATE_SDI):
    CPULOGIC_SDI:
    {
        static int lastSDIWasCardinal = 0;
        // if no more hitlag, enter tech state
        if (cpu_data->flags.hitlag == 0)
        {
            eventData->cpu_sdinum = 0; // Done SDIing, reset
            eventData->cpu_state = CPUSTATE_TECH;
            goto CPULOGIC_TECH;
        }

        // if hit during a techable animation, enter tech lockout.
        if (in_tumble_anim(cpu_data->TM.state_prev[0])) {
            int trap_type = LabOptions_Tech[OPTTECH_TRAP].option_val;
            if (trap_type == TECHTRAP_EARLIEST)
                eventData->cpu_tech_lockout = 20;
            else if (trap_type == TECHTRAP_LATEST)
                eventData->cpu_tech_lockout = 40;
        }

        // update move instance
        if (eventData->cpu_lasthit != cpu_data->dmg.atk_instance_hurtby)
        {
            eventData->cpu_countertimer = 0;
            eventData->cpu_hitnum++;
            eventData->cpu_lasthit = cpu_data->dmg.atk_instance_hurtby;

            // decide random SDI direction for grounded cpu
            if ((LabOptions_CPU[OPTCPU_SDINUM].option_val > 0) && (LabOptions_CPU[OPTCPU_SDIDIR].option_val == SDIDIR_RANDOM))
            {
                eventData->cpu_sdidir = HSD_Randi(2);
            }
        }

        // if final frame of hitlag, enter TDI state
        if (cpu_data->dmg.hitlag_frames == 1)
        {
            eventData->cpu_sdinum = 0; // Done SDIing, reset
            eventData->cpu_state = CPUSTATE_TDI;
            goto CPULOGIC_TDI;
        }

        // perform SDI behavior
        if (eventData->cpu_sdinum < LabOptions_CPU[OPTCPU_SDINUM].option_val)
        {
            eventData->cpu_sdinum++;
            float angle, magnitude;

            switch (LabOptions_CPU[OPTCPU_SDIDIR].option_val)
            {
            case SDIDIR_RANDOM:
            {
                // when grounded, only left right
                if (cpu_data->phys.air_state == 0)
                {
                    magnitude = 1;

                    // decide left or right
                    if (eventData->cpu_sdidir == 0)
                        angle = lastSDIWasCardinal ? M_1DEGREE * 17 : M_1DEGREE * 0; // right
                    else
                        angle = lastSDIWasCardinal ? M_1DEGREE * 197 : M_1DEGREE * 180; // left
                }
                // when airborne, any direction
                else
                {
                    // random input
                    angle = HSD_Randi(360) * M_1DEGREE;
                    magnitude = 0.49 + (HSD_Randf() * 0.51);

                    angle = get_angle_out_of_deadzone(angle, lastSDIWasCardinal);
                }

                break;
            }
            case SDIDIR_AWAY:
            {
                int dir = Fighter_GetOpponentDir(cpu_data, hmn_data) * -1;
                if (dir == 1)
                {
                    angle = 0;
                }
                else
                {
                    angle = 180;
                }
                angle = lastSDIWasCardinal ? (angle + 17) * M_1DEGREE : angle * M_1DEGREE;
                magnitude = 1;

                break;
            }
            case SDIDIR_TOWARD:
            {
                int dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
                if (dir == 1)
                {
                    angle = 0;
                }
                else
                {
                    angle = 180;
                }
                angle = lastSDIWasCardinal ? (angle + 17) * M_1DEGREE : angle * M_1DEGREE;
                magnitude = 1;

                break;
            }
            case SDIDIR_UP:
            {
                angle = lastSDIWasCardinal ? M_1DEGREE * 107 : M_1DEGREE * 90;
                magnitude = 1;
                break;
            }
            case SDIDIR_DOWN:
            {
                angle = lastSDIWasCardinal ? M_1DEGREE * 287 : M_1DEGREE * 270;
                magnitude = 1;
                break;
            }
            case SDIDIR_LEFT:
            {
                angle = lastSDIWasCardinal ? M_1DEGREE * 197 : M_1DEGREE * 180;
                magnitude = 1;
                break;
            }
            case SDIDIR_RIGHT:
            {
                angle = lastSDIWasCardinal ? M_1DEGREE * 17 : M_1DEGREE * 0;
                magnitude = 1;
                break;
            }
            }

            // flip if the last sdi was cardinal
            lastSDIWasCardinal = lastSDIWasCardinal ? 0 : 1;

            // store
            cpu_data->cpu.lstickX = cos(angle) * 127 * magnitude;
            cpu_data->cpu.lstickY = sin(angle) * 127 * magnitude;
        }

        break;
    }

    case (CPUSTATE_TDI):
    CPULOGIC_TDI:
    {
        // if no more hitlag and not being thrown, enter tech state. this might never be hit, just being safe
        if ((cpu_data->flags.hitlag == 0) && (is_thrown == 0))
        {
            eventData->cpu_state = CPUSTATE_TECH;
            goto CPULOGIC_TECH;
        }

        // if in shield, no need to TDI
        if ((cpu_data->state_id >= ASID_GUARDON) && (cpu_data->state_id <= ASID_GUARDREFLECT))
            break;

        if (cpu_data->dmg.hitlag_frames > 1)
            goto CPULOGIC_SDI;

        float hit_angle; // knockback angle, ignoring direction.
        float dir; // x direction from cpu to attacker to DI based on.
        if (is_thrown)
        {
            FighterData *attacker_data = cpu_data->grab.attacker->userdata;

            float reverse_min_angle = 95.0f * M_1DEGREE;

            hit_angle = (float)attacker_data->throw_hitbox[0].angle * M_1DEGREE;

            if (attacker_data->state_id != ASID_THROWB) {
                if (hit_angle < reverse_min_angle) {
                    // Most throws
                    dir = -attacker_data->facing_direction;
                } else {
                    // Is a "reverse" throw (e.x. marth dthrow)
                    dir = attacker_data->facing_direction;
                    hit_angle = M_PI - hit_angle;
                }
            } else {
                // Bthrow is special - we can't use attacker direction cuz it's different between throws,
                // e.x. peach doesn't turn around, but fox does. So we need to use cpu direction.
                // We assume that bthrows will always be a "reverse" throw.

                dir = cpu_data->facing_direction;
                if (hit_angle > reverse_min_angle)
                    hit_angle = M_PI - hit_angle;
            }
        }
        else
        {
            hit_angle = Fighter_GetKnockbackAngle(cpu_data);
            dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
        }

        float kb_angle; // actual knockback angle.
        if (dir < 0)
            kb_angle = hit_angle;
        else
            kb_angle = M_PI - hit_angle;

        // modulus angle
        float M_PI2 = M_PI*2;
        while (kb_angle < 0) kb_angle += M_PI2;
        while (kb_angle >= M_PI2) kb_angle -= M_PI2;

        // perform TDI behavior
        int tdi_kind = LabOptions_CPU[OPTCPU_TDI].option_val;
        int asdi_kind = LabOptions_CPU[OPTCPU_ASDI].option_val;
        switch (tdi_kind)
        {
            case (CPUTDI_RANDOM):
            TDI_RANDOM:
            {
                switch (HSD_Randi(3)) {
                    case 0: goto TDI_IN;
                    case 1: goto TDI_OUT;
                    case 2: goto TDI_NONE;
                }
            }

            case (CPUTDI_IN):
            TDI_IN:
            {
                float tdi_angle;
                if (kb_angle <= M_PI)
                    tdi_angle = kb_angle - M_PI * 0.5 * dir;
                else
                    tdi_angle = kb_angle + M_PI * 0.5 * dir;

                cpu_data->cpu.lstickX = cos(tdi_angle) * 127;
                cpu_data->cpu.lstickY = sin(tdi_angle) * 127;

                break;
            }

            case (CPUTDI_OUT):
            TDI_OUT:
            {
                float tdi_angle;
                if (kb_angle <= M_PI)
                    tdi_angle = kb_angle + M_PI * 0.5 * dir;
                else
                    tdi_angle = kb_angle - M_PI * 0.5 * dir;

                cpu_data->cpu.lstickX = cos(tdi_angle) * 127;
                cpu_data->cpu.lstickY = sin(tdi_angle) * 127;

                break;
            }
            case (CPUTDI_CUSTOM):
            {
                int cpu_hitnum = eventData->cpu_hitnum;

                // ensure we have a DI input for this hitnum
                if (cpu_hitnum <= stc_tdi_val_num)
                {
                    // get the stick values for this hit num
                    cpu_hitnum--;
                    CustomTDI_Apply(cpu, hmn, &stc_tdi_vals[cpu_hitnum]);

                    // increment
                } else {
                    goto TDI_RANDOM;
                }

                break;
            }
            case (CPUTDI_RANDOM_CUSTOM):
            {
                int i = HSD_Randi(stc_tdi_val_num);

                CustomTDI_Apply(cpu, hmn, &stc_tdi_vals[i]);

                break;
            }
            case (CPUTDI_NONE):
            TDI_NONE:
            {
                Fighter_ZeroCPUInputs(cpu_data);
                break;
            }
        }

        switch (asdi_kind)
        {
            case (ASDI_AUTO):
            {
                // follow TDI, or custom SDI
                break;
            }
            case (ASDI_AWAY):
            {
                int dir = Fighter_GetOpponentDir(cpu_data, hmn_data) * -1;
                if (dir == 1)
                {
                    cpu_data->cpu.cstickX = 127;
                    cpu_data->cpu.cstickY = 0;
                }
                else
                {
                    cpu_data->cpu.cstickX = -127;
                    cpu_data->cpu.cstickY = 0;
                }
                break;
            }
            case (ASDI_TOWARD):
            {
                int dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
                if (dir == 1)
                {
                    cpu_data->cpu.cstickX = 127;
                    cpu_data->cpu.cstickY = 0;
                }
                else
                {
                    cpu_data->cpu.cstickX = -127;
                    cpu_data->cpu.cstickY = 0;
                }
                break;
            }
            case (ASDI_LEFT):
            {
                cpu_data->cpu.cstickX = -127;
                cpu_data->cpu.cstickY = 0;
                break;
            }
            case (ASDI_RIGHT):
            {
                cpu_data->cpu.cstickX = 127;
                cpu_data->cpu.cstickY = 0;
                break;
            }
            case (ASDI_UP):
            {
                cpu_data->cpu.cstickX = 0;
                cpu_data->cpu.cstickY = 127;
                break;
            }
            case (ASDI_DOWN):
            {
                cpu_data->cpu.cstickX = 0;
                cpu_data->cpu.cstickY = -127;
                break;
            }
        }

        // this is kinda meh, maybe i can come up with something better later
        // spoof last input as current input as to not trigger SDI
        // also spoof input as held for more than the SDI window
        cpu_data->input.lstick.X = ((float)cpu_data->cpu.lstickX * 0.0078125);
        cpu_data->input.timer_lstick_tilt_x = 5;
        cpu_data->input.lstick.Y = ((float)cpu_data->cpu.lstickY * 0.0078125);
        cpu_data->input.timer_lstick_tilt_y = 5;

        break;
    }

    case (CPUSTATE_NONE):
    CPULOGIC_NONE:
    {
        int state = cpu_data->state_id;
        if (ASID_DAMAGEFLYHI <= state && state <= ASID_DAMAGEFLYROLL || state == ASID_DAMAGEFALL)
            goto CPULOGIC_FORCE_TECH;
        else
            goto CPUSTATE_ENTERSTART;
    }

    case (CPUSTATE_TECH):
    CPULOGIC_TECH:
    {
        // if no more hitstun, go to counter
        if (cpu_data->flags.hitstun == 0)
        {
            // also reset stick timer (messes with airdodge wiggle)
            cpu_data->input.lstick.X = 0;
            cpu_data->input.timer_lstick_tilt_x = 254;
            eventData->cpu_state = CPUSTATE_COUNTER;
            goto CPULOGIC_COUNTER;
        }

        CPULOGIC_FORCE_TECH:

        // perform tech behavior
        int tech_kind = LabOptions_Tech[OPTTECH_TECH].option_val;
        s8 dir;
        s8 stickX = 0;
        s8 sincePress = 0;
        s8 since2Press = -1;
        s8 sinceXSmash = -1;
    TECH_SWITCH:
        switch (tech_kind)
        {
        case (CPUTECH_RANDOM):
        {
            int roll = HSD_Randi(100);
            int sum = LabOptions_Tech[OPTTECH_TECHINPLACECHANCE].option_val;
            if (roll < sum)
            {
                tech_kind = 1;
                goto TECH_SWITCH;
                break;
            }
            sum += LabOptions_Tech[OPTTECH_TECHAWAYCHANCE].option_val;
            if (roll < sum)
            {
                tech_kind = 2;
                goto TECH_SWITCH;
                break;
            }
            sum += LabOptions_Tech[OPTTECH_TECHTOWARDCHANCE].option_val;
            if (roll < sum)
            {
                tech_kind = 3;
                goto TECH_SWITCH;
                break;
            }
            tech_kind = 4;
            goto TECH_SWITCH;
        }
        case (CPUTECH_NEUTRAL):
        {
            break;
        }
        case (CPUTECH_AWAY):
        {
            dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
            stickX = 40 * (dir * -1);
            break;
        }
        case (CPUTECH_TOWARDS):
        {
            dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
            stickX = 40 * (dir);
            break;
        }
        case (CPUTECH_NONE):
        {
            sincePress = -1;
            break;
        }
        }

        if (eventData->cpu_tech_lockout <= 0) {
            // input tech
            cpu_data->input.timer_LR = sincePress;
            cpu_data->input.since_rapid_lr = since2Press;
            cpu_data->cpu.lstickX = stickX;
            cpu_data->input.timer_lstick_smash_x = sinceXSmash;
        } else {
            cpu_data->input.timer_LR = 255;
            cpu_data->input.since_rapid_lr = 255;
            cpu_data->cpu.lstickX = 0;
            cpu_data->input.timer_lstick_smash_x = 255;
        }

        break;
    }

    case (CPUSTATE_GETUP):
    CPULOGIC_GETUP:
    {

        // if im in downwait, perform getup logic
        if ((cpu_data->state_id == ASID_DOWNWAITD) || (cpu_data->state_id == ASID_DOWNWAITU))
        {
            // perform getup behavior
            int getup = LabOptions_Tech[OPTTECH_GETUP].option_val;
            s8 dir;
            int inputs = 0;
            s8 stickX = 0;
            s8 stickY = 0;

        GETUP_SWITCH:
            switch (getup)
            {
            case (CPUGETUP_RANDOM):
            {
                int roll = HSD_Randi(100);
                int sum = LabOptions_Tech[OPTTECH_GETUPSTANDCHANCE].option_val;
                if (roll < sum)
                {
                    getup = 1;
                    goto GETUP_SWITCH;
                    break;
                }
                sum += LabOptions_Tech[OPTTECH_GETUPAWAYCHANCE].option_val;
                if (roll < sum)
                {
                    getup = 2;
                    goto GETUP_SWITCH;
                    break;
                }
                sum += LabOptions_Tech[OPTTECH_GETUPTOWARDCHANCE].option_val;
                if (roll < sum)
                {
                    getup = 3;
                    goto GETUP_SWITCH;
                    break;
                }
                getup = 4;
                goto GETUP_SWITCH;
                break;
            }
            case (CPUGETUP_STAND):
            {
                stickY = 127;
                break;
            }
            case (CPUGETUP_TOWARD):
            {
                dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
                stickX = 127 * (dir);
                break;
            }
            case (CPUGETUP_AWAY):
            {
                dir = Fighter_GetOpponentDir(cpu_data, hmn_data);
                stickX = 127 * (dir * -1);
                break;
            }
            case (CPUGETUP_ATTACK):
            {
                inputs = PAD_BUTTON_A;
                break;
            }
            }

            // input getup option
            cpu_data->cpu.held = inputs;
            cpu_data->cpu.lstickX = stickX;
            cpu_data->cpu.lstickY = stickY;
        }

        // if cpu is in any other down state, do nothing
        else if ((cpu_data->state_id >= ASID_DOWNBOUNDU) && (cpu_data->state_id <= ASID_DOWNSPOTD))
        {
            break;
        }

        // if cpu is not in a down state, enter COUNTER
        else
        {
            eventData->cpu_state = CPUSTATE_COUNTER;
            goto CPULOGIC_COUNTER;
            break;
        }

        break;
    }

    case (CPUSTATE_COUNTER):
    CPULOGIC_COUNTER:
    {
        int actionable_this_frame = CPUAction_CheckASID(cpu, ASID_ACTIONABLE);

        // check if the CPU has been actionable yet
        if (eventData->cpu_isactionable == 0)
        {
            if (!actionable_this_frame) break;

            eventData->cpu_isactionable = 1;                       // set actionable flag to begin running code
            eventData->cpu_groundstate = cpu_data->phys.air_state; // remember initial ground state
        }

        // if started in the air, didnt finish action, but now grounded, perform ground action
        if ((eventData->cpu_groundstate == 1) && (cpu_data->phys.air_state == 0))
            eventData->cpu_groundstate = 0;

        // if started on the ground, didnt finish action, but now airborne, perform air action
        if ((eventData->cpu_groundstate == 0) && (cpu_data->phys.air_state == 1))
            eventData->cpu_groundstate = 1;

        // ensure hit count and frame count criteria are met
        int action_id;
        if (eventData->cpu_hitkind == HITKIND_DAMAGE)
        {
            if ((eventData->cpu_hitnum < LabOptions_CPU[OPTCPU_CTRHITS].option_val) || (eventData->cpu_countertimer < LabOptions_CPU[OPTCPU_CTRFRAMES].option_val))
            {
                break;
            }

            // get counter action
            if (cpu_data->phys.air_state == 0 || (eventData->cpu_groundstate == 0)) // if am grounded or started grounded
            {
                int grndCtr = LabOptions_CPU[OPTCPU_CTRGRND].option_val;
                action_id = CPUCounterActionsGround[grndCtr];
            }
            else if (cpu_data->phys.air_state == 1) // only if in the air at the time of hitstun ending
            {
                int airCtr = LabOptions_CPU[OPTCPU_CTRAIR].option_val;
                action_id = CPUCounterActionsAir[airCtr];
            }
        }
        else if (eventData->cpu_hitkind == HITKIND_SHIELD)
        {

            // if the shield wasnt hit enough times, return to start
            if (eventData->cpu_hitshieldnum < LabOptions_CPU[OPTCPU_SHIELDHITS].option_val)
            {
                eventData->cpu_state = CPUSTATE_START;
                goto CPULOGIC_START;
                break;
            }

            // if this isnt the frame to counter, keep holding shield
            if (eventData->cpu_countertimer < LabOptions_CPU[OPTCPU_CTRFRAMES].option_val)
            {
                cpu_data->cpu.held = PAD_TRIGGER_R;
                break;
            }

            // get action to perform
            int shieldCtr = LabOptions_CPU[OPTCPU_CTRSHIELD].option_val;
            action_id = CPUCounterActionsShield[shieldCtr];
        }

        // Additional check for the grab release counter action
        else if (last_state == ASID_CAPTURECUT || last_state == ASID_CAPTUREJUMP)
        {

            if (eventData->cpu_countertimer < LabOptions_CPU[OPTCPU_CTRFRAMES].option_val)
            {
                break;
            }
            
            if (cpu_data->phys.air_state == 0 || eventData->cpu_groundstate == 0) // if am grounded or started grounded
            {
                int grndCtr = LabOptions_CPU[OPTCPU_CTRGRND].option_val;
                action_id = CPUCounterActionsGround[grndCtr];
            }

            else if (cpu_data->phys.air_state == 1) // only if in the air when the cpu_countertimer is up
            {
                int airCtr = LabOptions_CPU[OPTCPU_CTRAIR].option_val;
                action_id = CPUCounterActionsAir[airCtr];
            }
        }
        else
        {
            // wasnt hit, fell or something idk. enter start again
            goto CPUSTATE_ENTERSTART;
        }

        if (action_id == 0) {
            eventData->cpu_state = CPUSTATE_NONE;
            goto CPULOGIC_NONE;
        }
        else 
        {
            eventData->cpu_countering = true;
            if (Lab_CPUPerformAction(cpu, action_id, hmn))
                eventData->cpu_state = CPUSTATE_RECOVER;
        }

        break;
    }

    case (CPUSTATE_RECOVER):
    CPULOGIC_RECOVER:
    {

        // if onstage, go back to start
        if (cpu_data->phys.air_state == 0)
        {

        CPUSTATE_ENTERSTART:
            // clear inputs

            // go to start
            eventData->cpu_state = CPUSTATE_START;
            eventData->cpu_hitshield = 0;
            eventData->cpu_hitnum = 0;
            eventData->cpu_countertimer = 0;
            eventData->cpu_hitshield = 0;
            eventData->cpu_lasthit = -1;
            eventData->cpu_lastshieldstun = -1;
            eventData->cpu_hitkind = -1;
            eventData->cpu_hitshieldnum = 0;
            eventData->cpu_isactionable = 0;
            eventData->cpu_sdinum = 0;
            goto CPULOGIC_START;
        }

        // recover with CPU AI
        cpu_data->cpu.ai = 0;

        break;
    }
    }

    // We need to reset the counter whenever the CPU is inactionable.
    // Otherwise, if the cpu is actionable in the air before landing,
    // then the countertimer value will increment even during tech options.
    if (CPUAction_CheckASID(cpu, ASID_ACTIONABLE) || eventData->cpu_countering) {
        eventData->cpu_countertimer++;
    } else {
        eventData->cpu_countertimer = 0;
    }

    // update isthrown
    eventData->cpu_isthrown = is_thrown;

    // update cpu_hitshield
    if (eventData->cpu_hitshield == 0)
    {
        GOBJ *fighter = gobj_lookup[MATCHPLINK_FIGHTER];
        while (fighter != 0)
        {
            FighterData *fighter_data = fighter->userdata;

            // check if in guard off
            if (fighter_data->state_id == ASID_GUARDSETOFF)
            {
                eventData->cpu_hitshield = 1;
                break;
            }

            fighter = fighter->next;
        }
    }
}
int Update_CheckPause()
{
    HSD_Update *update = stc_hsd_update;
    int isChange = 0;

    // get their pad
    int controller = Fighter_GetControllerPort(stc_hmn_controller);
    HSD_Pad *pad = PadGet(controller, PADGET_MASTER);

    // if event menu not showing, develop mode + pause input, toggle frame advance
    if ((Pause_CheckStatus(1) != 2) && (*stc_dblevel >= 3) && (pad->down & HSD_BUTTON_START))
    {
        LabOptions_General[OPTGEN_FRAME].option_val ^= 1;
        Lab_ChangeFrameAdvance(0, LabOptions_General[OPTGEN_FRAME].option_val);
        isChange = 1;
    }

    // menu paused
    else if (LabOptions_General[OPTGEN_FRAME].option_val == 1)
    {
        // check if unpaused
        if (update->pause_kind != PAUSEKIND_SYS)
        {
            // pause
            isChange = 1;
        }
    }

    // menu unpaused
    else
    {
        // check if paused
        if (update->pause_kind == PAUSEKIND_SYS)
        {
            // unpause
            isChange = 1;
        }
    }

    return isChange;
}
int Update_CheckAdvance()
{

    static int timer = 0;

    HSD_Update *update = stc_hsd_update;
    int isAdvance = 0;

    int controller = Fighter_GetControllerPort(0);

    // get their pad
    HSD_Pad *pad = PadGet(controller, PADGET_MASTER);
    HSD_Pad *engine_pad = PadGet(controller, PADGET_ENGINE);

    // get their advance input
    static int stc_advance_btns[] = {HSD_TRIGGER_L, HSD_TRIGGER_Z, HSD_BUTTON_X, HSD_BUTTON_Y, HSD_TRIGGER_R};
    int advance_btn = stc_advance_btns[LabOptions_General[OPTGEN_FRAMEBTN].option_val];

    // check if holding L
    if (!LabOptions_General[OPTGEN_FRAMEBTN].disable && (pad->held & advance_btn))
    {
        timer++;

        // advance if first press or holding more than 10 frames
        if (timer == 1 || timer > 30)
        {
            isAdvance = 1;

            // remove button input
            pad->down &= ~advance_btn;
            pad->held &= ~advance_btn;
            engine_pad->down &= ~advance_btn;
            engine_pad->held &= ~advance_btn;

            // if using L, remove analog press too
            if (advance_btn == HSD_TRIGGER_L)
            {
                pad->triggerLeft = 0;
                pad->ftriggerLeft = 0;
                engine_pad->triggerLeft = 0;
                engine_pad->ftriggerLeft = 0;
            }
            else if (advance_btn == HSD_TRIGGER_R)
            {
                pad->triggerRight = 0;
                pad->ftriggerRight = 0;
                engine_pad->triggerRight = 0;
                engine_pad->ftriggerRight = 0;
            }
        }
    }

    else
    {
        update->advance = 0;
        timer = 0;
    }

    return isAdvance;
}
void DIDraw_Init()
{
    // Create DIDraw GOBJ
    GOBJ *didraw_gobj = GObj_Create(0, 0, 0);
    // Add gxlink
    GObj_AddGXLink(didraw_gobj, DIDraw_GX, 6, 0);
    // init didraw pointers
    for (int i = 0; i < 6; i++)
    {
        // for each subchar
        for (int j = 0; j < 2; j++)
        {
            didraws[i].num[j] = 0;
            didraws[i].vertices[j] = 0;
        }
    }

    return;
}
void DIDraw_Update()
{
    static ECBSize ecb_size_def = {
        .topY = 9,
        .botY = 2.5,
        .left = {-3.3, 5.7},
        .right = {3.3, 5.7},
    };

    // if enabled and pause menu isnt shown, update di draw
    if ((LabOptions_General[OPTGEN_DI].option_val == 1)) //  && (Pause_CheckStatus(1) != 2)
    {
        for (int i = 0; i < 4; ++i) {
            GOBJ *fighter = Fighter_GetGObj(i);
            if (fighter == 0) continue;

            FighterData *fighter_data = fighter->userdata;
            int ply = fighter_data->ply;
            DIDraw *didraw = &didraws[ply];

            // if in hitlag and hitstun simulate and update trajectory
            if (fighter_data->flags.hitstun == 1)
            {
                // free old
                if (didraw->vertices[ply] != 0)
                {
                    HSD_Free(didraw->vertices[ply]);
                    didraw->num[ply] = 0;
                    didraw->vertices[ply] = 0;
                }

                // get player's inputs
                float lstickX;
                float lstickY;
                float cstickX;
                float cstickY;
                // for HMN players
                if (Fighter_GetSlotType(fighter_data->ply) == 0)
                {
                    int input_kind;
                    if (Pause_CheckStatus(0) == 1) // if frame advance enabled, use master inputs
                        input_kind = PADGET_MASTER;
                    else
                        input_kind = PADGET_ENGINE; // no frame advance, use engine inputs
                    HSD_Pad *pad = PadGet(fighter_data->pad_index, input_kind);
                    lstickX = pad->fstickX;
                    lstickY = pad->fstickY;
                    cstickX = pad->fsubstickX;
                    cstickY = pad->fsubstickY;
                }
                // for CPUs
                else
                {
                    lstickX = fighter_data->input.lstick.X;
                    lstickY = fighter_data->input.lstick.Y;
                    cstickX = fighter_data->input.cstick.X;
                    cstickY = fighter_data->input.cstick.Y;
                }

                // get kb vector
                Vec3 kb = fighter_data->phys.kb_vel;
                float kb_angle = atan2(kb.Y, kb.X);
                // init ASDI vector
                Vec3 asdi_orig;
                Vec3 asdi = {0, 0, 0};
                // get fighter constants
                ftCommonData *ftCmDt = R13_PTR(PLCO_FTCOMMON);

                if (fighter_data->flags.hitlag) {
                    // Calculate ASDI
                    float asdi_mag = pow(ftCmDt->asdi_mag, 2);
                    float asdi_units = ftCmDt->asdi_units;
                    // CStick has priority, check if mag > 0.7
                    if (pow(cstickX, 2) + (pow(cstickY, 2)) >= asdi_mag)
                    {
                        asdi.X = cstickX * asdi_units;
                        asdi.Y = cstickY * asdi_units;
                    }
                    // now check if lstick mag > 0.7
                    else if (pow(lstickX, 2) + (pow(lstickY, 2)) >= asdi_mag)
                    {
                        asdi.X = lstickX * asdi_units;
                        asdi.Y = lstickY * asdi_units;
                    }
                    // Remember original ASDI
                    asdi_orig = asdi;

                    // Calculate TDI
                    if ((lstickX != 0) || (lstickY != 0)) // exclude input vector 0,0
                    {
                        // kb vector must exceed 0.00001
                        float kb_mult = (kb.Y * kb.Y) + (kb.X * kb.X);
                        if (kb_mult >= 0.00001)
                        {

                            // get values
                            float tdi_input = pow((-1 * kb.X * lstickY) + (lstickX * kb.Y), 2) / kb_mult;
                            float max_angle = ftCmDt->tdi_maxAngle * M_1DEGREE;
                            float kb_mag = sqrtf(kb_mult);

                            // check to negate
                            Vec3 inputs = {lstickX, lstickY, 0};
                            Vec3 result;
                            VECCrossProduct(&kb, &inputs, &result);
                            if (result.Z < 0)
                                tdi_input *= -1;

                            // apply TDI
                            kb_angle = (max_angle * tdi_input) + kb_angle;

                            // New X KB
                            kb.X = cos(kb_angle) * kb_mag;
                            // New Y KB
                            kb.Y = sin(kb_angle) * kb_mag;
                        }
                    }
                }

                //simulation variables
                int air_state = fighter_data->phys.air_state;
                float y_vel = fighter_data->phys.self_vel.Y;
                Vec3 pos = fighter_data->phys.pos;
                ftCommonData *ftCommon = R13_PTR(-0x514C);
                float decay = ftCommon->kb_frameDecay;
                int hitstun_frames = AS_FLOAT(fighter_data->state_var.state_var1);
                int vertices_num = 0;    // used to track how many vertices will be needed
                int override_frames = 0; // used as an alternate countdown
                DIDrawCalculate *DICollData = calloc(sizeof(DIDrawCalculate) * hitstun_frames);
                CollData ecb;
                ECBSize ecb_size;

                // init ecb struct
                Coll_InitECB(&ecb);
                if (fighter_data->phys.air_state == 0) // copy ecb struct if grounded
                {
                    memcpy(&ecb.envFlags, &fighter_data->coll_data.envFlags, 0x28);
                }

                // simulate each frame of knockback
                for (int i = 0; i < hitstun_frames; i++)
                {

                    // update bone positions.  If loop count < noECBUpdate-remaining hitlag fraes, use current ECB bottom Y offset
                    if (vertices_num < (fighter_data->coll_data.u.ecb_bot_lock_frames - fighter_data->dmg.hitlag_frames))
                    {

                        ecb_size.topY = fighter_data->coll_data.ecbCurr_top.Y;
                        ecb_size.botY = fighter_data->coll_data.ecbCurr_bot.Y;
                        ecb_size.left = fighter_data->coll_data.ecbCurr_left;
                        ecb_size.right = fighter_data->coll_data.ecbCurr_right;

                        // if grounded, ECB bottom is 0
                        if (air_state == 0)
                        {
                            ecb_size.botY = 0;
                        }
                    }
                    else
                    {
                        // use default ecb size
                        memcpy(&ecb_size, &ecb_size_def, sizeof(ECBSize));
                    }
                    // update ecb topN position
                    ecb.topN_Curr = pos;
                    ecb.topN_CurrCorrect = pos;

                    // apply ASDI
                    pos.X += asdi.X;
                    if (air_state != 0) // only apply Y asdi in the air
                    {
                        pos.Y += asdi.Y;
                    }
                    asdi.X = 0; // zero out ASDI
                    asdi.Y = 0; // zero out ASDI

                    // update y_vel
                    y_vel -= fighter_data->attr.gravity;
                    float terminal_velocity = fighter_data->attr.terminal_velocity * -1;
                    if (y_vel < terminal_velocity)
                        y_vel = terminal_velocity;
                    // decay KB vector
                    float angle = atan2(kb.Y, kb.X);
                    kb.X = kb.X - (cos(angle) * decay);
                    kb.Y = kb.Y - (sin(angle) * decay);

                    // add knockback
                    VECAdd(&pos, &kb, &pos);
                    // apply y_vel
                    pos.Y += y_vel;

                    // ecb prev = ecb curr
                    ecb.topN_Prev = ecb.topN_Curr;
                    // ecb curr = new position
                    ecb.topN_Curr = pos;

                    // only run X collision checks because theyre expensive
                    if (vertices_num >= DI_MaxColl)
                    {
                        DICollData[i].envFlags = 0; // spoof as not touching
                        pos = ecb.topN_Curr;        // position = projected
                    }

                    // update collision
                    else
                    {
                        // ground coll
                        if (air_state == 0)
                        {
                            int result = ECB_CollGround_PassLedge(&ecb, &ecb_size);
                            if (result == 0)
                            {
                                air_state = 1;
                            }
                        }
                        // air coll
                        else
                        {
                            ECB_CollAir(&ecb, &ecb_size);
                        }

                        // get corrected position
                        pos = ecb.topN_Curr;

                        // increment collision ID
                        ecb.coll_test += 1;

                        // perform stage collision code
                        if ((ecb.envFlags & ECB_GROUND) != 0)
                        {
                            // check if air->ground just occurred
                            if (air_state == 1)
                            {
                                // set grounded
                                air_state = 0;

                                // check if over max horizontal velocity
                                if (kb.X > ftCmDt->kb_maxVelX)
                                    kb.X = ftCmDt->kb_maxVelX;
                                if (kb.X < -ftCmDt->kb_maxVelX)
                                    kb.X = -ftCmDt->kb_maxVelX;

                                // adjust KB direction from slope
                                kb.X *= ecb.ground_slope.Y;
                                kb.Y *= ecb.ground_slope.X;

                                // zero out y_vel
                                y_vel = 0;

                                // zero out Y KB
                                kb.Y = 0;
                            }
                        }
                        else if ((ecb.envFlags & ECB_CEIL) != 0)
                        {
                            // only run this code when in the air
                            if (air_state == 1)
                            {
                                // combine KB and Gravity
                                Vec3 kb_temp = kb;
                                Vec3 vel_temp = {0, y_vel, 0};
                                VECAdd(&vel_temp, &kb_temp, &vel_temp);

                                // apply slope
                                VECMultAndAdd(&vel_temp, &ecb.ceil_slope);

                                // decay kb
                                kb.X = vel_temp.X * ftCmDt->kb_bounceDecay;
                                kb.Y = vel_temp.Y * ftCmDt->kb_bounceDecay;
                            }
                        }
                        else if ((ecb.envFlags & (ECB_WALLLEFT | ECB_WALLRIGHT)) != 0)
                        {
                            // only run this code when in the air
                            if (air_state == 1)
                            {

                                // get slope
                                Vec3 *slope;
                                if ((ecb.envFlags & ECB_WALLLEFT) != 0)
                                {
                                    slope = &ecb.leftwall_slope;
                                }
                                else
                                {
                                    slope = &ecb.rightwall_slope;
                                }

                                // combine KB and Gravity
                                Vec3 kb_temp = kb;
                                Vec3 vel_temp = {0, y_vel, 0};
                                VECAdd(&vel_temp, &kb_temp, &vel_temp);

                                // apply slope
                                VECMultAndAdd(&vel_temp, slope);

                                // decay kb
                                kb.X = vel_temp.X * ftCmDt->kb_bounceDecay;
                                kb.Y = vel_temp.Y * ftCmDt->kb_bounceDecay;

                                // zero y_vel
                                y_vel = 0;
                            }
                        }

                        // check for slide off
                        if (((ecb.envFlags & ECB_GROUND) == 0) && ((ecb.envFlags_prev & ECB_GROUND) != 0)) // not touching ground this frame, was touching last frame
                        {
                            if (override_frames == 0)
                                override_frames = 5; // terminate in 5 frames
                        }
                    }

                    // save this position
                    DICollData[i].pos.X = pos.X;
                    DICollData[i].pos.Y = pos.Y;
                    DICollData[i].kb_Y = kb.Y;
                    DICollData[i].ECBLeftY = ecb_size.left.Y;
                    DICollData[i].ECBTopY = ecb_size.topY;

                    // inc vertices count
                    vertices_num++;

                    // if override frames are set, decrement and exit
                    if (override_frames > 0)
                    {
                        override_frames--;
                        if (override_frames == 0)
                            break;
                    }
                }

                // alloc draw struct
                didraw->num[ply] = vertices_num + 2; // +2 for ASDI vertices
                // alloc vertices
                didraw->vertices[ply] = calloc(sizeof(Vec2) * (vertices_num + 2));

                // save ASDI first
                didraw->vertices[ply][0].X = fighter_data->coll_data.topN_Curr.X;
                didraw->vertices[ply][0].Y = fighter_data->coll_data.topN_Curr.Y + fighter_data->coll_data.ecbCurr_left.Y;
                didraw->vertices[ply][1].X = fighter_data->coll_data.topN_Curr.X + asdi_orig.X;
                didraw->vertices[ply][1].Y = fighter_data->coll_data.topN_Curr.Y + fighter_data->coll_data.ecbCurr_left.Y + asdi_orig.Y;

                // save this info to the draw struct
                for (int i = 0; i < vertices_num; i++)
                {
                    didraw->vertices[ply][i + 2].X = DICollData[i].pos.X;
                    didraw->vertices[ply][i + 2].Y = DICollData[i].pos.Y + ((DICollData[i].ECBLeftY + DICollData[i].ECBTopY) / 2);

                    // get vertex color
                    static GXColor airColor = {0, 138, 255, 255};
                    static GXColor groundColor = {255, 255, 255, 255};
                    static GXColor ceilColor = {255, 0, 0, 255};
                    static GXColor wallColor = {0, 255, 0, 255};
                    GXColor *color;
                    if ((ecb.envFlags & ECB_GROUND) != 0)
                        color = &groundColor;
                    else if ((ecb.envFlags & ECB_CEIL) != 0)
                        color = &ceilColor;
                    else if ((ecb.envFlags & ECB_WALLLEFT | ECB_WALLRIGHT) != 0)
                        color = &wallColor;
                    else
                        color = &airColor;
                    // set vertex color
                    didraw->color.r = color->r;
                    didraw->color.g = color->g;
                    didraw->color.b = color->b;
                    didraw->color.a = color->a;
                }

                // free the collision info
                HSD_Free(DICollData);
            }
            // if not in hitstun, zero out didraw
            else if (fighter_data->flags.hitstun == 0)
            {
                if (didraw->vertices[ply] != 0)
                {
                    HSD_Free(didraw->vertices[ply]);
                    didraw->num[ply] = 0;
                    didraw->vertices[ply] = 0;
                }
            }
        }
    }

    // is off, remove all di draw
    else
    {
        // all slots
        for (int i = 0; i < 6; i++)
        {
            DIDraw *didraw = &didraws[i];

            // all subchars
            for (int j = 0; j < 2; j++)
            {
                if (didraw->vertices[j] != 0)
                {
                    HSD_Free(didraw->vertices[j]);
                    didraw->num[j] = 0;
                    didraw->vertices[j] = 0;
                }
            }
        }
    }

    return;
}
void DIDraw_GX()
{
    // if toggle enabled
    if (LabOptions_General[OPTGEN_DI].option_val == 1)
    {

        // draw each
        for (int i = 0; i < 6; i++)
        {
            // for each subchar
            for (int j = 0; j < 2; j++)
            {
                DIDraw *didraw = &didraws[i];
                // if it exists
                if (didraw->num != 0)
                {
                    int vertex_num = didraw->num[j];
                    Vec2 *vertices = didraw->vertices[j];

                    // alloc prim
                    PRIM *gx = PRIM_NEW(vertex_num, 0x001F1306, 0x00000C55);

                    // draw each
                    for (int k = 0; k < vertex_num; k++)
                    {
                        PRIM_DRAW(gx, vertices[k].X, vertices[k].Y, 0, 0x008affff);
                    }

                    // close
                    PRIM_CLOSE();
                }
            }
        }
    }
    return;
}
void Update_Camera()
{
    // if camera is set to advanced
    if (LabOptions_General[OPTGEN_CAM].option_val == 3)
    {

        // Get player gobj
        GOBJ *fighter = Fighter_GetGObj(0);
        if (fighter != 0)
        {

            // get players inputs
            FighterData *fighter_data = fighter->userdata;
            HSD_Pad *pad = PadGet(fighter_data->pad_index, PADGET_MASTER);
            int held = pad->held;
            float stickX = pad->fsubstickX;
            float stickY = pad->fsubstickY;

            if (fabs(stickX) < STICK_DEADZONE)
                stickX = 0;
            if (fabs(stickY) < STICK_DEADZONE)
                stickY = 0;

            if (stickX != 0 || stickY != 0)
            {
                COBJ *cobj = Match_GetCObj();

                // adjust pan
                if ((held & HSD_BUTTON_A) != 0)
                {
                    DevCam_AdjustPan(cobj, stickX * -1, stickY * -1);
                }
                // adjust zoom
                else if ((held & HSD_BUTTON_Y) != 0)
                {
                    DevCam_AdjustZoom(cobj, stickY);
                }
                // adjust rotate
                else if ((held & HSD_BUTTON_B) != 0)
                {
                    MatchCamera *matchCam = MATCH_CAM;
                    DevCam_AdjustRotate(cobj, &matchCam->devcam_rot, &matchCam->devcam_pos, stickX, stickY);
                }
            }
        }
    }

    return;
}

void CustomTDI_Apply(GOBJ *cpu, GOBJ *hmn, CustomTDI *di)
{
    FighterData *hmn_data = hmn->userdata;
    FighterData *cpu_data = cpu->userdata;

    float di_direction = 1.f;
    float diff = cpu_data->phys.pos.X - hmn_data->phys.pos.X;

    int cur_direction;
    if (diff > 3.f) {
        cur_direction = 1;
    } else if (diff < -3.f) {
        cur_direction = 0;
    } else {
        cur_direction = hmn_data->facing_direction > 0.f;
    }

    if (di->reversing && di->direction != cur_direction)
        di_direction = -1.f;

    cpu_data->cpu.lstickX = di->lstickX * 127.f * di_direction;
    cpu_data->cpu.lstickY = di->lstickY * 127.f;
    cpu_data->cpu.cstickX = di->cstickX * 127.f * di_direction;
    cpu_data->cpu.cstickY = di->cstickY * 127.f;
}

void Lab_SelectCustomTDI(GOBJ *menu_gobj)
{
    MenuData *menu_data = menu_gobj->userdata;
    EventMenu *curr_menu = menu_data->currMenu;
    evMenu *menuAssets = event_vars->menu_assets;
    GOBJ *event_gobj = event_vars->event_gobj;
    LabData *event_data = event_gobj->userdata;
    Arch_LabData *LabAssets = stc_lab_data;

    // set menu state to wait
    //curr_menu->state = EMSTATE_WAIT;

    // create bg gobj
    GOBJ *tdi_gobj = GObj_Create(0, 0, 0);
    TDIData *userdata = calloc(sizeof(TDIData));
    GObj_AddUserData(tdi_gobj, 4, HSD_Free, userdata);

    // load menu joint
    JOBJ *tdi_joint = JOBJ_LoadJoint(menuAssets->menu);
    GObj_AddObject(tdi_gobj, 3, tdi_joint);                                     // add to gobj
    GObj_AddGXLink(tdi_gobj, GXLink_Common, GXLINK_MENUMODEL, GXPRI_MENUMODEL); // add gx link
    menu_data->custom_gobj_think = CustomTDI_Update;                            // set callback

    // load current stick joints
    JOBJ *stick_joint = JOBJ_LoadJoint(stc_lab_data->stick);
    stick_joint->scale.X = 2;
    stick_joint->scale.Y = 2;
    stick_joint->scale.Z = 2;
    stick_joint->trans.X = -6;
    stick_joint->trans.Y = -6;
    userdata->stick_curr[0] = stick_joint;
    JOBJ_AddChild(tdi_gobj->hsd_object, stick_joint);
    // current c stick
    stick_joint = JOBJ_LoadJoint(stc_lab_data->cstick);
    stick_joint->scale.X = 2;
    stick_joint->scale.Y = 2;
    stick_joint->scale.Z = 2;
    stick_joint->trans.X = 6;
    stick_joint->trans.Y = -6;
    userdata->stick_curr[1] = stick_joint;
    JOBJ_AddChild(tdi_gobj->hsd_object, stick_joint);

    // create stick curr text
    Text *text_curr = Text_CreateText(2, menu_data->canvas_menu);
    userdata->text_curr = text_curr;
    // enable align and kerning
    text_curr->align = 0;
    text_curr->kerning = 1;
    // scale canvas
    text_curr->viewport_scale.X = MENU_CANVASSCALE;
    text_curr->viewport_scale.Y = MENU_CANVASSCALE;
    text_curr->trans.Z = MENU_TEXTZ;
    // create hit num
    Text_AddSubtext(text_curr, -50, 185, &nullString);
    // create lstick coords
    for (int i = 0; i < 2; i++)
    {
        Text_AddSubtext(text_curr, -400, (80 + i * 40), &nullString);
    }
    // create cstick coords
    for (int i = 0; i < 2; i++)
    {
        Text_AddSubtext(text_curr, 250, (80 + i * 40), &nullString);
    }

    // create prev sticks
    for (int i = 0; i < TDI_DISPNUM; i++)
    {
        // left stick
        JOBJ *prevstick_joint = JOBJ_LoadJoint(stc_lab_data->stick);
        prevstick_joint->scale.X = 1;
        prevstick_joint->scale.Y = 1;
        prevstick_joint->scale.Z = 1;
        prevstick_joint->rot.X = 0.4;
        prevstick_joint->trans.X = -22 + (i * (55 / TDI_DISPNUM));
        prevstick_joint->trans.Y = 10;
        JOBJ_SetFlags(prevstick_joint, JOBJ_HIDDEN);
        userdata->stick_prev[i][0] = prevstick_joint;
        JOBJ_AddChild(tdi_gobj->hsd_object, prevstick_joint);

        // cstick
        prevstick_joint = JOBJ_LoadJoint(stc_lab_data->cstick);
        prevstick_joint->scale.X = 1;
        prevstick_joint->scale.Y = 1;
        prevstick_joint->scale.Z = 1;
        prevstick_joint->rot.X = 0.4;
        prevstick_joint->trans.X = -18 + (i * (55 / TDI_DISPNUM));
        prevstick_joint->trans.Y = 8;
        JOBJ_SetFlags(prevstick_joint, JOBJ_HIDDEN);
        userdata->stick_prev[i][1] = prevstick_joint;
        JOBJ_AddChild(tdi_gobj->hsd_object, prevstick_joint);

        // create text
        Text_AddSubtext(text_curr, (-460 + (i * ((55 * 19.6) / TDI_DISPNUM))), -100, &nullString);
    }

    // create description text
    Text_AddSubtext(text_curr, -460, 240, "Input TDI angles for the CPU to use.");
    Text_AddSubtext(text_curr, -460, 285, "A = Save Input  X = Delete Input  B = Return");
    Text_AddSubtext(text_curr, -460, 320, "Z = Reversible");

    // hide original menu
    event_vars->hide_menu = 1;

    // set pointers to custom gobj
    menu_data->custom_gobj = tdi_gobj;
    menu_data->custom_gobj_destroy = CustomTDI_Destroy;

    return;

    /*
    // Change color
    GXColor gx_color = TEXT_BGCOLOR;
    popup_joint->dobj->mobj->mat->diffuse = gx_color;
*/
}
void CustomTDI_Update(GOBJ *gobj)
{
    // get data
    TDIData *tdi_data = gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;
    LabData *event_data = event_vars->event_gobj->userdata;

    // get pausing players inputs
    HSD_Pad *pad = PadGet(menu_data->controller_index, PADGET_MASTER);
    int inputs = pad->down;

    // if press A, save stick
    if ((inputs & HSD_BUTTON_A) != 0 && stc_tdi_val_num < TDI_HITNUM) {
        GOBJ *hmn = Fighter_GetGObj(0);
        GOBJ *cpu = Fighter_GetGObj(1);
        FighterData *hmn_data = hmn->userdata;
        FighterData *cpu_data = cpu->userdata;
        int direction = cpu_data->phys.pos.X < hmn_data->phys.pos.X ? 0 : 1;

        stc_tdi_vals[stc_tdi_val_num++] = (CustomTDI) {
            .lstickX = pad->fstickX,
            .lstickY = pad->fstickY,
            .cstickX = pad->fsubstickX,
            .cstickY = pad->fsubstickY,
            .reversing = false,
            .direction = direction,
        };
        SFX_PlayCommon(1);
    }

    if ((inputs & HSD_TRIGGER_Z) != 0 && stc_tdi_val_num > 0) {
        stc_tdi_vals[stc_tdi_val_num-1].reversing ^= 1;
        SFX_PlayCommon(1);
    }

    // if press X, go back a hit
    if ((inputs & HSD_BUTTON_X) != 0 && stc_tdi_val_num > 0) {
        stc_tdi_val_num--;
        SFX_PlayCommon(0);
    }

    // if press B, exit
    if ((inputs & HSD_BUTTON_B) != 0)
    {
        CustomTDI_Destroy(gobj);
        return;
    }

    // update curr lstick
    JOBJ *stick_curr = tdi_data->stick_curr[0];
    stick_curr->rot.Y = pad->fstickX * 0.75;
    stick_curr->rot.X = pad->fstickY * 0.75 * -1;
    // update curr cstick
    stick_curr = tdi_data->stick_curr[1];
    stick_curr->rot.Y = pad->fsubstickX * 0.75;
    stick_curr->rot.X = pad->fsubstickY * 0.75 * -1;

    // Update curr stick coordinates
    Text *text_curr = tdi_data->text_curr;
    Text_SetText(text_curr, 0, "TDI: %d", stc_tdi_val_num + 1);
    Text_SetText(text_curr, 1, "X: %+.4f", pad->fstickX);
    Text_SetText(text_curr, 2, "Y: %+.4f", pad->fstickY);
    Text_SetText(text_curr, 3, "X: %+.4f", pad->fsubstickX);
    Text_SetText(text_curr, 4, "Y: %+.4f", pad->fsubstickY);

    // display previous sticks
    for (int i = 0; i < TDI_DISPNUM; i++)
    {
        JOBJ *lstick_prev = tdi_data->stick_prev[i][0];
        JOBJ *cstick_prev = tdi_data->stick_prev[i][1];
        int this_hit = i;
        if (stc_tdi_val_num > TDI_DISPNUM)
            this_hit = (stc_tdi_val_num - TDI_DISPNUM + i);

        // show stick
        if (i < stc_tdi_val_num)
        {
            // remove hidden flag
            JOBJ_ClearFlags(lstick_prev, JOBJ_HIDDEN);
            JOBJ_ClearFlags(cstick_prev, JOBJ_HIDDEN);

            // update rotation
            CustomTDI di = stc_tdi_vals[this_hit];
            lstick_prev->rot.Y = di.lstickX * 0.75;
            lstick_prev->rot.X = di.lstickY * -0.75;
            cstick_prev->rot.Y = di.cstickX * 0.75;
            cstick_prev->rot.X = di.cstickY * -0.75;

            // update text
            const char *str = di.reversing ? "TDI %d R" : "TDI %d";
            Text_SetText(text_curr, i + 5, str, this_hit + 1);
        }
        // hide stick
        else
        {
            // set hidden flag
            JOBJ_SetFlags(lstick_prev, JOBJ_HIDDEN);
            JOBJ_SetFlags(cstick_prev, JOBJ_HIDDEN);

            Text_SetText(text_curr, i + 5, nullString);
        }
    }

    // update jobj
    JOBJ_SetMtxDirtySub(gobj->hsd_object);

    return;
}
void CustomTDI_Destroy(GOBJ *gobj)
{
    // get data
    TDIData *tdi_data = gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;
    LabData *event_data = event_vars->event_gobj->userdata;

    // set TDI to custom
    if (stc_tdi_val_num > 0) {
        int prev = LabOptions_CPU[OPTCPU_TDI].option_val;
        if (prev != CPUTDI_CUSTOM && prev != CPUTDI_RANDOM_CUSTOM)
            LabOptions_CPU[OPTCPU_TDI].option_val = CPUTDI_CUSTOM;
    }

    // free text
    Text_Destroy(tdi_data->text_curr);

    // destroy
    GObj_Destroy(gobj);

    // show menu
    event_vars->hide_menu = 0;
    menu_data->custom_gobj = 0;
    menu_data->custom_gobj_think = 0;
    menu_data->custom_gobj_destroy = 0;

    // play sfx
    SFX_PlayCommon(0);

    return;
}
void Inputs_Think(GOBJ *gobj)
{
    Controller *controllers = gobj->userdata;
    bool show[4] = {false};

    if (Pause_CheckStatus(1) != 2) {
        switch (LabOptions_General[OPTGEN_INPUT].option_val) {
        case INPUTDISPLAY_OFF:
            break;
        case INPUTDISPLAY_HMN:
            show[0] = true;
            break;
        case INPUTDISPLAY_CPU:
            show[1] = true;
            break;
        case INPUTDISPLAY_HMN_AND_CPU:
            show[0] = true;
            show[1] = true;
            break;
        }
    }

    bool input_display = false;
    for (int i = 0; i < 4; i++) input_display |= show[i];
    if (!input_display) {
        Match_ShowTimer();
    }

    // update controllers
    for (int i = 0; i < 4; i++)
    {
        Controller *controller = &controllers[i];
        JOBJ *controller_jobj = controller->jobj;

        if (controller_jobj == NULL) continue;
        if (!show[i]) {
            // Aitch: I would LOVE to set the hidden flag instead of abusing the Z axis.
            // But for some reason it doesn't work! Lmk if you have a better idea.

            controller_jobj->trans.Z = -99999.0;
            //JOBJ_SetFlags(controller_jobj, JOBJ_HIDDEN);

            JOBJ_SetMtxDirtySub(controller_jobj);
            continue;
        }

        controller_jobj->trans.Z = 0.0;

        // get port and controller data
        GOBJ *fighter = Fighter_GetGObj(i);
        FighterData *ft_data = fighter->userdata;
        HSD_Pad *pad = PadGet(ft_data->pad_index, PADGET_ENGINE);

        // move lstick
        JOBJ *lstick_joint;
        JOBJ_GetChild(controller_jobj, &lstick_joint, 10, -1);
        lstick_joint->trans.X = (pad->fstickX * 2.3);
        lstick_joint->trans.Y = (pad->fstickY * 2.3);

        // move lstick
        JOBJ *rstick_joint;
        JOBJ_GetChild(controller_jobj, &rstick_joint, 8, -1);
        rstick_joint->trans.X = (pad->fsubstickX * 2.3);
        rstick_joint->trans.Y = (pad->fsubstickY * 2.3);

        // move ltrigger
        JOBJ *ltrig_joint;
        JOBJ_GetChild(controller_jobj, &ltrig_joint, button_lookup[BTN_L].jobj, -1);
        ltrig_joint->trans.X = (pad->ftriggerLeft * 0.5) + controller->ltrig_origin.X;
        ltrig_joint->trans.Z = (pad->ftriggerLeft * 1.5) + controller->ltrig_origin.Y;

        // move rtrigger
        JOBJ *rtrig_joint;
        JOBJ_GetChild(controller_jobj, &rtrig_joint, button_lookup[BTN_R].jobj, -1);
        rtrig_joint->trans.X = (pad->ftriggerRight * -0.5) + controller->rtrig_origin.X;
        rtrig_joint->trans.Z = (pad->ftriggerRight *  1.5) + controller->rtrig_origin.Y;

        // update button colors
        int held = pad->held;
        for (int i = 0; i < BTN_NUM; i++)
        {
            // Get buttons jobj and dobj from the lookup table
            JOBJ *button_jobj;
            JOBJ_GetChild(controller_jobj, &button_jobj, button_lookup[i].jobj, -1);
            DOBJ *button_dobj = JOBJ_GetDObjChild(button_jobj, button_lookup[i].dobj);

            // check if button is pressed
            if (held & button_bits[i])
            {
                // make white i guess for now
                GXColor color_pressed = INPUT_COLOR_PRESSED;
                button_dobj->mobj->mat->diffuse = color_pressed;
            }
            // not pressed, make the default color
            else
            {
                GXColor *color_released = &button_colors[i];
                button_dobj->mobj->mat->diffuse = *color_released;
            }
        }

        JOBJ_SetMtxDirtySub(controller_jobj);
    }
}

void Inputs_Init()
{
    // Create Input Display GOBJ
    GOBJ *input_gobj = GObj_Create(0, 0, 0);
    Controller *controllers = calloc(sizeof(Controller) * 4);
    GObj_AddUserData(input_gobj, 4, HSD_Free, controllers);
    GObj_AddProc(input_gobj, Inputs_Think, 4);

    // alloc a dummy root jobj
    JOBJDesc *root_desc = calloc(sizeof(JOBJDesc));
    root_desc->flags = JOBJ_ROOT_XLU | JOBJ_ROOT_TEXEDGE;
    root_desc->scale.X = 1;
    root_desc->scale.Y = 1;
    root_desc->scale.Z = 1;
    JOBJ *root = JOBJ_LoadJoint(root_desc);

    static Vec2 stc_pos[4] = {
        {5, 20},
        {15, 20},
        {5, 10},
        {15, 10},
    };

    int controller_count = 0;
    for (int i = 0; i < 4; i++)
    {
        int slot = Fighter_GetSlotType(i);
        if (slot != 0 && slot != 1)
            continue;

        Vec2 pos = stc_pos[controller_count++];

        Controller *controller = &controllers[i];

        // create jobj
        JOBJ *controller_jobj = JOBJ_LoadJoint(stc_lab_data->controller);
        controller->jobj = controller_jobj;
        controller_jobj->trans.X = pos.X;
        controller_jobj->trans.Y = pos.Y;
        JOBJ_AddChild(root, controller_jobj);

        // save trigger origins
        JOBJ *ltrig_jobj, *rtrig_jobj;
        JOBJ_GetChild(controller_jobj, &ltrig_jobj, button_lookup[BTN_L].jobj, -1);
        JOBJ_GetChild(controller_jobj, &rtrig_jobj, button_lookup[BTN_R].jobj, -1);
        controller->ltrig_origin.X = ltrig_jobj->trans.X;
        controller->ltrig_origin.Y = ltrig_jobj->trans.Z;
        controller->rtrig_origin.X = rtrig_jobj->trans.X;
        controller->rtrig_origin.Y = rtrig_jobj->trans.Z;
    }

    GObj_AddObject(input_gobj, 3, root);                              // add to gobj
    GObj_AddGXLink(input_gobj, GXLink_Common, INPUT_GXLINK, INPUT_GXPRI); // add gx link

    return;
}

// Recording Functions
GOBJ *Record_Init()
{
    // Create GOBJ
    GOBJ *rec_gobj = GObj_Create(0, 7, 0);
    // Add per frame process
    GObj_AddProc(rec_gobj, Record_Think, 3);

    // create cobj
    GOBJ *cam_gobj = GObj_Create(19, 20, 0);
    COBJDesc ***dmgScnMdls = Archive_GetPublicAddress(*stc_ifall_archive, 0x803f94d0);
    COBJDesc *cam_desc = dmgScnMdls[1][0];
    COBJ *rec_cobj = COBJ_LoadDesc(cam_desc);
    // init camera
    GObj_AddObject(cam_gobj, R13_U8(-0x3E55), rec_cobj);
    GOBJ_InitCamera(cam_gobj, Record_CObjThink, RECCAM_GXPRI);
    cam_gobj->cobj_links = RECCAM_COBJGXLINK;

    evMenu *menuAssets = event_vars->menu_assets;
    JOBJ *playback = JOBJ_LoadJoint(menuAssets->playback);

    // save seek jobj
    JOBJ *seek;
    JOBJ_GetChild(playback, &seek, REC_SEEKJOINT, -1);
    rec_data.seek_jobj = seek;

    rec_data.restore_timer = 0;

    // save left and right seek bounds
    JOBJ *seek_bound[2];
    JOBJ_GetChild(playback, &seek_bound, REC_LEFTBOUNDJOINT, REC_RIGHTBOUNDJOINT, -1);
    Vec3 seek_bound_pos;
    JOBJ_GetWorldPosition(seek_bound[0], 0, &seek_bound_pos);
    rec_data.seek_left = seek_bound_pos.X;
    JOBJ_GetWorldPosition(seek_bound[1], 0, &seek_bound_pos);
    rec_data.seek_right = seek_bound_pos.X;

    // Add to gobj
    GObj_AddObject(rec_gobj, 3, playback);
    // Add gxlink
    GObj_AddGXLink(rec_gobj, Record_GX, REC_GXLINK, GXPRI_RECJOINT);

    // Create text
    int canvas_index = Text_CreateCanvas(2, rec_gobj, 14, 15, 0, REC_GXLINK, GXPRI_RECTEXT, 19);
    Text *text = Text_CreateText(2, canvas_index);
    text->align = 1;
    text->kerning = 1;
    text->viewport_scale.X = REC_TEXTSCALE;
    text->viewport_scale.Y = REC_TEXTSCALE;

    // Get text positions
    JOBJ *text_joint[2];
    JOBJ_GetChild(playback, &text_joint, REC_LEFTTEXTJOINT, REC_RIGHTTEXTJOINT, -1);
    Vec3 text_left, text_right;
    JOBJ_GetWorldPosition(text_joint[0], 0, &text_left);
    JOBJ_GetWorldPosition(text_joint[1], 0, &text_right);

    // Create subtexts for each side
    Text_AddSubtext(text, (text_left.X * 25), -(text_left.Y * 25), &nullString);
    Text_AddSubtext(text, (text_right.X * 25), -(text_right.Y) * 25, &nullString);
    rec_data.text = text;

    // alloc rec_state
    rec_state = calloc(sizeof(Savestate));
    // set as not exist
    rec_state->is_exist = 0;

    // disable menu options
    for (int i = 0; i < sizeof(LabOptions_Record) / sizeof(EventOption); i++)
    {
        if (i != OPTREC_SAVE_LOAD)
            LabOptions_Record[i].disable = 1;
    }

    // allocate input arrays
    for (int i = 0; i < REC_SLOTS; i++)
    {
        rec_data.hmn_inputs[i] = calloc(sizeof(RecInputData));
        rec_data.hmn_inputs[i]->start_frame = -1;
        rec_data.hmn_inputs[i]->num = 0;

        rec_data.cpu_inputs[i] = calloc(sizeof(RecInputData));
        rec_data.cpu_inputs[i]->start_frame = -1;
        rec_data.cpu_inputs[i]->num = 0;
    }

    // init memcard stuff
    Memcard_InitWorkArea();
    Memcard_InitSnapshotList(HSD_MemAlloc(2112), HSD_MemAlloc(256064));

    // Create snapshot cam
    snap_image.img_ptr = 0;
    GOBJ *snap_gobj = GObj_Create(18, 18, 0);
    GOBJ_InitCamera(snap_gobj, Snap_CObjThink, 4);
    GX_AllocImageData(&snap_image, EXP_SCREENSHOT_WIDTH, EXP_SCREENSHOT_HEIGHT, 4, 2006);
    export_status = EXSTAT_NONE;

    /*
    // init dev text
    int height = 18;
    int width = 28;
    int x = -50;
    int y = 410;
    DevText *dev_text = DevelopText_CreateDataTable(x, y, 0, width, height, HSD_MemAlloc(height * width * 2));
    stc_devtext = dev_text;
    DevelopText_Activate(0, dev_text);
    dev_text->cursorBlink = 0;
    GXColor color = {21, 20, 59, 135};
    DevelopText_StoreBGColor(dev_text, &color);
    DevelopText_StoreTextScale(dev_text, 7.5, 10);
    */
    return rec_gobj;
}
void Record_CObjThink(GOBJ *gobj)
{
    // hide UI if set to off
    if ((rec_state->is_exist == 1) && ((LabOptions_Record[OPTREC_CPUMODE].option_val != 0) || (LabOptions_Record[OPTREC_HMNMODE].option_val != 0)))
    {
        CObjThink_Common(gobj);
    }

    return;
}
void Record_GX(GOBJ *gobj, int pass)
{
    // update UI position
    // the reason im doing this here is because i want it to update in the menu
    if (pass == 0)
    {
        JOBJ *seek = rec_data.seek_jobj;
        Text *text = rec_data.text;

        // get curr frame (the current position in the recording)
        int curr_frame = Record_GetCurrFrame();
        int end_frame = Record_GetEndFrame();

        // hide seek bar during recording
        if ((LabOptions_Record[OPTREC_CPUMODE].option_val == 2) || (LabOptions_Record[OPTREC_HMNMODE].option_val == 1))
        {
            JOBJ_SetFlags(seek, JOBJ_HIDDEN);

            // correct record frame
            if (curr_frame >= REC_LENGTH)
                curr_frame = REC_LENGTH;

            // update seek bar frames
            Text_SetText(text, 0, "%d", curr_frame + 1);
            Text_SetText(text, 1, &nullString);

            // update color
            GXColor text_color;
            if (curr_frame == REC_LENGTH)
            {
                text_color.r = 255;
                text_color.g = 57;
                text_color.b = 62;
            }
            else if (((float)curr_frame / (float)REC_LENGTH) >= 0.75)
            {
                text_color.r = 255;
                text_color.g = 124;
                text_color.b = 36;
            }
            else
            {
                text_color.r = 255;
                text_color.g = 255;
                text_color.b = 255;
            }
            Text_SetColor(text, 0, &text_color);
        }
        // during playback
        else
        {
            JOBJ_ClearFlags(seek, JOBJ_HIDDEN);

            // if playing back with no recording, adjust both numbers
            int local_frame_seek = curr_frame + 1;
            if (curr_frame >= end_frame)
            {
                local_frame_seek = curr_frame + 1;
                end_frame = curr_frame + 1;
            }

            // update seek bar position
            float range = rec_data.seek_right - rec_data.seek_left;
            float curr_pos;
            curr_pos = (float)local_frame_seek / (float)end_frame;
            seek->trans.X = rec_data.seek_left + (curr_pos * range);
            JOBJ_ClearFlagsAll(seek, JOBJ_HIDDEN);
            JOBJ_SetMtxDirtySub(seek);

            // update seek bar frames
            Text_SetText(text, 0, "%d", local_frame_seek);
            Text_SetText(text, 1, "%d", end_frame);

            // if random playback, hide frame count and bar
            if ((LabOptions_Record[OPTREC_CPUSLOT].option_val == 0) || (LabOptions_Record[OPTREC_HMNSLOT].option_val == 0))
            {
                Text_SetText(text, 1, "?");          // hide count
                JOBJ_SetFlagsAll(seek, JOBJ_HIDDEN); // hide bar
            }

            // update color
            GXColor text_color;
            text_color.r = 255;
            text_color.g = 255;
            text_color.b = 255;
            Text_SetColor(text, 0, &text_color);
        }
    }

    GXLink_Common(gobj, pass);

    return;
}
void Record_Think(GOBJ *rec_gobj)
{
    if (rec_state->is_exist != 1) return;

    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    int hmn_slot = Record_GetSlot(0);
    int cpu_slot = Record_GetSlot(1);

    RecInputData *hmn_inputs = rec_data.hmn_inputs[hmn_slot];
    RecInputData *cpu_inputs = rec_data.cpu_inputs[cpu_slot];
    int hmn_mode = LabOptions_Record[OPTREC_HMNMODE].option_val;
    int cpu_mode = LabOptions_Record[OPTREC_CPUMODE].option_val;

    // get longest recording
    int input_num = hmn_inputs->num;
    if (cpu_inputs->num > hmn_inputs->num)
        input_num = cpu_inputs->num;

    // get curr frame (the current position in the recording)
    int curr_frame = Record_GetCurrFrame();
    int end_frame = Record_GetEndFrame();

    // update inputs ---------------------------------------

    int adjusted_hmn_mode = hmn_mode;
    if (adjusted_hmn_mode > 0) // adjust hmn_mode to match cpu_mode
        adjusted_hmn_mode++;
    Record_Update(0, hmn_inputs, adjusted_hmn_mode);
    Record_Update(1, cpu_inputs, cpu_mode);

    // loop inputs check ------------------------------

    int loop_mode = LabOptions_Record[OPTREC_LOOP].option_val;
    int modes_allow_loop = hmn_mode != RECMODE_HMN_RECORD && cpu_mode != RECMODE_CPU_CONTROL && cpu_mode != RECMODE_CPU_RECORD;
    int has_inputs = input_num != 0;
    int past_last_input = Record_GetCurrFrame() >= input_num;
    if (loop_mode & modes_allow_loop & has_inputs & past_last_input)
    {
        event_vars->game_timer = rec_state->frame;

        // reroll
        if (LabOptions_Record[OPTREC_HMNSLOT].option_val == 0)
            rec_data.hmn_rndm_slot = Record_GetRandomSlot(&rec_data.hmn_inputs, LabOptions_SlotChancesHMN);
        if (LabOptions_Record[OPTREC_CPUSLOT].option_val == 0)
            rec_data.cpu_rndm_slot = Record_GetRandomSlot(&rec_data.cpu_inputs, LabOptions_SlotChancesCPU);
    }

    // autorestore check -----------------------------------

    if (modes_allow_loop) {
        int autorestore_mode = LabOptions_Record[OPTREC_AUTORESTORE].option_val;
        int restore = false;
        LabData *event_data = event_vars->event_gobj->userdata;
        switch (autorestore_mode)
        {
            case (AUTORESTORE_NONE):
                break;
            case (AUTORESTORE_PLAYBACK_END):
            {
                restore = past_last_input & has_inputs;
                break;
            }
            case (AUTORESTORE_COUNTER):
            {
                int state = cpu_data->state_id;
                restore = event_data->cpu_countering
                    || (ASID_DEADDOWN <= state && state <= ASID_DEADUPFALLHITCAMERAICE)
                    // Aitch: temporary hack to reset before we add Counter Action (Ledge) options.
                    || state == ASID_CLIFFWAIT;
                break;
            }
        }

        // start restore timer on restore, and increment it if it has already started
        if (restore || rec_data.restore_timer > 0)
            rec_data.restore_timer++;

        if (rec_data.restore_timer >= AUTORESTORE_DELAY)
            Record_LoadSavestate(rec_state);
    }
}

int Record_RearrangeButtons(RecInputs *inputs) {
    int held = 0;
    held |= inputs->btn_a << 8;
    held |= inputs->btn_b << 9;
    held |= inputs->btn_x << 10;
    held |= inputs->btn_y << 11;
    held |= inputs->btn_L << 6;
    held |= inputs->btn_R << 5;
    held |= inputs->btn_Z << 4;
    held |= inputs->btn_dpadup << 3;
    return held;
}

void Record_SetInputs(GOBJ *fighter, RecInputs *inputs, bool mirror) {
    FighterData *fighter_data = fighter->userdata;
    HSD_Pad *pad = PadGet(fighter_data->pad_index, PADGET_ENGINE);

    // read inputs
    pad->held = Record_RearrangeButtons(inputs);

    // stick signed bytes
    pad->stickX = inputs->stickX * (mirror ? -1 : 1);
    pad->stickY = inputs->stickY;
    pad->substickX = inputs->substickX * (mirror ? -1 : 1);
    pad->substickY = inputs->substickY;

    // stick floats
    pad->fstickX = ((float)inputs->stickX / 80) * (mirror ? -1 : 1);
    pad->fstickY = ((float)inputs->stickY / 80);
    pad->fsubstickX = ((float)inputs->substickX / 80) * (mirror ? -1 : 1);
    pad->fsubstickY = ((float)inputs->substickY / 80);

    // trigger byte
    pad->triggerRight = inputs->trigger;
    pad->triggerLeft = 0;

    // trigger float
    pad->ftriggerRight = ((float)inputs->trigger / 140);
    pad->ftriggerLeft = 0;
}

// assumes rec_mode_cpu
void Record_Update(int ply, RecInputData *input_data, int rec_mode)
{
    GOBJ *fighter = Fighter_GetGObj(ply);
    FighterData *fighter_data = fighter->userdata;

    // get curr frame (the time since saving positions)
    int curr_frame = (event_vars->game_timer - rec_state->frame);

    // get the frame the recording starts on. i actually hate this code and need to change how this works
    int rec_start;
    if (input_data->start_frame == -1) // case 1: recording didnt start, use current frame
    {
        rec_start = curr_frame - 1;
    }
    else // case 2: recording has started, use the frame saved
    {
        rec_start = input_data->start_frame - rec_state->frame;
    }
    int end_frame = rec_start + input_data->num;

    // Get HSD Pad
    HSD_Pad *pad = PadGet(fighter_data->pad_index, PADGET_ENGINE);

    // if the current frame before the recording ends
    if ((curr_frame) < (rec_start + REC_LENGTH))
    {
        switch (rec_mode)
        {
        case (RECMODE_CPU_OFF):
        case (RECMODE_CPU_CONTROL):
            break;
        case (RECMODE_CPU_RECORD):
        {
            // recording has started BUT the player has jumped back behind it, move the start frame back
            if ((input_data->start_frame == -1) || (curr_frame < rec_start))
            {
                input_data->start_frame = (event_vars->game_timer - 1);
                rec_start = curr_frame - 1;
            }

            // store inputs
            int held = pad->held;
            RecInputs *inputs = &input_data->inputs[curr_frame - 1];
            inputs->btn_a = !!((held)&HSD_BUTTON_A);
            inputs->btn_b = !!((held)&HSD_BUTTON_B);
            inputs->btn_x = !!((held)&HSD_BUTTON_X);
            inputs->btn_y = !!((held)&HSD_BUTTON_Y);
            inputs->btn_L = !!((held)&HSD_TRIGGER_L);
            inputs->btn_R = !!((held)&HSD_TRIGGER_R);
            inputs->btn_Z = !!((held)&HSD_TRIGGER_Z);
            inputs->btn_dpadup = !!((held)&HSD_BUTTON_DPAD_UP);

            inputs->stickX = pad->stickX;
            inputs->stickY = pad->stickY;
            inputs->substickX = pad->substickX;
            inputs->substickY = pad->substickY;

            // trigger - find the one pressed down more
            u8 trigger = pad->triggerLeft;
            if (pad->triggerRight > trigger)
                trigger = pad->triggerRight;
            inputs->trigger = trigger;

            // update input_num
            input_data->num = (curr_frame - rec_start);

            // clear inputs henceforth
            //memset(&input_data->inputs[curr_frame + 1], 0, (REC_LENGTH - curr_frame) * sizeof(RecInputs));

            break;
        }
        case (RECMODE_CPU_PLAYBACK):
        {
            // ensure we haven't taken over playback or set the cpu to counter
            if (stc_playback_cancelled_hmn && ply == 0)
                return;
            if (stc_playback_cancelled_cpu && ply == 1)
                return;

            // ensure we have an input for this frame
            if (curr_frame < rec_start || (curr_frame - rec_start) > (input_data->num))
                return;

            RecInputs *inputs = &input_data->inputs[curr_frame - 1];
            Record_SetInputs(fighter, inputs, event_vars->loaded_mirrored);
            break;
        }
        }
    }

    return;
}
void Record_InitState(GOBJ *menu_gobj)
{
    stc_playback_cancelled_hmn = false;
    stc_playback_cancelled_cpu = false;
    if (event_vars->Savestate_Save(rec_state))
        Record_OnSuccessfulSave(1);
}
void Record_ResaveState(GOBJ *menu_gobj)
{
    int prev_savestate_frame = rec_state->frame;
    Record_PruneState(menu_gobj);
    int new_savestate_frame = rec_state->frame;
    int savestate_diff = new_savestate_frame - prev_savestate_frame;

    // change start frame
    for (int ply = 0; ply < 2; ++ply) {
        for (int slot = 0; slot < REC_SLOTS; ++slot) {
            RecInputData *input_data;
            if (ply == 0)
                input_data = rec_data.hmn_inputs[slot];
            else
                input_data = rec_data.cpu_inputs[slot];

            if (input_data->start_frame != -1)
                input_data->start_frame += savestate_diff;
        }
    }
}
void Record_PruneState(GOBJ *menu_gobj)
{
    stc_playback_cancelled_hmn = false;
    stc_playback_cancelled_cpu = false;
    if (event_vars->Savestate_Save(rec_state))
        Record_OnSuccessfulSave(0);

    // If we re-save during mirroring, then we NEED to show the new savestate as unmirrored.
    // That's just how savestates work. So to use old, unmirrored inputs, we need to reverse them in place.
    if (event_vars->loaded_mirrored) {
        // need to disable mirroring, as the new savestate is inherently unmirrored.
        event_vars->loaded_mirrored = false;
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].option_val = OPTMIRROR_OFF;
        LabOptions_Record[OPTREC_HMNMODE].disable = 0;
        LabOptions_Record[OPTREC_CPUMODE].disable = 0;

        for (int ply = 0; ply < 2; ++ply) {
            for (int slot = 0; slot < REC_SLOTS; ++slot) {
                RecInputData *input_data;
                if (ply == 0)
                    input_data = rec_data.hmn_inputs[slot];
                else
                    input_data = rec_data.cpu_inputs[slot];

                int input_num = input_data->num;
                for (int input = 0; input <= input_num; ++input) {
                    RecInputs *inputs = &input_data->inputs[input];
                    inputs->stickX *= -1;
                    inputs->substickX *= -1;
                }
            }
        }
    }
}
void Record_DeleteState(GOBJ *menu_gobj)
{
    stc_playback_cancelled_hmn = false;
    stc_playback_cancelled_cpu = false;
    for (int i = 0; i < sizeof(LabOptions_Record) / sizeof(EventOption); i++)
    {
        if (i == OPTREC_SAVE_LOAD) {
            LabOptions_Record[i] = Record_Save;
            LabOptions_Record[i].disable = 0;
        } else {
            LabOptions_Record[i].option_val = (i == OPTREC_HMNSLOT || i == OPTREC_CPUSLOT) ? 1 : 0;
            LabOptions_Record[i].disable = 1;
        }
    }

    for (int i = 0; i < REC_SLOTS; ++i) {
        LabOptions_SlotChancesHMN[i].option_val = 0;
        LabOptions_SlotChancesCPU[i].option_val = 0;
        LabOptions_SlotChancesHMN[i].disable = 1;
        LabOptions_SlotChancesCPU[i].disable = 1;

        rec_data.hmn_inputs[i]->start_frame = -1;
        rec_data.hmn_inputs[i]->num = 0;
        rec_data.cpu_inputs[i]->start_frame = -1;
        rec_data.cpu_inputs[i]->num = 0;
    }

    LabMenu_Record.scroll = 0;
    LabMenu_Record.cursor = 0;
}
void Record_RestoreState(GOBJ *menu_gobj)
{
    Record_LoadSavestate(rec_state);
}
void Record_ChangeHMNSlot(GOBJ *menu_gobj, int value)
{
    // upon changing to random
    if (value == 0)
    {
        if (LabOptions_Record[OPTREC_HMNMODE].option_val == RECMODE_HMN_RECORD)
            LabOptions_Record[OPTREC_HMNSLOT].option_val = 1;
        else
            rec_data.hmn_rndm_slot = Record_GetRandomSlot(&rec_data.hmn_inputs, LabOptions_SlotChancesHMN);
    }

    // reload save
    Record_LoadSavestate(rec_state);
}
void Record_ChangeCPUSlot(GOBJ *menu_gobj, int value)
{
    // upon changing to random
    if (value == 0)
    {
        if (LabOptions_Record[OPTREC_CPUMODE].option_val == RECMODE_CPU_RECORD)
            LabOptions_Record[OPTREC_CPUSLOT].option_val = 1;
        else
            rec_data.cpu_rndm_slot = Record_GetRandomSlot(&rec_data.cpu_inputs, LabOptions_SlotChancesCPU);
    }

    // reload save
    Record_LoadSavestate(rec_state);
}
void Record_ChangeHMNMode(GOBJ *menu_gobj, int value)
{
    if (value == RECMODE_HMN_RECORD)
    {
        // if set to random
        if (LabOptions_Record[OPTREC_HMNSLOT].option_val == 0)
        {
            LabOptions_Record[OPTREC_HMNSLOT].option_val = 1;
        }
    }

    if (value == RECMODE_HMN_PLAYBACK)
        Record_LoadSavestate(rec_state);

    // disable some options if recording is in use
    if (LabOptions_Record[OPTREC_HMNMODE].option_val == RECMODE_HMN_RECORD ||
        LabOptions_Record[OPTREC_CPUMODE].option_val == RECMODE_CPU_RECORD)
    {
        LabOptions_Record[OPTREC_LOOP].option_val = 0;
        LabOptions_Record[OPTREC_LOOP].disable = 1;
        LabOptions_Record[OPTREC_AUTORESTORE].option_val = AUTORESTORE_NONE;
        LabOptions_Record[OPTREC_AUTORESTORE].disable = 1;
    }
    else
    {
        LabOptions_Record[OPTREC_LOOP].disable = 0;
        LabOptions_Record[OPTREC_AUTORESTORE].disable = 0;
    }

    // Mirrored Playback is only available in playing back and not in recording
    if ((value == RECMODE_HMN_PLAYBACK || LabOptions_Record[OPTREC_CPUMODE].option_val == RECMODE_CPU_PLAYBACK) &&
        (value != RECMODE_HMN_RECORD && LabOptions_Record[OPTREC_CPUMODE].option_val != RECMODE_CPU_RECORD))
    {
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].disable = 0;
    }
    else
    {
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].disable = 1;
    }
}
void Record_ChangeCPUMode(GOBJ *menu_gobj, int value)
{
    if (value == RECMODE_CPU_RECORD)
    {
        // if set to random
        if (LabOptions_Record[OPTREC_CPUSLOT].option_val == 0)
        {
            // change to slot 1
            LabOptions_Record[OPTREC_CPUSLOT].option_val = 1;
        }
    }

    if (value == RECMODE_CPU_PLAYBACK)
        Record_LoadSavestate(rec_state);

    // disable some options if recording is in use
    if (LabOptions_Record[OPTREC_HMNMODE].option_val == RECMODE_HMN_RECORD ||
        LabOptions_Record[OPTREC_CPUMODE].option_val == RECMODE_CPU_RECORD)
    {
        LabOptions_Record[OPTREC_LOOP].option_val = 0;
        LabOptions_Record[OPTREC_LOOP].disable = 1;
        LabOptions_Record[OPTREC_AUTORESTORE].option_val = AUTORESTORE_NONE;
        LabOptions_Record[OPTREC_AUTORESTORE].disable = 1;
    }
    else
    {
        LabOptions_Record[OPTREC_LOOP].disable = 0;
        LabOptions_Record[OPTREC_AUTORESTORE].disable = 0;
    }

    // Mirrored Playback is only available in playing back and not in recording
    if ((value == RECMODE_CPU_PLAYBACK || LabOptions_Record[OPTREC_HMNMODE].option_val == RECMODE_HMN_PLAYBACK) &&
        (value != RECMODE_CPU_RECORD && LabOptions_Record[OPTREC_HMNMODE].option_val != RECMODE_HMN_RECORD))
    {
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].disable = 0;
    }
    else
    {
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].disable = 1;
    }
}
void Record_ChangeMirroredPlayback(GOBJ *menu_gobj, int value)
{
    // Cannot change HMN/CPU Mode while mirrored playback is on.
    if (value != OPTMIRROR_OFF)
    {
        LabOptions_Record[OPTREC_HMNMODE].disable = 1;
        LabOptions_Record[OPTREC_CPUMODE].disable = 1;
    }
    else
    {
        LabOptions_Record[OPTREC_HMNMODE].disable = 0;
        LabOptions_Record[OPTREC_CPUMODE].disable = 0;
    }

    Record_LoadSavestate(rec_state);
}
int Record_GetRandomSlot(RecInputData **input_data, EventOption slot_menu[])
{
    u16 *chances[REC_SLOTS];
    int chance_count = enabled_slot_chances(slot_menu, chances);
    if (chance_count == 0) return 0;

    int slots[REC_SLOTS];
    int slot_num = 0;
    for (int i = 0; i < REC_SLOTS; ++i) {
        if (!slot_menu[i].disable)
            slots[slot_num++] = i;
    }

    int rand = HSD_Randi(100);

    for (int i = 0; i < chance_count; ++i) {
        int chance = (int)*chances[i];
        if (rand < chance)
            return slots[i];
        rand -= chance;
    }

    return 0;
}
int Record_GetSlot(int ply) {
    int slot;
    int random_slot;

    if (ply == 0) {
        slot = LabOptions_Record[OPTREC_HMNSLOT].option_val;
        random_slot = rec_data.hmn_rndm_slot;
    } else {
        slot = LabOptions_Record[OPTREC_CPUSLOT].option_val;
        random_slot = rec_data.cpu_rndm_slot;
    }

    if (slot == 0)
        slot = random_slot;
    else
        slot--;

    return slot;
}
int Record_PastLastInput(int ply)
{
    int slot = Record_GetSlot(ply);
    int curr_frame = Record_GetCurrFrame();
    RecInputData *inputs;
    if (ply == 0)
        inputs = rec_data.hmn_inputs[slot];
    else
        inputs = rec_data.cpu_inputs[slot];

    return curr_frame > inputs->num;
}
int Record_GetCurrFrame()
{
    return (event_vars->game_timer - 1) - rec_state->frame;
}
int Record_GetEndFrame()
{
    int hmn_slot = Record_GetSlot(0);
    int cpu_slot = Record_GetSlot(1);
    int curr_frame = Record_GetCurrFrame();

    RecInputData *hmn_inputs = rec_data.hmn_inputs[hmn_slot];
    RecInputData *cpu_inputs = rec_data.cpu_inputs[cpu_slot];

    // get what frame the longest recording ends on (savestate frame + recording start frame + recording time)
    int hmn_end_frame = 0;
    int cpu_end_frame = 0;
    if (hmn_inputs->start_frame != -1) // ensure a recording exists
        hmn_end_frame = (hmn_inputs->start_frame + hmn_inputs->num);
    if (cpu_inputs->start_frame != -1) // ensure a recording exists
        cpu_end_frame = (cpu_inputs->start_frame + cpu_inputs->num);

    // find the larger recording
    RecInputData *input_data = hmn_inputs;
    if (cpu_end_frame > hmn_end_frame)
        input_data = cpu_inputs;

    // get the frame the recording starts on. i actually hate this code and need to change how this works
    int rec_start;
    if (input_data->start_frame == -1) // case 1: recording didnt start, use current frame
        rec_start = curr_frame - 1;
    else // case 2: recording has started, use the frame saved
        rec_start = input_data->start_frame - rec_state->frame;

    // get end frame
    int end_frame = rec_start + input_data->num;

    return end_frame;
}
void Record_OnSuccessfulSave(int deleteRecordings)
{
    LabOptions_Record[OPTREC_SAVE_LOAD] = Record_Load;

    // enable other options
    for (int i = 0; i < sizeof(LabOptions_Record) / sizeof(EventOption); i++)
    {
        if (i == OPTREC_MIRRORED_PLAYBACK ||
            (i == OPTREC_HMNMODE && LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].option_val == 1) ||
            (i == OPTREC_CPUMODE && LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].option_val == 1))
          continue;

        LabOptions_Record[i].disable = 0;
    }

    if (deleteRecordings) {
        // clear slots
        for (int i = 0; i < REC_SLOTS; i++)
        {
            // clear data
            memset(rec_data.hmn_inputs[i], 0, sizeof(RecInputData));
            memset(rec_data.cpu_inputs[i], 0, sizeof(RecInputData));

            // init frame this recording starts on
            rec_data.hmn_inputs[i]->start_frame = -1;
            rec_data.cpu_inputs[i]->start_frame = -1;
        }

        // init settings
        LabOptions_Record[OPTREC_HMNMODE].option_val = RECMODE_HMN_OFF;
        LabOptions_Record[OPTREC_HMNSLOT].option_val = 1; // set hmn to slot 1
        LabOptions_Record[OPTREC_CPUMODE].option_val = RECMODE_CPU_OFF;
        LabOptions_Record[OPTREC_CPUSLOT].option_val = 1; // set cpu to slot 1
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].option_val = OPTMIRROR_OFF;
        LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].disable = 1;
    }

    // also save to personal savestate
    event_vars->Savestate_Save(event_vars->savestate);
    event_vars->savestate_saved_while_mirrored = event_vars->loaded_mirrored;

    // take screenshot
    snap_status = 1;

    return;
}
void Memcard_Wait()
{

    while (stc_memcard_work->is_done == 0)
    {
        blr2();
    }

    return;
}
void Record_MemcardLoad(int slot, int file_no)
{
    // search card for this save file
    u8 file_found = 0;
    char filename[32];
    int file_size;
    s32 memSize, sectorSize;
    if (CARDProbeEx(slot, &memSize, &sectorSize) == CARD_RESULT_READY)
    {
        // mount card
        stc_memcard_work->is_done = 0;
        if (CARDMountAsync(slot, stc_memcard_work->work_area, 0, Memcard_RemovedCallback) == CARD_RESULT_READY)
        {
            // check card
            Memcard_Wait();
            stc_memcard_work->is_done = 0;
            if (CARDCheckAsync(slot, Memcard_RemovedCallback) == CARD_RESULT_READY)
            {
                Memcard_Wait();

                CARDStat card_stat;
                if (CARDGetStatus(slot, file_no, &card_stat) == CARD_RESULT_READY)
                {
                    // check company code
                    if (strncmp(os_info->company, card_stat.company, sizeof(os_info->company)) == 0)
                    {
                        // check game name
                        if (strncmp(os_info->gameName, card_stat.gameName, sizeof(os_info->gameName)) == 0)
                        {
                            // check file name
                            if (strncmp("TMREC", card_stat.fileName, 5) == 0)
                            {
                                file_found = 1;
                                memcpy(&filename, card_stat.fileName, sizeof(filename)); // copy filename to load after this
                                file_size = card_stat.length;
                            }
                        }
                    }
                }
            }

            CARDUnmount(slot);
            stc_memcard_work->is_done = 0;
        }
    }

    // if found, load it
    if (file_found == 1)
    {

        int load_pre_tick = OSGetTick();

        // setup load
        MemcardSave memcard_save;
        memcard_save.data = HSD_MemAlloc(file_size);
        memcard_save.x4 = 3;
        memcard_save.size = file_size;
        memcard_save.xc = -1;
        Memcard_ReqSaveLoad(slot, filename, &memcard_save, &stc_memcard_info->file_name, 0, 0, 0);

        // wait to load
        int memcard_status = Memcard_CheckStatus();
        while (memcard_status == 11)
        {
            memcard_status = Memcard_CheckStatus();
        }

        // if file loaded successfully
        if (memcard_status == 0)
        {

            // enable other options
            for (int i = 1; i < sizeof(LabOptions_Record) / sizeof(EventOption); i++)
            {
                LabOptions_Record[i].disable = 0;
            }

            // take screenshot
            snap_status = 1;

            // begin unpacking
            u8 *transfer_buf = memcard_save.data;
            ExportHeader *header = transfer_buf;
            u8 *compressed_recording = transfer_buf + header->lookup.ofst_recording;
            RGB565 *img = transfer_buf + header->lookup.ofst_screenshot;
            ExportMenuSettings *menu_settings = transfer_buf + header->lookup.ofst_menusettings;

            // decompress
            RecordingSave *loaded_recsave = calloc(sizeof(RecordingSave) * 1.06);
            lz77_decompress(compressed_recording, loaded_recsave);

            // copy buffer to savestate
            memcpy(rec_state, &loaded_recsave->savestate, sizeof(Savestate));
            event_vars->savestate_saved_while_mirrored = false;
            event_vars->loaded_mirrored = false;

            // restore controller indices
            rec_state->ft_state[0].player_block.controller = stc_hmn_controller;
            rec_state->ft_state[1].player_block.controller = stc_cpu_controller;

            // load state
            Record_LoadSavestate(rec_state);

            // copy recordings
            for (int i = 0; i < REC_SLOTS; i++)
            {
                memcpy(rec_data.hmn_inputs[i], &loaded_recsave->hmn_inputs[i], sizeof(RecInputData));
                memcpy(rec_data.cpu_inputs[i], &loaded_recsave->cpu_inputs[i], sizeof(RecInputData));
            }

            HSD_Free(loaded_recsave);

            // copy recording settings
            LabOptions_Record[OPTREC_HMNMODE].option_val = menu_settings->hmn_mode;
            LabOptions_Record[OPTREC_HMNSLOT].option_val = menu_settings->hmn_slot;
            LabOptions_Record[OPTREC_CPUMODE].option_val = menu_settings->cpu_mode;
            LabOptions_Record[OPTREC_CPUSLOT].option_val = menu_settings->cpu_slot;
            LabOptions_Record[OPTREC_LOOP].option_val = menu_settings->loop_inputs;
            LabOptions_Record[OPTREC_AUTORESTORE].option_val = menu_settings->auto_restore;

            // enter recording menu
            MenuData *menu_data = event_vars->menu_gobj->userdata;
            EventMenu *curr_menu = menu_data->currMenu;
            curr_menu->state = EMSTATE_OPENSUB;
            // update curr_menu
            EventMenu *next_menu = curr_menu->options[2].menu;
            next_menu->prev = curr_menu;
            next_menu->state = EMSTATE_FOCUS;
            curr_menu = next_menu;
            menu_data->currMenu = curr_menu;

            // save to personal savestate
            event_vars->Savestate_Save(event_vars->savestate);
            event_vars->savestate_saved_while_mirrored = event_vars->loaded_mirrored;
        }

        HSD_Free(memcard_save.data);

        int load_post_tick = OSGetTick();
        int load_time = OSTicksToMilliseconds(load_post_tick - load_pre_tick);
    }

    return;
}
int Record_MenuThink(GOBJ *menu_gobj)
{

    int is_update = 1;

    // check to run export logic
    if (export_status != EXSTAT_NONE)
    {
        is_update = Record_ExportThink();
    }

    return is_update;
}
void Record_StartExport(GOBJ *menu_gobj)
{

    export_status = EXSTAT_REQSAVE;

    return;
}

void Record_LoadSavestate(Savestate *savestate) {
    int mirror = LabOptions_Record[OPTREC_MIRRORED_PLAYBACK].option_val;
    if (mirror == OPTMIRROR_RANDOM)
        mirror = HSD_Randi(2);

    // if a non-major savestate is saved while mirrored, we need to unmirror again.
    if (savestate != rec_state)
        mirror = mirror != event_vars->savestate_saved_while_mirrored;
    event_vars->loaded_mirrored = mirror;

    if (LabOptions_Record[OPTREC_HMNMODE].option_val == RECMODE_HMN_PLAYBACK
            && LabOptions_Record[OPTREC_HMNSLOT].option_val == 0)
        rec_data.hmn_rndm_slot = Record_GetRandomSlot(&rec_data.hmn_inputs, LabOptions_SlotChancesHMN);

    if (LabOptions_Record[OPTREC_CPUMODE].option_val == RECMODE_CPU_PLAYBACK
            && LabOptions_Record[OPTREC_CPUSLOT].option_val == 0)
        rec_data.cpu_rndm_slot = Record_GetRandomSlot(&rec_data.cpu_inputs, LabOptions_SlotChancesCPU);

    event_vars->game_timer = rec_state->frame;
    rec_data.restore_timer = 0;

    stc_playback_cancelled_hmn = false;
    stc_playback_cancelled_cpu = false;

    event_vars->Savestate_Load(savestate, mirror);
}

void Snap_CObjThink(GOBJ *gobj)
{
    if (snap_status == 1)
    {
        // take snap
        HSD_ImageDescCopyFromEFB(&snap_image, 0, 0, 0);
        snap_status = 0;
    }
}
void Savestates_Update()
{

    /*
    So i have code to do this in events.c that runs based a variable in
    the EventDesc, but i need to execute code directly after loading state
    so im just gonna do this here...
    */

    static int save_timer[4] = {0}; // Array to track save timer for each fighter
    const int SAVE_THRESHOLD = 10;
    // not when pause menu is showing
    if (Pause_CheckStatus(1) != 2)
    {
        if (lockout_timer > 0)
        {
            lockout_timer--;
        }
        else
        {
            // loop through all controller ports
            for (int port = 0; port < 4; port++)
            {
                HSD_Pad *pad = PadGet(port, PADGET_MASTER);
                if (pad == NULL) continue; // Skip if no controller in this port

                // check for savestate
                int blacklist = (HSD_BUTTON_DPAD_DOWN | HSD_BUTTON_DPAD_UP | HSD_TRIGGER_Z | HSD_TRIGGER_R | HSD_BUTTON_A | HSD_BUTTON_B | HSD_BUTTON_X | HSD_BUTTON_Y | HSD_BUTTON_START);
                
                // Save state (D-pad right)
                if ((pad->held & HSD_BUTTON_DPAD_RIGHT) && !(pad->held & blacklist))
                {
                    save_timer[port]++;
                    if (save_timer[port] == SAVE_THRESHOLD)
                    {
                        // save state
                        event_vars->Savestate_Save(event_vars->savestate);
                        event_vars->savestate_saved_while_mirrored = event_vars->loaded_mirrored;
                        save_timer[port] = 0; // Reset timer after saving
                        lockout_timer = LOCKOUT_DURATION;
                    }
                }
                else
                {
                    save_timer[port] = 0; // Reset timer if button is released
                }

                // Load state (D-pad left)
                if ((pad->down & HSD_BUTTON_DPAD_LEFT) && !(pad->held & blacklist))
                {
                    // load state
                    Record_LoadSavestate(event_vars->savestate);
                    stc_playback_cancelled_hmn = false;
                    stc_playback_cancelled_cpu = false;

                    // re-roll random slot
                    if (LabOptions_Record[OPTREC_HMNSLOT].option_val == 0)
                    {
                        rec_data.hmn_rndm_slot = Record_GetRandomSlot(&rec_data.hmn_inputs, LabOptions_SlotChancesHMN);
                    }
                    if (LabOptions_Record[OPTREC_CPUSLOT].option_val == 0)
                    {
                        rec_data.cpu_rndm_slot = Record_GetRandomSlot(&rec_data.cpu_inputs, LabOptions_SlotChancesCPU);
                    }
                }
            }
        }
    }

    return;
}

// Export functions
static char *keyboard_rows[2][4] = {
    {"1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm,./"},
    {"!@#$%^&*()", "QWERTYUIOP", "ASDFGHJKL: ", "ZXCVBNM<>?"}};
int RowPixelToBlockPixel(int pixel_x, int pixel_y, int width, int height)
{

    // get block width and height
    int block_width = divide_roundup(width, 4);

    // get block num
    int block_num = ((pixel_y / 4) * block_width) + (pixel_x / 4);

    // get pixels x and y within this block
    int block_pos_x = pixel_x % 4;
    int block_pos_y = pixel_y % 4;

    // get block pixel index
    int block_pixel = (block_num * 16) + (block_pos_y * 4) + block_pos_x;

    return block_pixel;
}
void ImageScale(RGB565 *out_img, RGB565 *in_img, int OutWidth, int OutHeight, int InWidth, int InHeight)
{

    int x_ratio = (InWidth / OutWidth);
    int y_ratio = (InHeight / OutHeight);
    int px, py;
    int in_pixel, out_pixel;

    for (int y = 0; y < (OutHeight); y++)
    {
        for (int x = 0; x < (OutWidth); x++)
        {
            px = x * x_ratio;
            py = y * y_ratio;
            in_pixel = RowPixelToBlockPixel(x, y, OutWidth, OutHeight);
            out_pixel = RowPixelToBlockPixel(px, py, InWidth, InHeight);
            out_img[in_pixel] = in_img[out_pixel];
        }
    }

    return;
}
void Export_Init(GOBJ *menu_gobj)
{

    MenuData *menu_data = menu_gobj->userdata;
    EventMenu *curr_menu = menu_data->currMenu;
    evMenu *menuAssets = event_vars->menu_assets;

    // create gobj
    GOBJ *export_gobj = GObj_Create(0, 0, 0);
    ExportData *export_data = calloc(sizeof(ExportData));
    GObj_AddUserData(export_gobj, 4, HSD_Free, export_data);

    // load menu joint
    JOBJ *export_joint = JOBJ_LoadJoint(stc_lab_data->export_menu);
    GObj_AddObject(export_gobj, 3, export_joint);                                  // add to gobj
    GObj_AddGXLink(export_gobj, GXLink_Common, GXLINK_MENUMODEL, GXPRI_MENUMODEL); // add gx link
    menu_data->custom_gobj_think = Export_Think;                                   // set callback

    // save jobj pointers
    JOBJ_GetChild(export_joint, &export_data->memcard_jobj[0], EXP_MEMCARDAJOBJ, -1);
    JOBJ_GetChild(export_joint, &export_data->memcard_jobj[1], EXP_MEMCARDBJOBJ, -1);
    JOBJ_GetChild(export_joint, &export_data->screenshot_jobj, EXP_SCREENSHOTJOBJ, -1);
    JOBJ_GetChild(export_joint, &export_data->textbox_jobj, EXP_TEXTBOXJOBJ, -1);

    // hide all
    JOBJ_SetFlags(export_data->memcard_jobj[0], JOBJ_HIDDEN);
    JOBJ_SetFlags(export_data->memcard_jobj[1], JOBJ_HIDDEN);
    JOBJ_SetFlags(export_data->screenshot_jobj, JOBJ_HIDDEN);
    JOBJ_SetFlags(export_data->textbox_jobj, JOBJ_HIDDEN);

    // alloc a buffer for all of the recording data
    RecordingSave *temp_rec_save = calloc(sizeof(RecordingSave));

    // copy match data to buffer
    memcpy(&temp_rec_save->match_data, &stc_match->match, sizeof(MatchInit));
    // copy savestate to buffer
    memcpy(&temp_rec_save->savestate, rec_state, sizeof(Savestate));
    // copy recordings
    for (int i = 0; i < REC_SLOTS; i++)
    {
        memcpy(&temp_rec_save->hmn_inputs[i], rec_data.hmn_inputs[i], sizeof(RecInputData));
        memcpy(&temp_rec_save->cpu_inputs[i], rec_data.cpu_inputs[i], sizeof(RecInputData));
    }

    // compress all recording data
    u8 *recording_buffer = calloc(sizeof(RecordingSave));
    int compress_size = Export_Compress(recording_buffer, temp_rec_save, sizeof(RecordingSave));
    HSD_Free(temp_rec_save); // free original data buffer

    // resize screenshot
    int img_size = GXGetTexBufferSize(RESIZE_WIDTH, RESIZE_HEIGHT, 4, 0, 0);
    RGB565 *orig_img = snap_image.img_ptr;
    RGB565 *new_img = calloc(img_size);
    export_data->scaled_image = new_img;
    ImageScale(new_img, orig_img, RESIZE_WIDTH, RESIZE_HEIGHT, EXP_SCREENSHOT_WIDTH, EXP_SCREENSHOT_HEIGHT);
    resized_image.img_ptr = new_img;                                            // store pointer to resized image
    export_data->screenshot_jobj->dobj->mobj->tobj->imagedesc = &resized_image; // replace pointer to imagedesc

    // get curr date
    OSCalendarTime td;
    OSTicksToCalendarTime(OSGetTime(), &td);

    // alloc a buffer to transfer to memcard
    stc_transfer_buf_size = sizeof(ExportHeader) + img_size + sizeof(ExportMenuSettings) + compress_size;
    stc_transfer_buf = calloc(stc_transfer_buf_size);

    // init header
    ExportHeader *header = stc_transfer_buf;
    header->metadata.version = REC_VERS;
    header->metadata.image_width = RESIZE_WIDTH;
    header->metadata.image_height = RESIZE_HEIGHT;
    header->metadata.image_fmt = 4;
    header->metadata.hmn = Fighter_GetExternalID(0);
    header->metadata.hmn_costume = Fighter_GetCostumeID(0);
    header->metadata.cpu = Fighter_GetExternalID(1);
    header->metadata.cpu_costume = Fighter_GetCostumeID(1);
    header->metadata.stage_external = Stage_GetExternalID();
    header->metadata.stage_internal = Stage_ExternalToInternal(header->metadata.stage_external);
    header->metadata.month = td.mon + 1;
    header->metadata.day = td.mday;
    header->metadata.year = td.year;
    header->metadata.hour = td.hour;
    header->metadata.minute = td.min;
    header->metadata.second = td.sec;
    header->lookup.ofst_screenshot = sizeof(ExportHeader);
    header->lookup.ofst_recording = sizeof(ExportHeader) + img_size;
    header->lookup.ofst_menusettings = sizeof(ExportHeader) + img_size + compress_size;

    // copy data to buffer
    // image
    memcpy(stc_transfer_buf + header->lookup.ofst_screenshot, new_img, img_size);
    // menu settings
    ExportMenuSettings *menu_settings = stc_transfer_buf + header->lookup.ofst_menusettings;
    menu_settings->hmn_mode = LabOptions_Record[OPTREC_HMNMODE].option_val;
    menu_settings->hmn_slot = LabOptions_Record[OPTREC_HMNSLOT].option_val;
    menu_settings->cpu_mode = LabOptions_Record[OPTREC_CPUMODE].option_val;
    menu_settings->cpu_slot = LabOptions_Record[OPTREC_CPUSLOT].option_val;
    menu_settings->loop_inputs = LabOptions_Record[OPTREC_LOOP].option_val;
    menu_settings->auto_restore = LabOptions_Record[OPTREC_AUTORESTORE].option_val;
    // recording data
    memcpy(stc_transfer_buf + header->lookup.ofst_recording, recording_buffer, compress_size); // compressed recording

    // free compresed data buffer
    HSD_Free(recording_buffer);

    // alloc filename buffer
    export_data->filename_buffer = calloc(32 + 2); // +2 for terminator and cursor

    // initialize memcard menu
    Export_SelCardInit(export_gobj);

    event_vars->hide_menu = 1;                       // hide original menu
    menu_data->custom_gobj = export_gobj;            // set custom gobj
    menu_data->custom_gobj_think = Export_Think;     // set think function
    menu_data->custom_gobj_destroy = Export_Destroy; // set destroy function

    return;
}
int Export_Think(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    int can_unpause = 0;

    switch (export_data->menu_index)
    {
    case (EXMENU_SELCARD):
    {
        can_unpause = Export_SelCardThink(export_gobj);
        break;
    }
    case (EXMENU_NAME):
    {
        can_unpause = Export_EnterNameThink(export_gobj);
        break;
    }
    case (EXMENU_CONFIRM):
    {
        can_unpause = Export_ConfirmThink(export_gobj);
        break;
    }
    }

    return can_unpause;
}
void Export_Destroy(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;

    switch (export_data->menu_index)
    {
    case (EXMENU_SELCARD):
    {
        Export_SelCardExit(export_gobj);
        break;
    }
    case (EXMENU_NAME):
    {
        Export_EnterNameExit(export_gobj);
        break;
    }
    }

    // free buffer allocs
    HSD_Free(stc_transfer_buf);
    HSD_Free(export_data->filename_buffer);
    HSD_Free(export_data->scaled_image);

    // destroy gobj
    GObj_Destroy(export_gobj);

    // show menu
    event_vars->hide_menu = 0;
    menu_data->custom_gobj = 0;
    menu_data->custom_gobj_think = 0;
    menu_data->custom_gobj_destroy = 0;

    return;
}
void Export_SelCardInit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;

    // show menu jobjs
    JOBJ_ClearFlags(export_data->memcard_jobj[0], JOBJ_HIDDEN);
    JOBJ_ClearFlags(export_data->memcard_jobj[1], JOBJ_HIDDEN);

    // create text
    Text *text_misc = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_misc = text_misc;
    // enable align and kerning
    text_misc->align = 1;
    text_misc->kerning = 1;
    // scale canvas
    text_misc->viewport_scale.X = MENU_CANVASSCALE;
    text_misc->viewport_scale.Y = MENU_CANVASSCALE;
    text_misc->trans.Z = MENU_TEXTZ;

    // create title text
    Text *text_title = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_title = text_title;
    // enable align and kerning
    text_title->align = 0;
    text_title->kerning = 1;
    // scale canvas
    text_title->trans.X = -23;
    text_title->trans.Y = -18;
    text_title->viewport_scale.X = MENU_CANVASSCALE * 2;
    text_title->viewport_scale.Y = MENU_CANVASSCALE * 2;
    text_title->trans.Z = MENU_TEXTZ;

    // create desc text
    Text *text_desc = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_desc = text_desc;
    // enable align and kerning
    text_desc->align = 0;
    text_desc->kerning = 1;
    // scale canvas
    text_desc->trans.X = -23;
    text_desc->trans.Y = 12;
    text_desc->viewport_scale.X = MENU_CANVASSCALE;
    text_desc->viewport_scale.Y = MENU_CANVASSCALE;
    text_desc->trans.Z = MENU_TEXTZ;

    Text_AddSubtext(text_title, 0, 0, "Select a Memory Card"); // add title
    Text_AddSubtext(text_desc, 0, 0, "");                      // add description

    // add dummy text
    Text_AddSubtext(text_misc, -165, 67, "Slot A");
    Text_AddSubtext(text_misc, 165, 67, "Slot B");

    // init memcard inserted status
    for (int i = 0; i < 2; i++)
    {
        export_data->is_inserted[i] = 0;
    }

    // init cursor
    export_data->menu_index = EXMENU_SELCARD;
    export_data->slot = 0;

    return;
}
int Export_SelCardThink(GOBJ *export_gobj)
{

    ExportData *export_data = export_gobj->userdata;

    int req_blocks = (divide_roundup(stc_transfer_buf_size, 8192) + 1);

    // get pausing players inputs
    HSD_Pad *pad = PadGet(stc_hmn_controller, PADGET_MASTER);
    int inputs = pad->down;

    // update memcard info
    for (int i = 1; i >= 0; i--)
    {
        // probe slot
        u8 is_inserted;

        s32 memSize, sectorSize;
        if (CARDProbeEx(i, &memSize, &sectorSize) == CARD_RESULT_READY)
        {
            // if it was just inserted, get info
            if (export_data->is_inserted[i] == 0)
            {

                // mount card
                stc_memcard_work->is_done = 0;
                if (CARDMountAsync(i, stc_memcard_work->work_area, 0, Memcard_RemovedCallback) == CARD_RESULT_READY)
                {
                    // check card
                    Memcard_Wait();
                    stc_memcard_work->is_done = 0;
                    if (CARDCheckAsync(i, Memcard_RemovedCallback) == CARD_RESULT_READY)
                    {
                        Memcard_Wait();

                        // if we get this far, a valid memcard is inserted
                        is_inserted = 1;
                        SFX_PlayCommon(2);
                        //export_data->slot = i;  // move cursor to this

                        // get free blocks
                        s32 byteNotUsed, filesNotUsed;
                        if (CARDFreeBlocks(i, &byteNotUsed, &filesNotUsed) == CARD_RESULT_READY)
                        {
                            export_data->free_blocks[i] = (byteNotUsed / 8192);
                            export_data->free_files[i] = filesNotUsed;
                        }
                    }
                    else
                        is_inserted = 0;

                    CARDUnmount(i);
                    stc_memcard_work->is_done = 0;
                }
                else
                    is_inserted = 0;
            }
            else
                is_inserted = 1;
        }
        else
            is_inserted = 0;

        export_data->is_inserted[i] = is_inserted;
    }

    // if left
    if ((inputs & HSD_BUTTON_LEFT) || (inputs & HSD_BUTTON_DPAD_LEFT))
    {
        if (export_data->slot > 0)
        {
            export_data->slot--;
            SFX_PlayCommon(2);
        }
    }

    // if right
    if ((inputs & HSD_BUTTON_RIGHT) || (inputs & HSD_BUTTON_DPAD_RIGHT))
    {
        if (export_data->slot < 1)
        {
            export_data->slot++;
            SFX_PlayCommon(2);
        }
    }

    int cursor = export_data->slot;
    // if press A,
    if ((inputs & HSD_BUTTON_A) || (inputs & HSD_BUTTON_START))
    {
        // ensure it can be saved
        if ((export_data->is_inserted[cursor] == 1) && (export_data->free_files[cursor] >= 1) && (export_data->free_blocks[cursor] >= req_blocks))
        {
            // can save move to next screen
            Export_SelCardExit(export_gobj);

            // init next menu
            Export_EnterNameInit(export_gobj);

            SFX_PlayCommon(1);

            return;
        }

        else
            SFX_PlayCommon(3);
    }

    // if press B,
    if ((inputs & HSD_BUTTON_B))
    {
        Export_Destroy(export_gobj);

        // play sfx
        SFX_PlayCommon(0);

        return;
    }

    // update selection
    Text *text = export_data->text_misc;
    for (int i = 0; i < 2; i++)
    {
        static GXColor white = {255, 255, 255, 255};
        static GXColor yellow = {201, 178, 0, 255};
        GXColor *color;

        // highlight cursor only
        if (export_data->slot == i)
            color = &yellow;
        else
            color = &white;

        Text_SetColor(text, i, color);
    }

    // update description
    Text *text_desc = export_data->text_desc;
    if (export_data->is_inserted[cursor] == 0)
    {
        Text_SetText(text_desc, 0, "No device is inserted in Slot %s.", slots_names[cursor]);
    }
    else if (export_data->free_files[cursor] < 1)
    {
        Text_SetText(text_desc, 0, "The memory card in Slot %s does not \nhave enough free files. 1 free file is \nrequired to save.", slots_names[cursor]);
    }
    else if (export_data->free_blocks[cursor] < req_blocks)
    {
        Text_SetText(text_desc, 0, "The memory card in Slot %s does not \nhave enough free blocks. %d blocks is \nrequired to save.", slots_names[cursor], req_blocks);
    }
    else
    {
        Text_SetText(text_desc, 0, "Slot %s: %d free blocks. %d blocks will be used.", slots_names[cursor], export_data->free_blocks[cursor], req_blocks);
    }
    return 1;
}
void Export_SelCardExit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;

    // hide menu jobjs
    JOBJ_SetFlags(export_data->memcard_jobj[0], JOBJ_HIDDEN);
    JOBJ_SetFlags(export_data->memcard_jobj[1], JOBJ_HIDDEN);

    Text_Destroy(export_data->text_title);
    Text_Destroy(export_data->text_desc);
    Text_Destroy(export_data->text_misc);
}
void Export_EnterNameInit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;

    // show menu jobjs
    JOBJ_ClearFlags(export_data->screenshot_jobj, JOBJ_HIDDEN);
    JOBJ_ClearFlags(export_data->textbox_jobj, JOBJ_HIDDEN);

    // create keyboard text
    Text *text_keyboard = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_keyboard = text_keyboard;
    // enable align and kerning
    text_keyboard->align = 1;
    text_keyboard->kerning = 1;
    // scale canvas
    text_keyboard->trans.X = EXP_KEYBOARD_X;
    text_keyboard->trans.Y = EXP_KEYBOARD_Y;
    text_keyboard->viewport_scale.X = MENU_CANVASSCALE * EXP_KEYBOARD_SIZE;
    text_keyboard->viewport_scale.Y = MENU_CANVASSCALE * EXP_KEYBOARD_SIZE;
    // init keyboard
    for (int i = 0; i < 4; i++)
    {
        // iterate through columns
        for (int j = 0; j < 10; j++)
        {
            Text_AddSubtext(text_keyboard, (-(9.0 / 2.0) * 60) + (j * 60), -80 + (i * 60), "");
        }
    }
    export_data->key_cursor[0] = 0;
    export_data->key_cursor[1] = 0;
    export_data->caps_lock = 0;
    Export_EnterNameUpdateKeyboard(export_gobj);

    // create file details
    ExportHeader *header = stc_transfer_buf;
    char *stage_name = stage_names[header->metadata.stage_internal];
    char *hmn_name = Fighter_GetName(header->metadata.hmn);
    char *cpu_name = Fighter_GetName(header->metadata.cpu);
    Text *text_filedetails = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_filedetails = text_filedetails;
    // enable align and kerning
    text_filedetails->align = 0;
    text_filedetails->kerning = 1;
    // scale canvas
    text_filedetails->trans.X = EXP_FILEDETAILS_X;
    text_filedetails->trans.Y = EXP_FILEDETAILS_Y;
    text_filedetails->viewport_scale.X = MENU_CANVASSCALE * EXP_FILEDETAILS_SIZE;
    text_filedetails->viewport_scale.Y = MENU_CANVASSCALE * EXP_FILEDETAILS_SIZE;

    Text_AddSubtext(text_filedetails, 0, 0, "Stage: %s\nHMN: %s\nCPU: %s\n\nSlot %s", stage_name, hmn_name, cpu_name, slots_names[export_data->slot]); // add title

    // create title text
    Text *text_title = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_title = text_title;
    // enable align and kerning
    text_title->align = 0;
    text_title->kerning = 1;
    // scale canvas
    text_title->trans.X = -23;
    text_title->trans.Y = -18;
    text_title->viewport_scale.X = MENU_CANVASSCALE * 2;
    text_title->viewport_scale.Y = MENU_CANVASSCALE * 2;
    text_title->trans.Z = MENU_TEXTZ;
    Text_AddSubtext(text_title, 0, 0, "Enter File Name");

    // create desc text
    Text *text_desc = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_desc = text_desc;
    // enable align and kerning
    text_desc->align = 0;
    text_desc->kerning = 1;
    // scale canvas
    text_desc->trans.X = -23;
    text_desc->trans.Y = 12;
    text_desc->viewport_scale.X = MENU_CANVASSCALE;
    text_desc->viewport_scale.Y = MENU_CANVASSCALE;
    Text_AddSubtext(text_desc, 0, 0, "A: Select  B: Backspace  Y: Caps  Start: Confirm"); // add description
    Text_AddSubtext(text_desc, 0, 40, "     X: Space  L: Cursor left  R: Cursor right");  // add description

    // create filename
    Text *text_filename = Text_CreateText(2, menu_data->canvas_menu);
    export_data->text_filename = text_filename;
    // enable align and kerning
    text_filename->align = 0;
    text_filename->kerning = 1;
    text_filename->use_aspect = 1;
    GXColor filename_color = {225, 225, 225, 255};
    text_filename->color = filename_color;
    // scale canvas
    text_filename->trans.X = EXP_FILENAME_X;
    text_filename->trans.Y = EXP_FILENAME_Y;
    text_filename->aspect.X = EXP_FILENAME_ASPECTX;
    text_filename->aspect.Y = EXP_FILENAME_ASPECTY;
    text_filename->viewport_scale.X = MENU_CANVASSCALE * EXP_FILENAME_SIZE;
    text_filename->viewport_scale.Y = MENU_CANVASSCALE * EXP_FILENAME_SIZE;
    // init filename buffer
    export_data->filename_buffer[0] = '_';
    export_data->filename_buffer[1] = '\0';
    Text_AddSubtext(text_filename, 0, 0, export_data->filename_buffer); // add title

    // init menu variables
    export_data->menu_index = EXMENU_NAME;
    export_data->filename_cursor = 0;

    return;
}
int Export_EnterNameThink(GOBJ *export_gobj)
{

    ExportData *export_data = export_gobj->userdata;

    // get pausing players inputs
    HSD_Pad *pad = PadGet(stc_hmn_controller, PADGET_MASTER);
    int inputs = pad->rapidFire;
    int input_down = pad->down;
    u8 *cursor = export_data->key_cursor;
    int update_keyboard = 0;
    int update_filename = 0;
    char *filename_buffer = export_data->filename_buffer;

    // first ensure memcard is still inserted
    s32 memSize, sectorSize;
    if (CARDProbeEx(export_data->slot, &memSize, &sectorSize) != CARD_RESULT_READY)
        goto EXIT;

    // if left
    if ((inputs & HSD_BUTTON_LEFT) || (inputs & HSD_BUTTON_DPAD_LEFT))
    {
        if (cursor[0] > 0)
        {
            cursor[0]--;
        }
        else
        {
            cursor[0] = (10 - 1);
        }
        update_keyboard = 1;
    }
    // if right
    else if ((inputs & HSD_BUTTON_RIGHT) || (inputs & HSD_BUTTON_DPAD_RIGHT))
    {
        if (cursor[0] < (10 - 1))
        {
            cursor[0]++;
        }
        else
        {
            cursor[0] = 0;
        }
        update_keyboard = 1;
    }
    // if up
    else if ((inputs & HSD_BUTTON_UP) || (inputs & HSD_BUTTON_DPAD_UP))
    {
        if (cursor[1] > 0)
        {
            cursor[1]--;
        }
        else
        {
            cursor[1] = (4 - 1);
        }
        update_keyboard = 1;
    }
    // if down
    else if ((inputs & HSD_BUTTON_DOWN) || (inputs & HSD_BUTTON_DPAD_DOWN))
    {
        if (cursor[1] < 4 - 1)
        {
            cursor[1]++;
        }
        else
        {
            cursor[1] = 0;
        }
        update_keyboard = 1;
    }
    // if A
    else if ((inputs & HSD_BUTTON_A))
    {

        // check if any remaining characters
        if (export_data->filename_cursor < 32)
        {

            // get correct set of letters
            char **keyboard_letters = keyboard_rows[export_data->caps_lock];

            // add character to buffer
            filename_buffer[export_data->filename_cursor] = keyboard_letters[cursor[1]][cursor[0]];

            // add cursor and terminator
            filename_buffer[export_data->filename_cursor + 1] = '_';
            filename_buffer[export_data->filename_cursor + 2] = '\0';

            // inc cursor
            export_data->filename_cursor++;

            // update filename
            update_filename = 1;

            // remove caps lock
            export_data->caps_lock = 0;
            update_keyboard = 1;

            SFX_PlayCommon(1);
        }
        else
        {
            SFX_PlayCommon(3);
        }
    }
    // if B
    else if ((inputs & HSD_BUTTON_B))
    {

        // check if can delete
        if (export_data->filename_cursor > 0)
        {
            // dec cursor
            export_data->filename_cursor--;

            // add cursor and terminator
            filename_buffer[export_data->filename_cursor] = '_';
            filename_buffer[export_data->filename_cursor + 1] = '\0';

            // update filename
            update_filename = 1;

            SFX_PlayCommon(0);
        }

        // exit here
        else if (input_down & HSD_BUTTON_B)
        {
        EXIT:
            Export_EnterNameExit(export_gobj);
            Export_SelCardInit(export_gobj);
            SFX_PlayCommon(0);
            return 0;
        }
    }
    // if Y
    if ((inputs & HSD_BUTTON_Y))
    {
        // toggle capslock
        if (export_data->caps_lock == 0)
            export_data->caps_lock = 1;
        else
            export_data->caps_lock = 0;

        // update keyboard
        update_keyboard = 1;

        SFX_PlayCommon(1);
    }
    // if X
    if ((inputs & HSD_BUTTON_X))
    {

        // check if any remaining characters
        if (export_data->filename_cursor < 32)
        {
            // add character to buffer
            filename_buffer[export_data->filename_cursor] = ' ';

            // add cursor and terminator
            filename_buffer[export_data->filename_cursor + 1] = '_';
            filename_buffer[export_data->filename_cursor + 2] = '\0';

            // inc cursor
            export_data->filename_cursor++;

            // update filename
            update_filename = 1;

            SFX_PlayCommon(1);
        }
        else
        {
            OSReport("max characters!\n");
        }
    }
    // if START
    if ((inputs & HSD_BUTTON_START))
    {
        // at least 1 character
        if (export_data->filename_cursor > 0)
        {
            Export_ConfirmInit(export_gobj);

            // play sfx
            SFX_PlayCommon(1);
        }
        else
        {
            SFX_PlayCommon(3);
        }

        return 0;
    }

    // update keyboard
    if (update_keyboard == 1)
    {
        Export_EnterNameUpdateKeyboard(export_gobj);
        SFX_PlayCommon(2);
    }

    // update filename if changed
    if (update_filename == 1)
    {
        Text_SetText(export_data->text_filename, 0, filename_buffer);
    }

    return 0;
}
void Export_EnterNameExit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;

    // hide menu jobjs
    JOBJ_SetFlags(export_data->screenshot_jobj, JOBJ_HIDDEN);
    JOBJ_SetFlags(export_data->textbox_jobj, JOBJ_HIDDEN);

    Text_Destroy(export_data->text_title);
    Text_Destroy(export_data->text_desc);
    Text_Destroy(export_data->text_keyboard);
    Text_Destroy(export_data->text_filename);
    Text_Destroy(export_data->text_filedetails);
}
void Export_ConfirmInit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    MenuData *menu_data = event_vars->menu_gobj->userdata;

    // create gobj
    GOBJ *confirm_gobj = GObj_Create(0, 0, 0);
    export_data->confirm_gobj = confirm_gobj;

    // load menu joint
    JOBJ *confirm_jobj = JOBJ_LoadJoint(stc_lab_data->export_popup);
    GObj_AddObject(confirm_gobj, 3, confirm_jobj);                                   // add to gobj
    GObj_AddGXLink(confirm_gobj, GXLink_Common, GXLINK_MENUMODEL, GXPRI_POPUPMODEL); // add gx link

    // create text
    Text *confirm_text = Text_CreateText(2, menu_data->canvas_popup);
    export_data->confirm_text = confirm_text;
    // enable align and kerning
    confirm_text->align = 1;
    confirm_text->kerning = 1;
    // scale canvas
    confirm_text->trans.X = 0;
    confirm_text->trans.Y = 0;
    confirm_text->viewport_scale.X = MENU_CANVASSCALE;
    confirm_text->viewport_scale.Y = MENU_CANVASSCALE;
    confirm_text->trans.Z = MENU_TEXTZ;
    Text_AddSubtext(confirm_text, 0, -40, "Save File to Slot %s?", slots_names[export_data->slot]);
    int yes_subtext = Text_AddSubtext(confirm_text, -60, 20, "Yes");
    GXColor yellow = {201, 178, 0, 255};
    Text_SetColor(confirm_text, yes_subtext, &yellow);
    Text_AddSubtext(confirm_text, 60, 20, "No");

    export_data->menu_index = EXMENU_CONFIRM;
    export_data->confirm_state = EXPOP_CONFIRM;
    export_data->confirm_cursor = 0;

    return 0;
}
int Export_ConfirmThink(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;

    // get pausing players inputs
    HSD_Pad *pad = PadGet(stc_hmn_controller, PADGET_MASTER);
    int inputs = pad->down;

    // if unplugged exit
    s32 memSize, sectorSize;
    if (CARDProbeEx(export_data->slot, &memSize, &sectorSize) != CARD_RESULT_READY)
    {
        Export_ConfirmExit(export_gobj);
        Export_EnterNameExit(export_gobj);

        // play sfx
        SFX_PlayCommon(0);
    }

    switch (export_data->confirm_state)
    {
    case (EXPOP_CONFIRM):
    {

        int update_cursor = 0;

        // if left
        if ((inputs & HSD_BUTTON_LEFT) || (inputs & HSD_BUTTON_DPAD_LEFT))
        {
            if (export_data->confirm_cursor > 0)
            {
                export_data->confirm_cursor--;
                update_cursor = 1;
            }
        }
        // if right
        else if ((inputs & HSD_BUTTON_RIGHT) || (inputs & HSD_BUTTON_DPAD_RIGHT))
        {
            if (export_data->confirm_cursor < 1)
            {
                export_data->confirm_cursor++;
                update_cursor = 1;
            }
        }

        // if b
        else if ((inputs & HSD_BUTTON_B))
        {
            Export_ConfirmExit(export_gobj);

            // play sfx
            SFX_PlayCommon(0);

            return 0;
        }
        // if a
        else if ((inputs & HSD_BUTTON_A) || (inputs & HSD_BUTTON_START))
        {

            // begin save
            if (export_data->confirm_cursor == 0)
            {

                MenuData *menu_data = event_vars->menu_gobj->userdata;

                // free current text
                Text_Destroy(export_data->confirm_text);

                // create text
                Text *confirm_text = Text_CreateText(2, menu_data->canvas_popup);
                export_data->confirm_text = confirm_text;
                // enable align and kerning
                confirm_text->align = 1;
                confirm_text->kerning = 1;
                // scale canvas
                confirm_text->trans.X = 0;
                confirm_text->trans.Y = 0;
                confirm_text->viewport_scale.X = MENU_CANVASSCALE;
                confirm_text->viewport_scale.Y = MENU_CANVASSCALE;
                confirm_text->trans.Z = MENU_TEXTZ;
                Text_AddSubtext(confirm_text, 0, -20, "");

                export_data->confirm_state = EXPOP_SAVE;

                export_status = EXSTAT_REQSAVE;

                // play sfx
                SFX_PlayCommon(1);

                return 0;
            }
            // go back to keyboard menu
            else
            {
                Export_ConfirmExit(export_gobj);

                // play sfx
                SFX_PlayCommon(0);

                return 0;
            }
        }

        if (update_cursor == 1)
        {
            for (int i = 0; i < 2; i++)
            {
                static GXColor white = {255, 255, 255, 255};
                static GXColor yellow = {201, 178, 0, 255};
                GXColor *color;

                // highlight cursor only
                if (export_data->confirm_cursor == i)
                    color = &yellow;
                else
                    color = &white;

                Text_SetColor(export_data->confirm_text, i + 1, color);
            }

            SFX_PlayCommon(2);
        }
        break;
    }
    case (EXPOP_SAVE):
    {
        // wait for save to finish
        if (Export_Process(export_gobj) == 1)
        {
            Export_ConfirmExit(export_gobj);
            Export_Destroy(export_gobj);

            // play sfx
            SFX_PlayCommon(1);

            return 0;
        }
        break;
    }
    }
    return 0;
}
void Export_ConfirmExit(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;

    GObj_Destroy(export_data->confirm_gobj);
    Text_Destroy(export_data->confirm_text);

    export_data->menu_index = EXMENU_NAME;
}
void Export_EnterNameUpdateKeyboard(GOBJ *export_gobj)
{
    ExportData *export_data = export_gobj->userdata;
    Text *text_keyboard = export_data->text_keyboard;
    u8 *cursor = export_data->key_cursor;

    // get correct set of letters
    char **keyboard_letters = keyboard_rows[export_data->caps_lock];

    // iterate through rows
    for (int i = 0; i < 4; i++)
    {
        // iterate through columns
        for (int j = 0; j < 10; j++)
        {

            int this_subtext = (i * 10) + j;

            // update letter text
            char letter[2];
            letter[0] = keyboard_letters[i][j];
            letter[1] = '\0';
            Text_SetText(text_keyboard, this_subtext, &letter);

            // update letter color
            static GXColor white = {255, 255, 255, 255};
            static GXColor yellow = {201, 178, 0, 255};
            GXColor *color;
            // check for cursor
            if ((cursor[0] == j) && (cursor[1] == i))
                color = &yellow;
            else
                color = &white;
            Text_SetColor(text_keyboard, this_subtext, color);
        }
    }

    return;
}
int Export_Process(GOBJ *export_gobj)
{

    ExportData *export_data = export_gobj->userdata;
    Text *text = export_data->confirm_text;

    int finished = 0;

    // if snapshot is processing, dont update
    switch (export_status)
    {
    case (EXSTAT_REQSAVE):
    {

        int slot = export_data->slot;

        save_pre_tick = OSGetTick();

        // create filename string
        ExportHeader *header = stc_transfer_buf;
        char filename[32];
        sprintf(filename, tm_filename, header->metadata.month, header->metadata.day, header->metadata.year, header->metadata.hour, header->metadata.minute, header->metadata.second); // generate filename based on date, time, fighters, and stage

        // save file name to metadata
        memcpy(&header->metadata.filename, export_data->filename_buffer, export_data->filename_cursor);

        // check if file exists and delete it
        s32 memSize, sectorSize;
        if (CARDProbeEx(slot, &memSize, &sectorSize) == CARD_RESULT_READY)
        {
            // mount card
            stc_memcard_work->is_done = 0;
            if (CARDMountAsync(slot, stc_memcard_work->work_area, 0, Memcard_RemovedCallback) == CARD_RESULT_READY)
            {
                // check card
                Memcard_Wait();
                stc_memcard_work->is_done = 0;
                if (CARDCheckAsync(slot, Memcard_RemovedCallback) == CARD_RESULT_READY)
                {
                    Memcard_Wait();
                    // get free blocks
                    s32 byteNotUsed, filesNotUsed;
                    if (CARDFreeBlocks(slot, &byteNotUsed, &filesNotUsed) == CARD_RESULT_READY)
                    {
                        // search for file with this name
                        for (int i = 0; i < CARD_MAX_FILE; i++)
                        {
                            CARDStat card_stat;

                            if (CARDGetStatus(slot, i, &card_stat) == CARD_RESULT_READY)
                            {
                                // check company code
                                if (strncmp(os_info->company, card_stat.company, sizeof(os_info->company)) == 0)
                                {
                                    // check game name
                                    if (strncmp(os_info->gameName, card_stat.gameName, sizeof(os_info->gameName)) == 0)
                                    {
                                        // check file name
                                        if (strncmp(&filename, card_stat.fileName, sizeof(filename)) == 0)
                                        {
                                            // delete
                                            CARDDeleteAsync(slot, &filename, Memcard_RemovedCallback);
                                            stc_memcard_work->is_done = 0;
                                            Memcard_Wait();
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                CARDUnmount(slot);
                stc_memcard_work->is_done = 0;
            }
        }

        // setup save
        memcpy(stc_memcard_info->file_name, &stc_save_name, sizeof(stc_save_name));
        memset(stc_memcard_info->file_desc, '\0', 32);                                                   // fill with spaces
        memcpy(stc_memcard_info->file_desc, export_data->filename_buffer, export_data->filename_cursor); // copy inputted name
        memcard_save.data = stc_transfer_buf;
        memcard_save.x4 = 3;
        memcard_save.size = stc_transfer_buf_size;
        memcard_save.xc = -1;
        Memcard_ReqSaveCreate(slot, &filename, &memcard_save, stc_memcard_unk, stc_memcard_info->file_name, stc_lab_data->save_banner, stc_lab_data->save_icon, 0);

        // change status
        export_status = EXSTAT_SAVEWAIT;

        break;
    }
    case (EXSTAT_SAVEWAIT):
    {

        // wait to finish writing
        if (Memcard_CheckStatus() != 11)
        {
            // change state
            export_status = EXSTAT_DONE;
        }
        else
        {
            if (*stc_memcard_write_status == 6)
            {
                Text_SetText(text, 0, "Writing Data...");
            }
            else
            {
                Text_SetText(text, 0, "Creating File");
            }
        }

        break;
    }
    case (EXSTAT_DONE):
    {

        export_status = EXSTAT_NONE;
        finished = 1;

        Text_Destroy(text);

        // done saving, output time
        int save_post_tick = OSGetTick();
        int save_time = OSTicksToMilliseconds(save_post_tick - save_pre_tick);
        break;
    }
    }

    return finished;
}
int Export_Compress(u8 *dest, u8 *source, u32 size)
{

    int pre_tick = OSGetTick();
    int compress_size = lz77_compress(source, size, dest, 8);
    int post_tick = OSGetTick();
    int time_dif = OSTicksToMilliseconds(post_tick - pre_tick);
    return compress_size;
}

static void UpdateDataTracking(GOBJ *character) {
    FighterData *data = character->userdata;

    if (data->flags.past_iasa)
        data->TM.iasa_frames++;
    else
        data->TM.iasa_frames = 0;
}

static void UpdateOverlays(GOBJ *character, EventOption *overlays) {
    FighterData *data = character->userdata;

    static int overlays_running[4] = { -1, -1, -1, -1 };
    int *overlay_running = &overlays_running[data->ply];

    for (OverlayGroup i = 0; i < OVERLAY_COUNT; i++)
    {
        // overlays go from general to specific, so we iterate in reverse
        // to use the most specific overlay possibe.
        int j = OVERLAY_COUNT - i - 1;

        int color_idx = overlays[j].option_val;
        if (color_idx == 0) continue;

        if (CheckOverlay(character, j))
        {
            Overlay ov = LabValues_OverlayColours[color_idx];

            if (ov.occur_once && *overlay_running == j)
                return;

            memset(&data->color[1], 0, sizeof(ColorOverlay));
            memset(&data->color[0], 0, sizeof(ColorOverlay));

            // Changes the alpha of the double jump overlay based on the number of jump used
            if (j == OVERLAY_JUMPS_USED){
                float max_jumps = (float)data->attr.max_jumps;
                float jumps_used = (float)data->jump.jumps_used;
                float alpha_factor = 1.0 - (max_jumps - jumps_used) / max_jumps;
                ov.color.a *= alpha_factor;
            }

            *overlay_running = j;
            data->color[1].hex = ov.color;
            data->color[1].color_enable = 1;
            data->flags.invisible = ov.invisible;

            if (ov.show_collision)
                data->show_hit = 1;

            if (ov.play_sound)
                SFX_PlayCommon(2);

            return;
        }
    }

    if (*overlay_running != -1) {
        memset(&data->color[1], 0, sizeof(ColorOverlay));
        memset(&data->color[0], 0, sizeof(ColorOverlay));
        data->flags.invisible = 0;
        data->show_hit = LabOptions_General[OPTGEN_HIT].option_val;
        *overlay_running = -1;
    }
}

static void Stage_SetFODPlatformHeight(int side) {
    int option_idx = side == 0 ? OPTSTAGE_FOD_PLAT_HEIGHT_LEFT : OPTSTAGE_FOD_PLAT_HEIGHT_RIGHT;
    int idx = LabOptions_Stage_FOD[option_idx].option_val;
    if (idx == 0) return; // random default
    float height = LabValues_FODPlatformHeights[idx];

    GOBJ *platform_gobj = Stage_GetMapGObj(4);
    if (side == 0)
        platform_gobj = platform_gobj->previous;

    GOBJ *stage_gobj = Stage_GetMapGObj(3);
    JOBJ *stage_jobj = stage_gobj->hsd_object;
    JOBJ *plat_jobj = stage_jobj->child->child;
    if (side != 0)
        plat_jobj = plat_jobj->sibling;

    struct FODData { u16 mode, timer; } *data = (void*)((u8*)platform_gobj->userdata + 0xC4);

    data->mode = 1;
    data->timer = 0xFF;

    plat_jobj->trans.Y = height * 1.21875;
    JOBJ_SetMtxDirtySub(plat_jobj);
}

static void Stage_Think(void) {
    GrExternal stage_kind = Stage_GetExternalID();
    if (stage_kind == GRKINDEXT_IZUMI) {
        Stage_SetFODPlatformHeight(0);
        Stage_SetFODPlatformHeight(1);
    }
}

// Think Function that runs after the stage and character's think functions.
// Needed to prevent data from overwritten by the native think functions.
void Event_PostThink(GOBJ *gobj)
{
    GOBJ *hmn = Fighter_GetGObj(0);
    GOBJ *cpu = Fighter_GetGObj(1);

    // we do this in post so that we can use the updated character data before rendering.
    UpdateDataTracking(hmn);
    UpdateDataTracking(cpu);

    UpdateOverlays(hmn, LabOptions_OverlaysHMN);
    UpdateOverlays(cpu, LabOptions_OverlaysCPU);

    Stage_Think();
}

// Init Function
void Event_Init(GOBJ *gobj)
{
    LabData *eventData = gobj->userdata;
    EventDesc *eventInfo = eventData->eventInfo;
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    GObj_AddProc(gobj, Event_PostThink, 20);

    // Init runtime options...

    // overlays
    memcpy(LabOptions_OverlaysHMN, LabOptions_OverlaysDefault, sizeof(LabOptions_OverlaysDefault));
    memcpy(LabOptions_OverlaysCPU, LabOptions_OverlaysDefault, sizeof(LabOptions_OverlaysDefault));

    // info display
    memcpy(LabOptions_InfoDisplayHMN, LabOptions_InfoDisplayDefault, sizeof(LabOptions_InfoDisplayDefault));
    memcpy(LabOptions_InfoDisplayCPU, LabOptions_InfoDisplayDefault, sizeof(LabOptions_InfoDisplayDefault));
    LabOptions_InfoDisplayHMN[OPTINF_PRESET].onOptionChange = Lab_ChangeInfoPresetHMN;
    LabOptions_InfoDisplayCPU[OPTINF_PRESET].onOptionChange = Lab_ChangeInfoPresetCPU;

    // saved options
    Memcard *memcard = R13_PTR(MEMCARD);
    LabOptions_General[OPTGEN_FRAMEBTN].option_val = memcard->TM_LabFrameAdvanceButton;
    LabOptions_General[OPTGEN_INPUT].option_val = memcard->TM_LabCPUInputDisplay;
    LabOptions_General[OPTGEN_TAUNT].option_val = memcard->TM_LabTauntEnabled;

    int overlay_save_count = sizeof(memcard->TM_LabSavedOverlays_HMN) / sizeof(OverlaySave);
    for (int i = 0; i < overlay_save_count; ++i) {
        OverlaySave save_hmn = memcard->TM_LabSavedOverlays_HMN[i];
        OverlaySave save_cpu = memcard->TM_LabSavedOverlays_CPU[i];

        if (save_hmn.overlay != 0)
            LabOptions_OverlaysHMN[save_hmn.group].option_val = save_hmn.overlay;

        if (save_cpu.overlay != 0)
            LabOptions_OverlaysCPU[save_cpu.group].option_val = save_cpu.overlay;
    }

    int enabled_osds = memcard->TM_OSDEnabled;
    for (int i = 0; i < OPTOSD_MAX; i++) {
        int osd_bit_position = osd_memory_bit_position[i];
        int is_osd_enabled = (enabled_osds & (1 << osd_bit_position)) != 0;
        LabOptions_OSDs[i].option_val = is_osd_enabled;
    }
    
    // stage options
    EventMenu *stage_menu = stage_menus[Stage_GetExternalID()];
    if (stage_menu != NULL) {
        LabOptions_Main[OPTLAB_STAGE].disable = 0;
        LabOptions_Main[OPTLAB_STAGE].menu = stage_menu;
    }

    // Aitch: replace HSD_Randi call in stadium transformation code with lwz r3, TRANSFORMATION_ID_PTR
    // No way around this hack really, we can't fake the transformation well because loading is done immediately after
    // the randi call, so we would have to decompile the entire 50k asm line function and reverse engineer the state machine.
    // Directly overwriting the asm is the best call for now imo.
    // This needs to be done here in init, as dolphin will jit the function when it runs and overwriting won't do anything.
    *(u32*)(0x801d463c) = 0x806d3778;

    // theres got to be a better way to do this...
    event_vars = *event_vars_ptr;

    event_vars->savestate_saved_while_mirrored = false;
    event_vars->loaded_mirrored = false;

    // get this events assets
    stc_lab_data = Archive_GetPublicAddress(event_vars->event_archive, "lab");

    // Init Info Display
    infodisp_gobj_hmn = InfoDisplay_Init(0);
    infodisp_gobj_cpu = InfoDisplay_Init(1);

    // update to show/hide
    InfoDisplay_Update(infodisp_gobj_hmn, LabOptions_InfoDisplayHMN, Fighter_GetGObj(0), NULL);
    InfoDisplay_Update(infodisp_gobj_cpu, LabOptions_InfoDisplayCPU, Fighter_GetGObj(1), infodisp_gobj_hmn);

    // Init DIDraw
    DIDraw_Init();

    // Init Recording
    Record_Init();

    // Init Input Display
    Inputs_Init();

    // store hsd_update functions
    HSD_Update *hsd_update = stc_hsd_update;
    hsd_update->checkPause = Update_CheckPause;
    hsd_update->checkAdvance = Update_CheckAdvance;

    // determine controllers
    stc_hmn_controller = Fighter_GetControllerPort(hmn_data->ply);
    stc_cpu_controller = (stc_hmn_controller+1) % 4;
    stc_null_controller = (stc_cpu_controller+1) % 4;

    // set CPU AI to no_act 15
    cpu_data->cpu.ai = 15;

    // change team - homing missiles don't move towards characters on same team
    hmn_data->team = 0;
    cpu_data->team = 1;

    // check to immediately load recording
    if (*onload_fileno != -1)
    {
        Record_MemcardLoad(*onload_slot, *onload_fileno);
        LabOptions_Record[OPTREC_SAVE_LOAD] = Record_Load;

        // When we load rwing savestates, we don't want infinite shields by default. This would cause desyncs galore.
        LabOptions_CPU[OPTCPU_SHIELD].option_val = CPUINFSHIELD_OFF;
    }

    // Aitch: VERY nice for debugging. Please don't remove.
    TMLOG("HMN: %x\tCPU: %x\n", (u32)hmn_data, (u32)cpu_data);

    return;
}

// Update Function
void Event_Update()
{
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    HSD_Pad *pad = PadGet(hmn_data->pad_index, PADGET_MASTER);
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;

    if (Pause_CheckStatus(1) != 2) {
        float speed = LabOptions_GameSpeeds[LabOptions_General[OPTGEN_SPEED].option_val];
        HSD_SetSpeedEasy(speed);
    } else {
        HSD_SetSpeedEasy(1.0);
    }

    // update DI draw
    DIDraw_Update();

    // update info display
    InfoDisplay_Update(infodisp_gobj_hmn, LabOptions_InfoDisplayHMN, Fighter_GetGObj(0), NULL);
    InfoDisplay_Update(infodisp_gobj_cpu, LabOptions_InfoDisplayCPU, Fighter_GetGObj(1), infodisp_gobj_hmn);

    // update advanced cam
    Update_Camera();

    // Check for savestates
    Savestates_Update();
}

void Event_Think_LabState_Normal(GOBJ *event) {
    // Aitch: TODO - is there a good reason why we need both Record_Think and this function?
    // There could be a priority issue, where we need to call Record_Think after the cpu inputs are decided for whatever reason.
    // I think it makes sense to fold the two into a single function, if possible.
    // Or, call Record_Think from this function rather than it being a callback.

    LabData *eventData = event->userdata;
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    HSD_Pad *pad = PadGet(hmn_data->pad_index, PADGET_ENGINE);

    static int move_timer = 0;
    const int MOVE_THRESHOLD = 10;
    // Move CPU
    if (lockout_timer > 0)
    {
        lockout_timer--;
    }
    else
    {
        if (pad->held & HSD_BUTTON_DPAD_DOWN)
        {
            move_timer++;
            if (move_timer == MOVE_THRESHOLD)
            {
                // handle timer logic
                move_timer = 0;
                lockout_timer = LOCKOUT_DURATION;

                // ensure CPU is not dead
                if (cpu_data->flags.dead == 0)
                {
                    // ensure player is grounded
                    int isGround = 0;
                    if (hmn_data->phys.air_state == 0)
                    {

                        // check for ground in front of player
                        Vec3 coll_pos;
                        int line_index;
                        int line_kind;
                        Vec3 line_unk;
                        float fromX = (hmn_data->phys.pos.X) + (hmn_data->facing_direction * 16);
                        float toX = fromX;
                        float fromY = (hmn_data->phys.pos.Y + 5);
                        float toY = fromY - 10;
                        isGround = GrColl_RaycastGround(&coll_pos, &line_index, &line_kind, &line_unk, -1, -1, -1, 0, fromX, fromY, toX, toY, 0);
                        if (isGround == 1)
                        {

                            // do this for every subfighter (thanks for complicated code ice climbers)
                            int is_moved = 0;
                            for (int i = 0; i < 2; i++)
                            {
                                GOBJ *this_fighter = Fighter_GetSubcharGObj(cpu_data->ply, i);

                                if (this_fighter != 0)
                                {

                                    FighterData *this_fighter_data = this_fighter->userdata;

                                    if ((this_fighter_data->flags.sleep == 0) && (this_fighter_data->flags.dead == 0))
                                    {

                                        is_moved = 1;

                                        // place CPU here
                                        this_fighter_data->phys.pos = coll_pos;
                                        this_fighter_data->coll_data.ground_index = line_index;

                                        // facing player
                                        this_fighter_data->facing_direction = hmn_data->facing_direction * -1;

                                        // set grounded
                                        this_fighter_data->phys.air_state = 0;
                                        //Fighter_SetGrounded(this_fighter);

                                        // kill velocity
                                        Fighter_KillAllVelocity(this_fighter);

                                        // enter wait
                                        Fighter_EnterWait(this_fighter);

                                        // update ECB
                                        this_fighter_data->coll_data.topN_Curr = this_fighter_data->phys.pos; // move current ECB location to new position
                                        Coll_ECBCurrToPrev(&this_fighter_data->coll_data);
                                        this_fighter_data->cb.Coll(this_fighter);

                                        // update camera box
                                        Fighter_UpdateCameraBox(this_fighter);
                                        this_fighter_data->camera_subject->boundleft_curr = this_fighter_data->camera_subject->boundleft_proj;
                                        this_fighter_data->camera_subject->boundright_curr = this_fighter_data->camera_subject->boundright_proj;

                                        // init CPU logic (for nana's popo position history...)
                                        int cpu_kind = Fighter_GetCPUKind(this_fighter_data->ply);
                                        int cpu_level = Fighter_GetCPULevel(this_fighter_data->ply);
                                        Fighter_CPUInitialize(this_fighter_data, cpu_kind, cpu_level, 0);

                                        // place subfighter in the Z axis
                                        if (this_fighter_data->flags.ms == 1)
                                        {
                                            ftCommonData *ft_common = *stc_ftcommon;
                                            this_fighter_data->phys.pos.Z = ft_common->ms_zjostle_max * -1;
                                        }
                                    }
                                }
                            }

                            if (is_moved == 1)
                            {

                                // reset CPU think variables
                                eventData->cpu_state = CPUSTATE_START;
                                eventData->cpu_hitshield = 0;
                                eventData->cpu_hitnum = 0;
                                eventData->cpu_countertimer = 0;
                                eventData->cpu_hitshield = 0;
                                eventData->cpu_lasthit = -1;
                                eventData->cpu_lastshieldstun = -1;
                                eventData->cpu_hitkind = -1;
                                eventData->cpu_hitshieldnum = 0;
                                eventData->cpu_isactionable = 0;
                                eventData->cpu_sdinum = 0;
                            }
                        }
                    }

                    // play SFX
                    if (isGround == 0)
                    {
                        SFX_PlayCommon(3);
                    }
                    else
                    {
                        SFX_Play(221);
                    }
                }
            }
        }
        else
        {
            move_timer = 0;
        }
    }

    int hmn_mode = LabOptions_Record[OPTREC_HMNMODE].option_val;
    int cpu_mode = LabOptions_Record[OPTREC_CPUMODE].option_val;

    int cpu_control = false;

    switch (cpu_mode)
    {
    case (RECMODE_CPU_OFF):
    {
        Fighter_SetSlotType(cpu_data->ply, 1);
        cpu_data->pad_index = stc_cpu_controller;
        CPUThink(event, hmn, cpu);

        break;
    }
    case (RECMODE_CPU_PLAYBACK):
    {
        switch (LabOptions_Record[OPTREC_PLAYBACK_COUNTER].option_val) {
        case PLAYBACKCOUNTER_OFF:
            break;
        case PLAYBACKCOUNTER_ENDS:
            stc_playback_cancelled_cpu |= Record_PastLastInput(1);
            break;
        case PLAYBACKCOUNTER_ON_HIT_CPU:
            stc_playback_cancelled_cpu |= cpu_data->flags.hitlag_victim;
            break;
        case PLAYBACKCOUNTER_ON_HIT_HMN:
            stc_playback_cancelled_cpu |= hmn_data->flags.hitlag_victim;
            break;
        case PLAYBACKCOUNTER_ON_HIT_EITHER:
            stc_playback_cancelled_cpu |= cpu_data->flags.hitlag_victim || hmn_data->flags.hitlag_victim;
            break;
        }

        if (!stc_playback_cancelled_cpu) {
            Fighter_SetSlotType(cpu_data->ply, 0);
            cpu_data->pad_index = stc_cpu_controller;
        } else {
            Fighter_SetSlotType(cpu_data->ply, 1);
            cpu_data->pad_index = stc_cpu_controller;
            CPUThink(event, hmn, cpu);
        }

        break;
    }
    case (RECMODE_CPU_CONTROL):
    case (RECMODE_CPU_RECORD):
    {
        cpu_control = true;

        Fighter_SetSlotType(cpu_data->ply, 0);
        cpu_data->pad_index = stc_hmn_controller;

        break;
    }
    }

    if (!cpu_control) {
        HSD_Pad *hmn_pad = PadGet(stc_hmn_controller, PADGET_MASTER);

        int sticks = fabs(hmn_pad->fstickX) >= STICK_DEADZONE
            || fabs(hmn_pad->fstickY) >= STICK_DEADZONE
            || fabs(hmn_pad->fsubstickX) >= STICK_DEADZONE
            || fabs(hmn_pad->fsubstickY) >= STICK_DEADZONE;
        int triggers = hmn_pad->ftriggerLeft >= TRIGGER_DEADZONE
            || hmn_pad->ftriggerRight >= TRIGGER_DEADZONE;
        int buttons = hmn_pad->held & (HSD_BUTTON_A | HSD_BUTTON_B | HSD_BUTTON_X | HSD_BUTTON_Y | 
                HSD_BUTTON_DPAD_UP | HSD_TRIGGER_L | HSD_TRIGGER_R | HSD_TRIGGER_Z);

        if (hmn_mode == RECMODE_HMN_PLAYBACK && (buttons | triggers | sticks)) {
            stc_playback_cancelled_hmn = true;
        }

        if (stc_playback_cancelled_hmn || hmn_mode != RECMODE_HMN_PLAYBACK)
            hmn_data->pad_index = stc_hmn_controller;
        else
            hmn_data->pad_index = stc_null_controller;
    } else {
        hmn_data->pad_index = stc_null_controller;
    }

    for (int i = 0; i < REC_SLOTS; i++) {
        int slot_menu_idx = OPTSLOTCHANCE_1 + i;

        // if slot newly created

        if (rec_data.hmn_inputs[i]->num != 0 && LabOptions_SlotChancesHMN[slot_menu_idx].disable) {
            u16 *chances[REC_SLOTS];
            int chance_count = enabled_slot_chances(LabOptions_SlotChancesHMN, chances);

            LabOptions_SlotChancesHMN[slot_menu_idx].disable = 0;
            if (chance_count) {
                LabOptions_SlotChancesHMN[slot_menu_idx].option_val = 100 / chance_count;
                chances[chance_count++] = &LabOptions_SlotChancesHMN[slot_menu_idx].option_val;
                distribute_chances(chances, chance_count);
            } else {
                LabOptions_SlotChancesHMN[slot_menu_idx].option_val = 100;
            }
        }

        if (rec_data.cpu_inputs[i]->num != 0 && LabOptions_SlotChancesCPU[slot_menu_idx].disable) {
            u16 *chances[REC_SLOTS];
            int chance_count = enabled_slot_chances(LabOptions_SlotChancesCPU, chances);

            LabOptions_SlotChancesCPU[slot_menu_idx].disable = 0;
            if (chance_count) {
                LabOptions_SlotChancesCPU[slot_menu_idx].option_val = 100 / chance_count;
                chances[chance_count++] = &LabOptions_SlotChancesCPU[slot_menu_idx].option_val;
                distribute_chances(chances, chance_count);
            } else {
                LabOptions_SlotChancesCPU[slot_menu_idx].option_val = 100;
            }
        }
    }
}

// Think Function
void Event_Think(GOBJ *event)
{
    LabData *eventData = event->userdata;

    Lab_CustomOSDsThink();

    // get fighter data
    GOBJ *hmn = Fighter_GetGObj(0);
    FighterData *hmn_data = hmn->userdata;
    GOBJ *cpu = Fighter_GetGObj(1);
    FighterData *cpu_data = cpu->userdata;
    HSD_Pad *pad = PadGet(hmn_data->pad_index, PADGET_ENGINE);

    // We allow negative values to track how long we have not been in lockout for.
    // If the CPU is in hitlag, do not finish the lockout. This prevents insta techs
    // when the tech windows is the 1f between hitlag and knockdown.
    if (cpu_data->flags.hitlag == 0 || eventData->cpu_tech_lockout > 2)
        eventData->cpu_tech_lockout--;

    // Disable the D-pad up button according to the OPTGEN_TAUNT value
    if (LabOptions_General[OPTGEN_TAUNT].option_val == 1)
    {
      pad->held &= ~PAD_BUTTON_DPAD_UP;
    }

    // lock percent if enabled
    if (LabOptions_CPU[OPTCPU_LOCKPCNT].option_val)
    {
        cpu_data->dmg.percent = cpu_locked_percent;
        Fighter_SetHUDDamage(1, cpu_locked_percent);
    }

    if (LabOptions_General[OPTGEN_HMNPCNTLOCK].option_val)
    {
        hmn_data->dmg.percent = hmn_locked_percent;
        Fighter_SetHUDDamage(0, hmn_locked_percent);
    }        

    // update menu's percent
    LabOptions_General[OPTGEN_HMNPCNT].option_val = hmn_data->dmg.percent;
    LabOptions_CPU[OPTCPU_PCNT].option_val = cpu_data->dmg.percent;
    
    // reset stale moves
    if (LabOptions_General[OPTGEN_STALE].option_val == 0)
    {
        for (int i = 0; i < 6; i++)
        {
            // check if fighter exists
            GOBJ *fighter = Fighter_GetGObj(i);
            if (fighter != 0)
            {
                // reset stale move table
                int *staleMoveTable = Fighter_GetStaleMoveTable(i);
                memset(staleMoveTable, 0, 0x2C);
            }
        }
    }

    // apply intangibility
    if (LabOptions_CPU[OPTCPU_INTANG].option_val == 1)
    {
        cpu_data->flags.no_reaction_always = 1;
        cpu_data->flags.nudge_disable = 1;
        cpu_data->grab.vuln = 0x1FF;
        cpu_data->dmg.percent = 0;
        Fighter_SetHUDDamage(cpu_data->ply, 0);

        // if new state, apply colanim
        if (cpu_data->TM.state_frame <= 1)
        {
            Fighter_ColAnim_Apply(cpu_data, INTANG_COLANIM, 0);
        }
    }
    else
    {
        cpu_data->flags.no_reaction_always = 0;
        cpu_data->flags.nudge_disable = 0;
    }

    // apply invisibility
    int invisible = LabOptions_Tech[OPTTECH_INVISIBLE].option_val;
    if (invisible) {
        if (is_tech_anim(cpu_data->state_id))
        {
            // get distinguishable frame from lookup table
            int char_id = cpu_data->kind;
            if (char_id >= sizeof(tech_frame_distinguishable)/sizeof(*tech_frame_distinguishable))
                assert("invalid character kind causing read out of bounds");

            int frame_distinguishable = tech_frame_distinguishable[char_id];

            // apply if tech anim is after distinguishable frame and invulnerable
            if (frame_distinguishable != -1) {
                // state frame is 0-indexed but frame distinguishable is 1-indexed, so add 1
                int state_frame = cpu_data->TM.state_frame + 1;
                int delay = LabOptions_Tech[OPTTECH_INVISIBLE_DELAY].option_val;

                int after = state_frame > frame_distinguishable + delay;
                int vuln = cpu_data->TM.state_frame >= 19;

                cpu_data->flags.invisible = after && state_frame < 20;

                int sound = LabOptions_Tech[OPTTECH_SOUND].option_val;
                if (sound && state_frame == frame_distinguishable)
                    SFX_PlayCommon(1);
            }
        }
    }

    // update shield deterioration
    switch (LabOptions_CPU[OPTCPU_SHIELD].option_val)
    {
        case CPUINFSHIELD_OFF:
            break;
        case CPUINFSHIELD_UNTIL_HIT:
            if (eventData->cpu_hitshield == 0) {
                hmn_data->shield.health = 60;
                cpu_data->shield.health = 60;
            }
            break;
        case CPUINFSHIELD_ON:
            cpu_data->shield.health = 60;
            hmn_data->shield.health = 60;
            break;
    }

    if (LabOptions_CPU[OPTCPU_SET_POS].onOptionSelect == Lab_FinishMoveCPU) {
        // set CPU position

        if (cpu_data->phys.air_state == 0) // if is grounded
            Fighter_EnterFall(cpu);

        Fighter_KillAllVelocity(cpu);
        cpu_data->phys.pos.Y += cpu_data->attr.gravity; // remove small initial gravity delta

        HSD_Pad *pad = PadGet(stc_hmn_controller, PADGET_MASTER);
        cpu_data->phys.pos.X += pad->fstickX * 1.5;
        cpu_data->phys.pos.Y += pad->fstickY * 1.5;
    } else if (LabOptions_CPU[OPTCPU_CTRL_BY].option_val != CTRLBY_NONE) {
        // cpu controlled by another port

        int port = LabOptions_CPU[OPTCPU_CTRL_BY].option_val - CTRLBY_PORT_1;
        Fighter_SetSlotType(cpu_data->ply, 0);
        cpu_data->pad_index = port;
    } else {
        // normal cpu

        Event_Think_LabState_Normal(event);
    }
}

// Initial Menu
EventMenu *Event_Menu = &LabMenu_Main;

// helper fns ######################################################

static int is_tech_anim(int state) {
    return state == ASID_PASSIVE || state == ASID_PASSIVESTANDF || state == ASID_PASSIVESTANDB;
}

// Clean up percentages so the total is 100, evenly distributing the change.
static void distribute_chances(u16 *chances[], unsigned int chance_count) {
    if (chance_count == 0) return;

    int sum = 0;
    for (int t = 0; t < chance_count; ++t)
        sum += *chances[t];

    int delta = 100 - sum;
    int change;
    if (sum > 100) { change = -1; } else { change = 1; }

    // Aitch: We could always start on the first chance, but when adding a new slot we should distribute starting
    // on the largest (if decreasing) or smallest (if increasing) option.
    // This fixes percents when rounding: for example, three options will have chances 33, 33, and 34.
    // When we add a new event to this, if we always start on the first option,
    // then the resulting percents will be 24, 25, 25, 26.
    // When we start on the largest/smallest option then they will all be 25 as expected.
    int option = 0;
    int min = 999;
    for (int t = 0; t < chance_count; ++t) {
        int check = (int)*chances[t] * change;
        if (check < min) {
            min = check;
            option = t;
        }
    }

    // Keep adding/removing from next option until all needed change to revert to 100% is applied.
    // Algorithm is pretty garbage, but it should be quick enough.
    while (delta)
    {
        u16 *opt_val = chances[option];
        int prev_chance = (int)*opt_val;
        int new_chance = prev_chance + change;

        if (0 <= new_chance && new_chance <= 100) {
            *opt_val = new_chance;
            delta -= change;
        }

        option = (option + 1) % chance_count;
    }
}

// Clean up percentages so the total is always 100.
// The next percentage is modified.
static void rebound_chances(u16 *chances[], unsigned int chance_count, int just_changed_option) {
    if (chance_count == 0) return;

    int sum = 0;
    for (int t = 0; t < chance_count; ++t)
        sum += *chances[t];

    int delta = 100 - sum;
    // keep adding/removing from next option chance until all needed change to revert to 100% is applied
    while (delta)
    {
        int rebound_option = (just_changed_option + 1) % chance_count;

        u16 *opt_val = chances[rebound_option];
        int prev_chance = (int)*opt_val;

        int new_chance = prev_chance + delta;
        if (new_chance < 0) {
            *opt_val = 0;
            delta = new_chance;
        } else if (new_chance > 100) {
            *opt_val = 100;
            delta = new_chance - 100;
        } else {
            *opt_val = (u16)new_chance;
            delta = 0;
        }

        just_changed_option = rebound_option;
    }
}

static float get_angle_out_of_deadzone(float angle, int lastSDIWasCardinal)
{
    // get out of deadzone if last sdi was cardinal
    if (angle > M_1DEGREE * 73.5 || angle < M_1DEGREE * 106.5 && lastSDIWasCardinal)
    {
        angle = M_1DEGREE * 73;
    }
    else if (angle > M_1DEGREE * 163.5 || angle < M_1DEGREE * 196.5 && lastSDIWasCardinal)
    {
        angle = M_1DEGREE * 163;
    }
    else if (angle > M_1DEGREE * 253.5 || angle < M_1DEGREE * 296.5 && lastSDIWasCardinal)
    {
        angle = M_1DEGREE * 253;
    }
    else if (angle > M_1DEGREE * 343.5 || angle < M_1DEGREE * 16.5 && lastSDIWasCardinal)
    {
        angle = M_1DEGREE * 343;
    }
    return angle;
}

// lz77 functions credited to https://github.com/andyherbert/lz1
static int x_to_the_n(int x, int n)
{
    int i; /* Variable used in loop counter */
    int number = 1;

    for (i = 0; i < n; ++i)
        number *= x;

    return (number);
}

static u32 lz77_compress(u8 *uncompressed_text, u32 uncompressed_size, u8 *compressed_text, u8 pointer_length_width)
{
    u16 pointer_pos, temp_pointer_pos, output_pointer, pointer_length, temp_pointer_length;
    u32 compressed_pointer, output_size, coding_pos, output_lookahead_ref, look_behind, look_ahead;
    u16 pointer_pos_max, pointer_length_max;
    pointer_pos_max = x_to_the_n(2, 16 - pointer_length_width);
    pointer_length_max = x_to_the_n(2, pointer_length_width);

    *((u32 *)compressed_text) = uncompressed_size;
    *(compressed_text + 4) = pointer_length_width;
    compressed_pointer = output_size = 5;

    for (coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        pointer_pos = 0;
        pointer_length = 0;
        for (temp_pointer_pos = 1; (temp_pointer_pos < pointer_pos_max) && (temp_pointer_pos <= coding_pos); ++temp_pointer_pos)
        {
            look_behind = coding_pos - temp_pointer_pos;
            look_ahead = coding_pos;
            for (temp_pointer_length = 0; uncompressed_text[look_ahead++] == uncompressed_text[look_behind++]; ++temp_pointer_length)
                if (temp_pointer_length == pointer_length_max)
                    break;
            if (temp_pointer_length > pointer_length)
            {
                pointer_pos = temp_pointer_pos;
                pointer_length = temp_pointer_length;
                if (pointer_length == pointer_length_max)
                    break;
            }
        }
        coding_pos += pointer_length;
        if ((coding_pos == uncompressed_size) && pointer_length)
        {
            output_pointer = (pointer_length == 1) ? 0 : ((pointer_pos << pointer_length_width) | (pointer_length - 2));
            output_lookahead_ref = coding_pos - 1;
        }
        else
        {
            output_pointer = (pointer_pos << pointer_length_width) | (pointer_length ? (pointer_length - 1) : 0);
            output_lookahead_ref = coding_pos;
        }
        *((u16 *)(compressed_text + compressed_pointer)) = output_pointer;
        compressed_pointer += 2;
        *(compressed_text + compressed_pointer++) = *(uncompressed_text + output_lookahead_ref);
        output_size += 3;
    }

    return output_size;
}

static u32 lz77_decompress(u8 *compressed_text, u8 *uncompressed_text)
{
    u8 pointer_length_width;
    u16 input_pointer, pointer_length, pointer_pos, pointer_length_mask;
    u32 compressed_pointer, coding_pos, pointer_offset, uncompressed_size;

    uncompressed_size = *((u32 *)compressed_text);
    pointer_length_width = *(compressed_text + 4);
    compressed_pointer = 5;

    pointer_length_mask = x_to_the_n(2, pointer_length_width) - 1;

    for (coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        input_pointer = *((u16 *)(compressed_text + compressed_pointer));
        compressed_pointer += 2;
        pointer_pos = input_pointer >> pointer_length_width;
        pointer_length = pointer_pos ? ((input_pointer & pointer_length_mask) + 1) : 0;
        if (pointer_pos)
            for (pointer_offset = coding_pos - pointer_pos; pointer_length > 0; --pointer_length)
                uncompressed_text[coding_pos++] = uncompressed_text[pointer_offset++];
        *(uncompressed_text + coding_pos) = *(compressed_text + compressed_pointer++);
    }

    return coding_pos;
}

// adapted from decomp github.com/doldecomp/melee/blob/7f0e2ddb83fa974b64b05d0a7e5b374cf12c0541/src/melee/ft/ftwalljump.c
static bool can_walljump(GOBJ* fighter) {
    FighterData *fighter_data = fighter->userdata;
    CollData* coll_data = &fighter_data->coll_data;

    return fighter_data->flags.can_walljump && (coll_data->envFlags & ((1u << 11) | (1u << 5))) != 0;
}

static bool check_has_jump(GOBJ *g) {
    FighterData *data = g->userdata;
    int max_jumps = data->attr.max_jumps;
    int jumps_used = data->jump.jumps_used;

    return jumps_used < max_jumps;
}
