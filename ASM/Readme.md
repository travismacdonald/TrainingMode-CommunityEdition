# ASM

Aitch: I don't write much melee asm, but here's some things I've picked up by hacking on unclepunch's asm.
Here are some things you should know before diving into here, and some questions I have.

## Notes:

- If you see a load with offset around 0x23ec-0x2424 in OSD related code,
  it's probably reading or writing to `struct MEX` or `struct TM` in `struct FighterData` in `MexTK/include/fighter.h`.
- `Moonwalk_Exit` is not called that for any reason, it's just a generic exit.
- **Don't be afraid of the dolphin debugger!**. Use the latest version of dolphin.
- Everything in both `ASM/Globals.s` and `ASM/training-mode/Globals.s` are helper macros.
  These are imported into most other ASM files.

### `mflr`

Because ASM files could be placed anywhere, you can't exactly just load a label as an address in a register.
To get around this, there is an extremely common pattern using the `mflr` instruction.
When the `bl <label>` instruction is followed by an `mflr <r>` instruction, the address right after the `bl` is placed in `r`.
So often you will see something like:

```
bl SkipData
.long 12345 # important data
SkipData:
mflr r4
lwz r3, 0x0(r4) # r3 now contains 12345
```

### `EventJumpTable`

If you see the macro `EventJumpTable` in an ASM file somewhere, you're in for a wild ride.
This macro is defined as:

```
.macro EventJumpTable
  bl  Minigames
  bl  GeneralTech
  bl  SpacieTech
.endm
```

This allows event asm code to find global event data.
This macro is not useful by itself. If this macro is used, there will be another macro imported (eg. `EventNameStrings`).
That other macro will have three labels: `Minigames`, `GeneralTech`, and `SpacieTech`.
Under each label, for each event, with be ANOTHER macro for the data for that event.

There will also be definitions for EventID and PageID, and this piece of code:

```
SkipPageList:
  mflr	r4		#Jump Table Start in r4
  mulli	r5,PageID,0x4		#Each Pointer is 0x4 Long
  add	r4,r4,r5		#Get Event's Pointer Address
  lwz	r5,0x0(r4)		#Get bl Instruction
  rlwinm	r5,r5,0,6,29		#Mask Bits 6-29 (the offset)
  mr  r3,EventID
  add	r4,r4,r5		#Gets ASCII Address in r4
```

After this, in the address r4, will be a pointer to the data for the event (event name, event allowed characters, etc.).
That is the entire purpose of all these imports and definitions and code.
Yes, this code reads a branch instruction and extracts the offset XD.
Absolutely bonkers.

### `R13`

`R13` is the small data area pointer.
This points to a small amount of static data contained inside the executable (Start.dol) itself, rather than the iso.
There are some offsets under the `r13 Offsets` header in `ASM/training-mode/Globals.s`, as well as in `MexTK/include/offsets.h`.
Look there for what is loaded if you see a load with negative offset into `R13`.

## Questions I Have

- There seems to be two distict styles of asm: one that import mex.h and one that does not.
  I'm pretty sure the mex import asm files are newer.
  Should I convert the old style asm files to new ones?
  If so, how?
