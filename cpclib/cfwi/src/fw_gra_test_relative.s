.module fw_gra_test_relative

_fw_gra_test_relative::
        ld      hl,#2
        add     hl,sp
        ld      e,(hl)

        inc     hl
        ld      d,(hl)

        inc     hl
        ld      a,(hl)

        inc     hl
        ld      h,(hl)

        ld      l,a
        call    0xBBF3  ; GRA TEST RELATIVE
        ld      l,a
        ret
