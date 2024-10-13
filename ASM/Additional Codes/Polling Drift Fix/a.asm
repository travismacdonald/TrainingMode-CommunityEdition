# insert at 801a5050

CHECK:
    lis r3, 0x804c
    lbz r3, 0x1f7b(r3)
    cmpwi r3, 0
    beq CHECK
    li r3, 0
