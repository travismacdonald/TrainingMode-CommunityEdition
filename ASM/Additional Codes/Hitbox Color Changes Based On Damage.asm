    # To be inserted at 80009fa4

    # Get Values
    lfs f5, 0xC(r3)         # Get Hitbox Damage
    bl Floats
    mflr r5
    lfs f6, 0x0(r5)         # Min GB Value
    lfs f7, 0x4(r5)         # Damage Value for Lightest Color
    lfs f8, 0x8(r5)         # Damage Value for Darkest Color

    # Check If Does Over Max Damage Accounted For
ClampToMax:
    fcmpo cr0, f5, f8
    blt ClampToMin
    li r6, 0x0
    b StoreGB

ClampToMin:
    fcmpo cr0, f5, f7
    bgt Formula
    lfs f8, 0x0(r5)         # Value for Lightest Color
    b ConvertToDecimal

Formula:
    fsubs f8, f8, f7
    fsubs f5, f5, f7
    fsubs f7, f7, f7
    fdivs f8, f6, f8        # minimum GB value / max damage
    fmuls f8, f5, f8        # damage * (minimum GB value / damage range)
    fsubs f8, f6, f8        # minimum GB value - (damage * (minimum GB value / damage range))

ConvertToDecimal:
    fctiwz f5, f8
    stfd f5, -0xC(sp)
    lwz r6, -0x8(sp)        # Get Hitbox Damage as Int

StoreGB:
    subi r5, r13, 0x8000    # get hitbox color ptr

    cmpwi r25, 0
    beq ID_0
    cmpwi r25, 1
    beq ID_1
    cmpwi r25, 2
    beq ID_2
    b ID_ETC

ID_0:
    stb r6, 0x1(r5)         # G
    stb r6, 0x2(r5)         # B
    li r6, 255
    stb r6, 0x0(r5)         # R
    b Alpha
ID_1:
    stb r6, 0x0(r5)         # R
    stb r6, 0x2(r5)         # B
    li r6, 255
    stb r6, 0x1(r5)         # G
    b Alpha
ID_2:
    subi r6, r6, 10
    stb r6, 0x0(r5)         # R
    stb r6, 0x1(r5)         # G
    li r6, 255
    stb r6, 0x2(r5)         # B
    b Alpha
ID_ETC:
    stb r6, 0x0(r5)         # R
    stb r6, 0x1(r5)         # G
    stb r6, 0x2(r5)         # B

Alpha:
    li r6, 200
    stb r6, 0x3(r5)         # A
    b Exit

Floats:
    blrl
    .long 0x43480000        # Min GB Value 200
    .long 0x40A00000        # Damage Value for Lightest Color 5
    .long 0x41900000        # Damage Value for Darkest Color 18

Exit:
