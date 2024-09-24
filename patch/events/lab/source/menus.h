// Main Menu
static char **LabOptions_OffOn[] = {"Off", "On"};
static EventOption LabOptions_Main[] = {
    {
        .option_kind = OPTKIND_MENU,                                                     // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                                                  // number of values for this option
        .option_val = 0,                                                                 // value of this option
        .menu = &LabMenu_General,                                                        // pointer to the menu that pressing A opens
        .option_name = {"General"},                                                      // pointer to a string
        .desc = "Toggle player percent, overlays,\nframe advance, and camera settings.", // string describing what this option does
        .option_values = 0,                                                              // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_MENU,       // the type of option this is; menu, string list, integers list, etc
        .value_num = 0,                    // number of values for this option
        .option_val = 0,                   // value of this option
        .menu = &LabMenu_CPU,              // pointer to the menu that pressing A opens
        .option_name = {"CPU Options"},    // pointer to a string
        .desc = "Configure CPU behavior.", // string describing what this option does
        .option_values = 0,                // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_MENU,           // the type of option this is; menu, string list, integers list, etc
        .value_num = 0,                        // number of values for this option
        .option_val = 0,                       // value of this option
        .menu = &LabMenu_Record,               // pointer to the menu that pressing A opens
        .option_name = "Recording",            // pointer to a string
        .desc = "Record and playback inputs.", // string describing what this option does
        .option_values = 0,                    // pointer to an array of strings
        .onOptionChange = 0,
    },
    // info display
    {
        .option_kind = OPTKIND_MENU,                          // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                       // number of values for this option
        .option_val = 0,                                      // value of this option
        .menu = &LabMenu_InfoDisplay,                         // pointer to the menu that pressing A opens
        .option_name = "Info Display",                        // pointer to a string
        .desc = "Display various game information onscreen.", // string describing what this option does
        .option_values = 0,                                   // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_FUNC,                                                                                       // the type of option this is; menu, string list, integers list, etc
        .value_num = 0,                                                                                                    // number of values for this option
        .option_val = 0,                                                                                                   // value of this option
        .menu = 0,                                                                                                         // pointer to the menu that pressing A opens
        .option_name = "Help",                                                                                             // pointer to a string
        .desc = "D-Pad Left - Load State\nD-Pad Right - Save State\nD-Pad Down - Move CPU\nHold R in the menu for turbo.", // string describing what this option does
        .option_values = 0,                                                                                                // pointer to an array of strings
        .onOptionChange = Lab_Exit,
    },
    {
        .option_kind = OPTKIND_FUNC,                  // the type of option this is; menu, string list, integers list, etc
        .value_num = 0,                               // number of values for this option
        .option_val = 0,                              // value of this option
        .menu = 0,                                    // pointer to the menu that pressing A opens
        .option_name = "Exit",                        // pointer to a string
        .desc = "Return to the Event Select Screen.", // string describing what this option does
        .option_values = 0,                           // pointer to an array of strings
        .onOptionChange = 0,
        .onOptionSelect = Lab_Exit,
    },
};
static EventMenu LabMenu_Main = {
    .name = "Main Menu",                                         // the name of this menu
    .option_num = sizeof(LabOptions_Main) / sizeof(EventOption), // number of options this menu contains
    .scroll = 0,                                                 // runtime variable used for how far down in the menu to start
    .state = 0,                                                  // bool used to know if this menu is focused, used at runtime
    .cursor = 0,                                                 // index of the option currently selected, used at runtime
    .options = &LabOptions_Main,                                 // pointer to all of this menu's options
    .prev = 0,                                                   // pointer to previous menu, used at runtime
};
// General
static char **LabOptions_CamMode[] = {"Normal", "Zoom", "Fixed", "Advanced"};
static char **LabOptions_FrameAdvButton[] = {"L", "Z", "X", "Y"};
static EventOption LabOptions_General[] = {
    // frame advance
    {
        .option_kind = OPTKIND_STRING,                                                                 // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_OffOn) / 4,                                                     // number of values for this option
        .option_val = 0,                                                                               // value of this option
        .menu = 0,                                                                                     // pointer to the menu that pressing A opens
        .option_name = "Frame Advance",                                                                // pointer to a string
        .desc = "Enable frame advance. Press to advance one\nframe. Hold to advance at normal speed.", // string describing what this option does
        .option_values = LabOptions_OffOn,                                                             // pointer to an array of strings
        .onOptionChange = Lab_ChangeFrameAdvance,
    },
    // frame advance button
    {
        .option_kind = OPTKIND_STRING,                        // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_FrameAdvButton) / 4,   // number of values for this option
        .option_val = 0,                                      // value of this option
        .menu = 0,                                            // pointer to the menu that pressing A opens
        .option_name = "Frame Advance Button",                // pointer to a string
        .desc = "Choose which button will advance the frame", // string describing what this option does
        .option_values = LabOptions_FrameAdvButton,           // pointer to an array of strings
        .onOptionChange = 0,
        .disable = 1,
    },
    // p1 percent
    {
        .option_kind = OPTKIND_INT,             // the type of option this is; menu, string list, integer list, etc
        .value_num = 999,                       // number of values for this option
        .option_val = 0,                        // value of this option
        .menu = 0,                              // pointer to the menu that pressing A opens
        .option_name = "Player Percent",        // pointer to a string
        .desc = "Adjust the player's percent.", // string describing what this option does
        .option_values = "%d%%",                // pointer to an array of strings
        .onOptionChange = Lab_ChangePlayerPercent,
    },
    // model display
    {
        .option_kind = OPTKIND_STRING,                      // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                     // number of values for this option
        .option_val = 1,                                    // value of this option
        .menu = 0,                                          // pointer to the menu that pressing A opens
        .option_name = "Model Display",                     // pointer to a string
        .desc = "Toggle player and item model visibility.", // string describing what this option does
        .option_values = LabOptions_OffOn,                  // pointer to an array of strings
        .onOptionChange = Lab_ChangeModelDisplay,
    },
    // fighter collision
    {
        .option_kind = OPTKIND_STRING,                                                                                                                           // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                                                                                                                          // number of values for this option
        .option_val = 0,                                                                                                                                         // value of this option
        .menu = 0,                                                                                                                                               // pointer to the menu that pressing A opens
        .option_name = "Fighter Collision",                                                                                                                      // pointer to a string
        .desc = "Toggle hitbox and hurtbox visualization.\nYellow = hurt, red = hit, purple = grab, \nwhite = trigger, green = reflect,\nblue = shield/absorb.", // string describing what this option does
        .option_values = LabOptions_OffOn,                                                                                                                       // pointer to an array of strings
        .onOptionChange = Lab_ChangeHitDisplay,
    },
    // environment collision
    {
        .option_kind = OPTKIND_STRING,                                                                                          // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                                                                                         // number of values for this option
        .option_val = 0,                                                                                                        // value of this option
        .menu = 0,                                                                                                              // pointer to the menu that pressing A opens
        .option_name = "Environment Collision",                                                                                 // pointer to a string
        .desc = "Toggle environment collision visualization.\nAlso displays the players' ECB (environmental \ncollision box).", // string describing what this option does
        .option_values = LabOptions_OffOn,                                                                                      // pointer to an array of strings
        .onOptionChange = Lab_ChangeEnvCollDisplay,
    },
    // camera mode
    {
        .option_kind = OPTKIND_STRING,                                                                                                      // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_CamMode) / 4,                                                                                        // number of values for this option
        .option_val = 0,                                                                                                                    // value of this option
        .menu = 0,                                                                                                                          // pointer to the menu that pressing A opens
        .option_name = "Camera Mode",                                                                                                       // pointer to a string
        .desc = "Adjust the camera's behavior.\nIn advanced mode, use C-Stick while holding\nA/B/Y to pan, rotate and zoom, respectively.", // string describing what this option does
        .option_values = LabOptions_CamMode,                                                                                                // pointer to an array of strings
        .onOptionChange = Lab_ChangeCamMode,
    },
    // hud
    {
        .option_kind = OPTKIND_STRING,                          // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                         // number of values for this option
        .option_val = 1,                                        // value of this option
        .menu = 0,                                              // pointer to the menu that pressing A opens
        .option_name = "HUD",                                   // pointer to a string
        .desc = "Toggle player percents and timer visibility.", // string describing what this option does
        .option_values = LabOptions_OffOn,                      // pointer to an array of strings
        .onOptionChange = Lab_ChangeHUD,
    },
    // di display
    {
        .option_kind = OPTKIND_STRING,                                                                                      // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                                                                                     // number of values for this option
        .option_val = 0,                                                                                                    // value of this option
        .menu = 0,                                                                                                          // pointer to the menu that pressing A opens
        .option_name = "DI Display",                                                                                        // pointer to a string
        .desc = "Display knockback trajectories.\nUse frame advance to see the effects of DI\nin realtime during hitstop.", // string describing what this option does
        .option_values = LabOptions_OffOn,                                                                                  // pointer to an array of strings
        .onOptionChange = 0,
    },
    // input display
    {
        .option_kind = OPTKIND_STRING,             // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                            // number of values for this option
        .option_val = 0,                           // value of this option
        .menu = 0,                                 // pointer to the menu that pressing A opens
        .option_name = "Input Display",            // pointer to a string
        .desc = "Display player inputs onscreen.", // string describing what this option does
        .option_values = LabOptions_OffOn,         // pointer to an array of strings
        .onOptionChange = 0,
    },
    // move staling
    {
        .option_kind = OPTKIND_STRING,                                                          // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                                                         // number of values for this option
        .option_val = 1,                                                                        // value of this option
        .menu = 0,                                                                              // pointer to the menu that pressing A opens
        .option_name = "Move Staling",                                                          // pointer to a string
        .desc = "Toggle the staling of moves. Attacks become \nweaker the more they are used.", // string describing what this option does
        .option_values = LabOptions_OffOn,                                                      // pointer to an array of strings
        .onOptionChange = 0,
    },
};
static EventMenu LabMenu_General = {
    .name = "General",                                              // the name of this menu
    .option_num = sizeof(LabOptions_General) / sizeof(EventOption), // number of options this menu contains
    .scroll = 0,                                                    // runtime variable used for how far down in the menu to start
    .state = 0,                                                     // bool used to know if this menu is focused, used at runtime
    .cursor = 0,                                                    // index of the option currently selected, used at runtime
    .options = &LabOptions_General,                                 // pointer to all of this menu's options
    .prev = 0,                                                      // pointer to previous menu, used at runtime
};
// Info Display
static char **LabValues_InfoDisplay[] = {"None", "Position", "State Name", "State Frame", "Velocity - Self", "Velocity - KB", "Velocity - Total", "Engine LStick", "System LStick", "Engine CStick", "System CStick", "Engine Trigger", "System Trigger", "Ledgegrab Timer", "Intangibility Timer", "Hitlag", "Hitstun", "Shield Health", "Shield Stun", "Grip Strength", "ECB Lock", "ECB Bottom", "Jumps", "Walljumps", "Jab Counter", "Line Info", "Blastzone Left/Right", "Blastzone Up/Down"};
static char **LabValues_InfoPresets[] = {"None", "Custom", "Ledge", "Damage"};
//static char **LabValues_InfoPosition[] = {"Top Left", "Top Mid", "Top Right", "Bottom Left", "Bottom Mid", "Bottom Right"};
static char **LabValues_InfoSize[] = {"Small", "Medium", "Large"};
static char **LabValues_InfoPlayers[] = {"Player 1", "Player 2", "Player 3", "Player 4"};
static EventOption LabOptions_InfoDisplay[] = {
    {
        .option_kind = OPTKIND_STRING,                           // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoPlayers) / 4,          // number of values for this option
        .option_val = 0,                                         // value of this option
        .menu = 0,                                               // pointer to the menu that pressing A opens
        .option_name = "Player",                                 // pointer to a string
        .desc = "Toggle which player's information to display.", // string describing what this option does
        .option_values = LabValues_InfoPlayers,                  // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoPlayer,
    },
    {
        .option_kind = OPTKIND_STRING,                                                                                                        // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoSize) / 4,                                                                                          // number of values for this option
        .option_val = 1,                                                                                                                      // value of this option
        .menu = 0,                                                                                                                            // pointer to the menu that pressing A opens
        .option_name = "Size",                                                                                                                // pointer to a string
        .desc = "Change the size of the info display window.\nLarge is recommended for CRT.\nMedium/Small recommended for Dolphin Emulator.", // string describing what this option does
        .option_values = LabValues_InfoSize,                                                                                                  // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoSizePos,
    },
    {
        .option_kind = OPTKIND_STRING,                       // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoPresets) / 4,      // number of values for this option
        .option_val = 0,                                     // value of this option
        .menu = 0,                                           // pointer to the menu that pressing A opens
        .option_name = "Display Preset",                     // pointer to a string
        .desc = "Choose between pre-configured selections.", // string describing what this option does
        .option_values = LabValues_InfoPresets,              // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoPreset,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 1",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 2",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 3",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 4",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 5",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 6",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 7",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
    {
        .option_kind = OPTKIND_STRING,                   // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_InfoDisplay) / 4,  // number of values for this option
        .option_val = 0,                                 // value of this option
        .menu = 0,                                       // pointer to the menu that pressing A opens
        .option_name = "Row 8",                          // pointer to a string
        .desc = "Adjust what is displayed in this row.", // string describing what this option does
        .option_values = LabValues_InfoDisplay,          // pointer to an array of strings
        .onOptionChange = Lab_ChangeInfoRow,
    },
};
static EventMenu LabMenu_InfoDisplay = {
    .name = "Info Display",             // the name of this menu
    .option_num = 11,                   // number of options this menu contains
    .scroll = 0,                        // runtime variable used for how far down in the menu to start
    .state = 0,                         // bool used to know if this menu is focused, used at runtime
    .cursor = 0,                        // index of the option currently selected, used at runtime
    .options = &LabOptions_InfoDisplay, // pointer to all of this menu's options
    .prev = 0,                          // pointer to previous menu, used at runtime
};
// CPU
static char **LabValues_Shield[] = {"Off", "On Until Hit", "On"};
static char **LabValues_CPUBehave[] = {"Stand", "Shield", "Crouch", "Jump"};
static char **LabValues_TDI[] = {"Random", "Inwards", "Outwards", "Floorhug", "Custom", "None"};
static char **LabValues_SDIFreq[] = {"None", "Low", "Medium", "High"};
static char **LabValues_SDIDir[] = {"Random", "Away", "Towards"};
static char **LabValues_Tech[] = {"Random", "Neutral", "Away", "Towards", "None"};
static char **LabValues_Getup[] = {"Random", "Stand", "Away", "Towards", "Attack"};
static char **LabValues_CounterGround[] = {"None", "Spotdodge", "Shield", "Grab", "Up B", "Down B", "Up Smash", "Down Smash", "Forward Smash", "Roll Away", "Roll Towards", "Roll Random", "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air", "Jab", "Forward Tilt", "Up Tilt", "Down Tilt", "Short Hop", "Full Hop"};
static char **LabValues_CounterAir[] = {"None", "Airdodge", "Jump Away", "Jump Towards", "Jump Neutral", "Up B", "Down B", "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air", "Tumble Fastfall", "Wiggle Fastfall"};
static char **LabValues_CounterShield[] = {"None", "Grab", "Short Hop", "Full Hop", "Spotdodge", "Roll Away", "Roll Towards", "Roll Random", "Up B", "Down B", "Neutral Air", "Forward Air", "Down Air", "Back Air", "Up Air"};
static char **LabValues_GrabEscape[] = {"None", "Medium", "High", "Perfect"};
static EventOption LabOptions_CPU[] = {
    // cpu percent
    {
        .option_kind = OPTKIND_INT,          // the type of option this is; menu, string list, integer list, etc
        .value_num = 999,                    // number of values for this option
        .option_val = 0,                     // value of this option
        .menu = 0,                           // pointer to the menu that pressing A opens
        .option_name = "CPU Percent",        // pointer to a string
        .desc = "Adjust the CPU's percent.", // string describing what this option does
        .option_values = "%d%%",             // pointer to an array of strings
        .onOptionChange = Lab_ChangeCPUPercent,
    },
    {
        .option_kind = OPTKIND_STRING,                // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_CPUBehave) / 4, // number of values for this option
        .option_val = 0,                              // value of this option
        .menu = 0,                                    // pointer to the menu that pressing A opens
        .option_name = "Behavior",                    // pointer to a string
        .desc = "Adjust the CPU's default action.",   // string describing what this option does
        .option_values = LabValues_CPUBehave,         // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                    // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_Shield) / 4,        // number of values for this option
        .option_val = 1,                                  // value of this option
        .menu = 0,                                        // pointer to the menu that pressing A opens
        .option_name = {"Infinite Shields"},              // pointer to a string
        .desc = "Adjust how shield health deteriorates.", // string describing what this option does
        .option_values = LabValues_Shield,                // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                      // the type of option this is; menu, string list, integer list, etc
        .value_num = 2,                                     // number of values for this option
        .option_val = 0,                                    // value of this option
        .menu = 0,                                          // pointer to the menu that pressing A opens
        .option_name = {"Intangibility"},                   // pointer to a string
        .desc = "Toggle the CPU's ability to take damage.", // string describing what this option does
        .option_values = LabOptions_OffOn,                  // pointer to an array of strings
        .onOptionChange = Lab_ChangeCPUIntang,
    },
    // SDI Freq
    {
        .option_kind = OPTKIND_STRING,                                                 // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_SDIFreq) / 4,                                    // number of values for this option
        .option_val = 0,                                                               // value of this option
        .menu = 0,                                                                     // pointer to the menu that pressing A opens
        .option_name = "Smash DI Frequency",                                           // pointer to a string
        .desc = "Adjust how often the CPU will alter their position\nduring hitstop.", // string describing what this option does
        .option_values = LabValues_SDIFreq,                                            // pointer to an array of strings
        .onOptionChange = 0,
    },
    // SDI Direction
    {
        .option_kind = OPTKIND_STRING,                                                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_SDIDir) / 4,                                                   // number of values for this option
        .option_val = 0,                                                                             // value of this option
        .menu = 0,                                                                                   // pointer to the menu that pressing A opens
        .option_name = "Smash DI Direction",                                                         // pointer to a string
        .desc = "Adjust the direction in which the CPU will alter \ntheir position during hitstop.", // string describing what this option does
        .option_values = LabValues_SDIDir,                                                           // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                                        // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_TDI) / 4,                               // number of values for this option
        .option_val = 0,                                                      // value of this option
        .menu = 0,                                                            // pointer to the menu that pressing A opens
        .option_name = "Trajectory DI",                                       // pointer to a string
        .desc = "Adjust how the CPU will alter their knockback\ntrajectory.", // string describing what this option does
        .option_values = LabValues_TDI,                                       // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_FUNC,                                           // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                                        // number of values for this option
        .option_val = 0,                                                       // value of this option
        .menu = 0,                                                             // pointer to the menu that pressing A opens
        .option_name = "Custom TDI",                                           // pointer to a string
        .desc = "Create custom trajectory DI values for the\nCPU to perform.", // string describing what this option does
        .option_values = 0,                                                    // pointer to an array of strings
        .onOptionChange = 0,
        .onOptionSelect = Lab_SelectCustomTDI,
    },
    {
        .option_kind = OPTKIND_STRING,                                         // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_Tech) / 4,                               // number of values for this option
        .option_val = 0,                                                       // value of this option
        .menu = 0,                                                             // pointer to the menu that pressing A opens
        .option_name = "Tech Option",                                          // pointer to a string
        .desc = "Adjust what the CPU will do upon colliding\nwith the stage.", // string describing what this option does
        .option_values = LabValues_Tech,                                       // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                                      // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_Getup) / 4,                           // number of values for this option
        .option_val = 0,                                                    // value of this option
        .menu = 0,                                                          // pointer to the menu that pressing A opens
        .option_name = "Get Up Option",                                     // pointer to a string
        .desc = "Adjust what the CPU will do after missing\na tech input.", // string describing what this option does
        .option_values = LabValues_Getup,                                   // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_GrabEscape) / 4,               // number of values for this option
        .option_val = CPUMASH_NONE,                                  // value of this option
        .menu = 0,                                                   // pointer to the menu that pressing A opens
        .option_name = "Grab Escape",                                // pointer to a string
        .desc = "Adjust how the CPU will attempt to escape\ngrabs.", // string describing what this option does
        .option_values = LabValues_GrabEscape,                       // pointer to an array of strings
        .onOptionChange = 0,
    },
    /*
    {
        .option_kind = OPTKIND_STRING,                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_OffOn) / 4,                   // number of values for this option
        .option_val = 0,                                             // value of this option
        .menu = 0,                                                   // pointer to the menu that pressing A opens
        .option_name = "Auto Reset",                                 // pointer to a string
        .desc = "Automatically reset after the CPU is\nactionable.", // string describing what this option does
        .option_values = LabOptions_OffOn,                           // pointer to an array of strings
        .onOptionChange = 0,
    },
*/
    {
        .option_kind = OPTKIND_STRING,                                                     // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_CounterGround) / 4,                                  // number of values for this option
        .option_val = 1,                                                                   // value of this option
        .menu = 0,                                                                         // pointer to the menu that pressing A opens
        .option_name = "Counter Action (Ground)",                                          // pointer to a string
        .desc = "Select the action to be performed after a\ngrounded CPU's hitstun ends.", // string describing what this option does
        .option_values = LabValues_CounterGround,                                          // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                                                      // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_CounterAir) / 4,                                      // number of values for this option
        .option_val = 4,                                                                    // value of this option
        .menu = 0,                                                                          // pointer to the menu that pressing A opens
        .option_name = "Counter Action (Air)",                                              // pointer to a string
        .desc = "Select the action to be performed after an\nairborne CPU's hitstun ends.", // string describing what this option does
        .option_values = LabValues_CounterAir,                                              // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_CounterShield) / 4,                            // number of values for this option
        .option_val = 1,                                                             // value of this option
        .menu = 0,                                                                   // pointer to the menu that pressing A opens
        .option_name = "Counter Action (Shield)",                                    // pointer to a string
        .desc = "Select the action to be performed after the\nCPU's shield is hit.", // string describing what this option does
        .option_values = LabValues_CounterShield,                                    // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_INT,                                                  // the type of option this is; menu, string list, integer list, etc
        .value_num = 100,                                                            // number of values for this option
        .option_val = 0,                                                             // value of this option
        .menu = 0,                                                                   // pointer to the menu that pressing A opens
        .option_name = "Counter After Frames",                                       // pointer to a string
        .desc = "Adjust the amount of actionable frames before \nthe CPU counters.", // string describing what this option does
        .option_values = "%d Frames",                                                // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_INT,                                           // the type of option this is; menu, string list, integer list, etc
        .value_num = 100,                                                     // number of values for this option
        .option_val = 1,                                                      // value of this option
        .menu = 0,                                                            // pointer to the menu that pressing A opens
        .option_name = "Counter After Hits",                                  // pointer to a string
        .desc = "Adjust the amount of hits taken before the \nCPU counters.", // string describing what this option does
        .option_values = "%d Hits",                                           // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_INT,                                                           // the type of option this is; menu, string list, integer list, etc
        .value_num = 100,                                                                     // number of values for this option
        .option_val = 1,                                                                      // value of this option
        .menu = 0,                                                                            // pointer to the menu that pressing A opens
        .option_name = "Counter After Shield Hits",                                           // pointer to a string
        .desc = "Adjust the amount of hits the CPU's shield\nwill take before they counter.", // string describing what this option does
        .option_values = "%d Hits",                                                           // pointer to an array of strings
        .onOptionChange = 0,
    },
};
static EventMenu LabMenu_CPU = {
    .name = "CPU Options",                                      // the name of this menu
    .option_num = sizeof(LabOptions_CPU) / sizeof(EventOption), // number of options this menu contains
    .scroll = 0,                                                // runtime variable used for how far down in the menu to start
    .state = 0,                                                 // bool used to know if this menu is focused, used at runtime
    .cursor = 0,                                                // index of the option currently selected, used at runtime
    .options = &LabOptions_CPU,                                 // pointer to all of this menu's options
    .prev = 0,                                                  // pointer to previous menu, used at runtime
};
// Recording
static char **LabValues_RecordSlot[] = {"Random", "Slot 1", "Slot 2", "Slot 3", "Slot 4", "Slot 5", "Slot 6"};
static char **LabValues_HMNRecordMode[] = {"Off", "Record", "Playback"};
static char **LabValues_CPURecordMode[] = {"Off", "Control", "Record", "Playback"};

static const EventOption Record_Save = {
    .option_kind = OPTKIND_FUNC,                                             // the type of option this is; menu, string list, integer list, etc
    .value_num = 0,                                                          // number of values for this option
    .option_val = 0,                                                         // value of this option
    .menu = 0,                                                               // pointer to the menu that pressing A opens
    .option_name = "Save Positions",                                         // pointer to a string
    .desc = "Save the current fighter positions\nas the initial positions.", // string describing what this option does
    .option_values = 0,                                                      // pointer to an array of strings
    .onOptionSelect = Record_InitState,
};

static const EventOption Record_Load = {
    .option_kind = OPTKIND_FUNC,                                                             // the type of option this is; menu, string list, integer list, etc
    .value_num = 0,                                                                          // number of values for this option
    .option_val = 0,                                                                         // value of this option
    .menu = 0,                                                                               // pointer to the menu that pressing A opens
    .option_name = "Restore Positions",                                                      // pointer to a string
    .desc = "Load the saved fighter positions and \nstart the sequence from the beginning.", // string describing what this option does
    .option_values = 0,                                                                      // pointer to an array of strings
    .onOptionSelect = Record_RestoreState,
};

static EventOption LabOptions_Record[] = {
    // swapped between Record_Save and Record_Load
    Record_Save,

    {
        .option_kind = OPTKIND_STRING,                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_HMNRecordMode) / 4,            // number of values for this option
        .option_val = 0,                                             // value of this option
        .menu = 0,                                                   // pointer to the menu that pressing A opens
        .option_name = "HMN Mode",                                   // pointer to a string
        .desc = "Toggle between recording and playback of\ninputs.", // string describing what this option does
        .option_values = LabValues_HMNRecordMode,                    // pointer to an array of strings
        .onOptionChange = Record_ChangeHMNMode,
    },
    {
        .option_kind = OPTKIND_STRING,                                                                                       // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_RecordSlot) / 4,                                                                       // number of values for this option
        .option_val = 1,                                                                                                     // value of this option
        .menu = 0,                                                                                                           // pointer to the menu that pressing A opens
        .option_name = "HMN Record Slot",                                                                                    // pointer to a string
        .desc = "Toggle which recording slot to save inputs \nto. Maximum of 6 and can be set to random \nduring playback.", // string describing what this option does
        .option_values = LabValues_RecordSlot,                                                                               // pointer to an array of strings
        .onOptionChange = Record_ChangeHMNSlot,
    },
    {
        .option_kind = OPTKIND_STRING,                               // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_CPURecordMode) / 4,            // number of values for this option
        .option_val = 0,                                             // value of this option
        .menu = 0,                                                   // pointer to the menu that pressing A opens
        .option_name = "CPU Mode",                                   // pointer to a string
        .desc = "Toggle between recording and playback of\ninputs.", // string describing what this option does
        .option_values = LabValues_CPURecordMode,                    // pointer to an array of strings
        .onOptionChange = Record_ChangeCPUMode,
    },
    {
        .option_kind = OPTKIND_STRING,                                                                                       // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabValues_RecordSlot) / 4,                                                                       // number of values for this option
        .option_val = 1,                                                                                                     // value of this option
        .menu = 0,                                                                                                           // pointer to the menu that pressing A opens
        .option_name = "CPU Record Slot",                                                                                    // pointer to a string
        .desc = "Toggle which recording slot to save inputs \nto. Maximum of 6 and can be set to random \nduring playback.", // string describing what this option does
        .option_values = LabValues_RecordSlot,                                                                               // pointer to an array of strings
        .onOptionChange = Record_ChangeCPUSlot,
    },
    {
        .option_kind = OPTKIND_STRING,                     // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_OffOn) / 4,         // number of values for this option
        .option_val = 0,                                   // value of this option
        .menu = 0,                                         // pointer to the menu that pressing A opens
        .option_name = "Loop Input Playback",              // pointer to a string
        .desc = "Loop the recorded inputs when they end.", // string describing what this option does
        .option_values = LabOptions_OffOn,                 // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_STRING,                                              // the type of option this is; menu, string list, integer list, etc
        .value_num = sizeof(LabOptions_OffOn) / 4,                                  // number of values for this option
        .option_val = 0,                                                            // value of this option
        .menu = 0,                                                                  // pointer to the menu that pressing A opens
        .option_name = "Auto Restore",                                              // pointer to a string
        .desc = "Automatically restore saved positions \nafter the playback ends.", // string describing what this option does
        .option_values = LabOptions_OffOn,                                          // pointer to an array of strings
        .onOptionChange = 0,
    },
    {
        .option_kind = OPTKIND_FUNC,                                                             // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                                                          // number of values for this option
        .option_val = 0,                                                                         // value of this option
        .menu = 0,                                                                               // pointer to the menu that pressing A opens
        .option_name = "Resave Positions",                                                       // pointer to a string
        .desc = "Save the current position, without\nremoving recorded inputs.",                 // string describing what this option does
        .option_values = 0,                                                                      // pointer to an array of strings
        .onOptionSelect = Record_ResaveState,
    },
    {
        .option_kind = OPTKIND_FUNC,                                                             // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                                                          // number of values for this option
        .option_val = 0,                                                                         // value of this option
        .menu = 0,                                                                               // pointer to the menu that pressing A opens
        .option_name = "Delete Positions",                                                       // pointer to a string
        .desc = "Delete the current initial position\nand recordings.",                          // string describing what this option does
        .option_values = 0,                                                                      // pointer to an array of strings
        .onOptionSelect = Record_DeleteState,
    },
    {
        .option_kind = OPTKIND_FUNC,                                                             // the type of option this is; menu, string list, integer list, etc
        .value_num = 0,                                                                          // number of values for this option
        .option_val = 0,                                                                         // value of this option
        .menu = 0,                                                                               // pointer to the menu that pressing A opens
        .option_name = "Export",                                                                 // pointer to a string
        .desc = "Export the recording to a memory card\nfor later use or to share with others.", // string describing what this option does
        .option_values = 0,                                                                      // pointer to an array of strings
        .onOptionSelect = Export_Init,
    },
};
static EventMenu LabMenu_Record = {
    .name = "Recording",                                           // the name of this menu
    .option_num = sizeof(LabOptions_Record) / sizeof(EventOption), // number of options this menu contains
    .scroll = 0,                                                   // runtime variable used for how far down in the menu to start
    .state = 0,                                                    // bool used to know if this menu is focused, used at runtime
    .cursor = 0,                                                   // index of the option currently selected, used at runtime
    .options = &LabOptions_Record,                                 // pointer to all of this menu's options
    .prev = 0,                                                     // pointer to previous menu, used at runtime
};
