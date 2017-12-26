#ifndef  __FW_MC_H__
#define __FW_MC_H__

/** 177: MC BOOT PROGRAM
    #BD13
    Load and run a program.
    Action:
    Shut down as much of the system as possible then load a program into RAM and run
    it. If the load fails then the previous foreground program is restarted.
    Entry conditions:
    HL contains the address of the routine to call to load the program.
    Exit conditions:
    Does not exit!
    Notes:
    The system is partially reset before attempting to load the program. External
    interrupts are disabled, as are all timer, frame flyback and keyboard break events.
    Sound generation is turned off, indirections are set to their default routines and the
    stack is reset to the default system stack. This process ensures that no memory outside
    the firmware variables area is in use when loading the program. Overwriting an active
    event block or indirection routine could otherwise have unfortunate consequences.
    The partial system reset does not change the ROM state or ROM selection. The
    routine run to load the program must be in accessible RAM or an enabled ROM. Note
    that the firmware jumpblock normally enables the lower ROM and disables the upper
    ROM and so the routine must normally be in RAM above #4000 or in the lower ROM.
    The routine run to load the program is free to use any store from #0040 up to the base
    of the firmware variables area (#B100) and may alter indirections and arm external
    device interrupts as required. It should obey the following
    Exit conditions:
    If the program loaded successfully:
    Carry true.
    HL contains the program entry point.
    If the program failed to load:
    Carry false.
    HL corrupt.
    Always:
    A, BC, DE, IX, IY and other flags corrupt.

    After a successful load the firmware is completely initialized (as at EMS) and the
    program is entered at the entry address returned by the load routine. Returning from
    the program will reset the system (perform RST 0).
    After an unsuccessful load an appropriate error message is printed and the previous
    foreground program is restarted. If the previous foreground program was itself a RAM
    program then the default ROM is entered instead as the program may have been
    corrupted during the failed loading.
    Related entries:
    CAS IN DIRECT
    KL CHOKE OFF
    MC START PROGRAM
*/
void fw_mc_boot_program(void *loader) __z88dk_fastcall;

/** WARNING DONE BUT UNTESTED, MIGHT NOT WORK

    178: MC START PROGRAM
    #BD16
    Run a foreground program.
    Action:
    Fully initialize the system and enter a program.
    Entry conditions:
    HL contains the entry point address.
    C contains the required ROM selection.
    Exit conditions:
    Never exits!
    Notes:
    HL and C comprise the 'far address' of the entry point of the foreground program (see
    section 2).
    When entering a foreground program in ROM the ROM selection should be that
    required to select the appropriate ROM. When entering a foreground program in RAM
    the ROM selection should be used to enable or disable ROMs as the RAM program
    requires (ROM select addresses #FC..#FF).
    This routine carries out a full EMS initialization of the firmware before entering the
    program. Returning from the program will reset the system (perform RST 0).
    MC START PROGRAM is intended for running programs in ROM or programs that
    have already been loaded into RAM. To load and run a RAM program use MC BOOT
    PROGRAM.
    Related entries:
    MC BOOT PROGRAM
    RESET ENTRY (RST 0)
*/
void fw_mc_start_program(uint8_t rom_selection, void *entry);

/** 179: MC WAIT FLYBACK
    #BD19
    Wait for frame flyback.
    Action:
    Wait until frame flyback occurs.
    Entry conditions:
    No conditions.
    Exit conditions:
    All registers and flags preserved.
    Notes:
    Frame flyback is a signal generated by the CRT controller to signal the start of the
    vertical retrace period. During this period the screen is not being written and so major
    operations can be performed on the screen without producing unsightly effects. A
    prime example is rolling the screen.
    The frame flyback signal only lasts for a couple of hundred microseconds but the
    vertical retrace period is much longer than this. However, there will be a ticker
    interrupt in the middle of frame flyback which may cause the foreground processing
    to be suspended for a significant length of time. It is important, therefore, to perform
    any critical processing as soon after the frame flyback is detected as is possible.
    This routine returns immediately if frame flyback is occurring when it is called. It
    does not wait for the start of frame flyback (use a frame flyback event to do this).
    Related entries:
    KL ADD FRAME FLY
*/
void fw_mc_wait_flyback(void);

/** 180: MC SET MODE
    #BD1C
    Set the screen mode.
    Action:
    Load the hardware with the required screen mode.
    Entry conditions:
    A contains the required mode.
    Exit conditions:
    AF corrupt.
    All other registers preserved.
    Notes:
    The required mode is checked and no action is taken if it is invalid. If it is valid then
    the new value is sent to the hardware.
    The screen modes are:
    0:
    1:
    2:
    160 x 200 pixels,
    320 x 200 pixels,
    640 x 200 pixels,
    20 x 25 characters.
    40 x 25 characters.
    80 x 25 characters.
    Altering the screen mode without notifying the Screen Pack will produce peculiar
    effects on the screen. In general SCR SET MODE should be called to change screen
    mode. This, in turn, sets the new mode into the hardware.
    Related entries:
    SCR SET MODE
*/
void fw_mc_set_mode(uint8_t mode) __z88dk_fastcall;

