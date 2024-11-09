    # To be inserted at 80099b74
    .include "../../Globals.s"
    .include "../../../m-ex/Header.s"

    # Get Airdodge Angle
    mr r3, r31
    branchl r12, Joystick_Angle_Retrieve
    stfs f1, TM_AirdodgeAngle(r31)

    # Get Hop Type
    # read state_var1, which is 0 for fullhop and 1 for shorthop
    lwz r3, 0x2340(r31)
    stw r3, TM_ShortOrFullHop(r31)

Exit:
    addi r3, r29, 0