/** WARNING DONE BUT UNTESTED, MIGHT NOT WORK

    181: MC SCREEN OFFSET
    #BD1F
    Set the screen offset.
    Action:
    Load the hardware with the offset of the first byte on the screen inside a 2K screen
    block and which 16K block the screen memory is located in.
    Entry conditions:
    A contains the new screen base.
    HL contains the new screen offset.
    Exit conditions:
    AF corrupt.
    All other registers preserved.
    Notes:
    The screen base address is masked with #C0 to make sure it refers to a valid 16K
    memory area. The default screen base is #C0 (the screen is underneath the upper
    ROM).
    The screen offset is masked with #07FE to make it legal. Note that bit 0 is ignored as
    the hardware only uses even offsets.
    If the screen base or offset is changed without notifying the Screen Pack then
    unexpected effects may occur on the screen. In general SCR SET BASE or SCR SET
    OFFSET should be called. These, in their turn, send the values to the hardware.
    Related entries:
    SCR SET BASE
    SCR SET OFFSET
*/
void fw_mc_screen_offset(uint8_t screen_base, uint16_t screen_offset);

enum hardware_color
{
	hardware_color_r0_g0_b0_black		= 20,
	hardware_color_r0_g0_b1_blue		= 4,
	hardware_color_r0_g0_b2_bright_blue	= 21,
	hardware_color_r1_g0_b0_red		= 28,
	hardware_color_r1_g0_b1_magenta	= 24,
	hardware_color_r1_g0_b2_mauve		= 29,
	hardware_color_r2_g0_b0_bright_red	= 12,
	hardware_color_r2_g0_b1_purple		= 5,
	hardware_color_r2_g0_b2_bright_magenta	= 13,
	hardware_color_r0_g1_b0_green		= 22,
	hardware_color_r0_g1_b1_cyan		= 6,
	hardware_color_r0_g1_b2_sky_blue	= 23,
	hardware_color_r1_g1_b0_yellow		= 30,
	hardware_color_r1_g1_b1_white		= 0,
	hardware_color_r1_g1_b2_pastel_blue	= 31,
	hardware_color_r2_g1_b0_orange		= 14,
	hardware_color_r2_g1_b1_pink		= 7,
	hardware_color_r2_g1_b2_pastel_magenta	= 15,
	hardware_color_r0_g2_b0_bright_green	= 18,
	hardware_color_r0_g2_b1_sea_green	= 2,
	hardware_color_r0_g2_b2_bright_cyan	= 19,
	hardware_color_r1_g2_b0_lime		= 26,
	hardware_color_r1_g2_b1_pastel_green	= 25,
	hardware_color_r1_g2_b2_pastel_cyan	= 27,
	hardware_color_r2_g2_b0_bright_yellow	= 10,
	hardware_color_r2_g2_b1_pastel_yellow	= 3,
	hardware_color_r2_g2_b2_bright_white	= 11,
};

typedef union ink_vector1
{
	struct
	{
		enum hardware_color border_color;
		enum hardware_color all_other_color;
	};
	enum hardware_color as_array[2];
} ink_vector1;

typedef union ink_vector2
{
	struct
	{
		enum hardware_color border_color;
		enum hardware_color ink0;
		enum hardware_color ink1;
	};
	enum hardware_color as_array[3];
} ink_vector2;

typedef union ink_vector4
{
	struct
	{
		enum hardware_color border_color;
		enum hardware_color ink0;
		enum hardware_color ink1;
		enum hardware_color ink2;
		enum hardware_color ink3;
	};
	enum hardware_color as_array[5];
} ink_vector4;

typedef union ink_vector16
{
	struct
	{
		enum hardware_color border_color;
		enum hardware_color ink0;
		enum hardware_color ink1;
		enum hardware_color ink2;
		enum hardware_color ink3;
		enum hardware_color ink4;
		enum hardware_color ink5;
		enum hardware_color ink6;
		enum hardware_color ink7;
		enum hardware_color ink8;
		enum hardware_color ink9;
		enum hardware_color ink10;
		enum hardware_color ink11;
		enum hardware_color ink12;
		enum hardware_color ink13;
		enum hardware_color ink14;
		enum hardware_color ink15;
	};
	enum hardware_color as_array[17];
} ink_vector16;

/** WARNING DONE BUT UNTESTED, MIGHT NOT WORK

    #### CFWI-specific information: ####

    MC CLEAR INKS only uses the border color and first ink.  You
    probably already have in RAM your palette in ink_vector format, so
    avoid duplicate it.

    If fw_mc_clear_inks accepts only ink_vector1, compiler will
    complain about your palette not being the good type.

    If fw_mc_clear_inks accepts only bigger ink_vector then memory is
    wasted.

    You can still cast type but this is against code concision and
    clarity.

    Simplest thing: have different C-level fw_mc_clear_inks* function
    declarations that backed by the same ASM-level symbol, only they
    take different C-level types.  Simple code, no waste at any level.



    182: MC CLEAR INKS
    #BD22
    Set all inks to one colour.
    Action:
    Set the colour of the border and set the colour of all the inks. All inks are set to the
    same colour thus giving the impression that the screen has been cleared instantly.
    Entry conditions:
    DE contains the address of an ink vector.
    Exit conditions:
    AF corrupt.
    All other registers preserved.
    Notes:
    The ink vector has the form:
    Byte 0:
    Byte 1:
    Colour of the border.
    Colour for all inks.
    The colours supplied are the numbers used by the hardware rather than the grey scale
    numbers supplied to SCR SET INK (see Appendix V).
    After the screen has been cleared (or whatever) the correct ink colours can be set by
    calling MC SET INKS.
    This routine sets the colours for all 16 inks whether they can be displayed on the
    screen in the current mode or not.
    This ink clearing technique is used by the Screen Pack when clearing the screen or
    changing mode (by SCR CLEAR and SCR SET MODE).
    Related entries:
    MC SET INKS
*/
void fw_mc_clear_inks16(ink_vector16 *ink_vector) __z88dk_fastcall;
void fw_mc_clear_inks4(ink_vector4 *ink_vector) __z88dk_fastcall;
void fw_mc_clear_inks2(ink_vector2 *ink_vector) __z88dk_fastcall;
void fw_mc_clear_inks1(ink_vector1 *ink_vector) __z88dk_fastcall;

/** WARNING DONE BUT UNTESTED, MIGHT NOT WORK

    #### CFWI-specific information: ####

    MC SET INKS only needs as many inks as your current mode.  But it
    will set all 16 inks even if current mode only accepts less, which
    means that if you provide it a ink_vector4 or ink_vector2, the
    bytes following it will be interpreted as color.

    For this reason, the C-level prototype accepts ink_vector16. But
    you can of course use a C-level cast to tell the compiler that
    you're aware.

    Exemple:

    ink_vector4 mypalette =
    {
    hardware_color_r0_g0_b1_blue,
    hardware_color_r2_g2_b2_bright_white
    hardware_color_r0_g0_b0_black
    hardware_color_r2_g1_b0_orange
    }

    fw_mc_set_inks( (ink_vector16 *)mypalette);



    183: MC SET INKS
    #BD25
    Set colours of all the inks.
    Action:
    Set the colours of all the inks and the border.
    Entry conditions:
    DE contains the address of an ink vector.
    Exit conditions:
    AF corrupt.
    All other registers preserved.
    Notes:
    The ink vector passed has the following layout:
    Byte
    Byte
    Byte
    ...
    Byte
    0:
    1:
    2:
    16:
    Colour
    Colour
    Colour
    ...
    Colour
    of the border.
    for ink 0.
    for ink 1.
    for ink 15.
    The colours supplied are the numbers used by the hardware rather than the grey scale
    numbers supplied to SCR SET INK (see Appendix V).
    This routine sets the colours for all inks including those that cannot be visible in the
    current screen mode. However, it is only necessary to supply sensible colours for the
    visible inks.
    The Screen Pack sets the colours for all the inks each time the inks flash and after an
    ink colour has been changed (by calling SCR SET INK or SCR SET BORDER).
    Related entries:
    MC CLEAR INKS
    SCR SET BORDER
    SCR SET INK
*/
void fw_mc_set_inks(ink_vector16 *ink_vector) __z88dk_fastcall;

/** 184: MC RESET PRINTER
    #BD28
    Reset the printer indirection.
    Action:
    Set the printer indirection, MC WAIT PRINTER, to its default routine and, in V1.1
    firmware, set up the default printer translation table.
    Entry conditions:
    No conditions.
    Exit conditions:
    AF, BC, DE and HL corrupt.
    All other registers preserved.
    Notes:
    The default printer translation table is described in Appendix XIV. This is designed to
    drive the DMP-1 printer. It only translates the additional characters in the character
    set (#A0..#AF); it does not translate any of the standard ASCII characters or the
    graphics characters.
    Related entries:
    MC WAIT PRINTER
    MC PRINT CHAR
*/
void fw_mc_reset_printer(void);

#endif /* __FW_MC_H__ */
