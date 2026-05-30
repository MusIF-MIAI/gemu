; Disassembly of ../../DUMP1/funktionalcpu.cap
; origin 0x0000, 7252 bytes  (re-assemble with gasm)

; ---- named cells (variables / data) ----
step_code          EQU 0x0010   ; progress code: which sub-test is running (shown on halt)
scratch_fc         EQU 0x00FC   ; self-test scratch (shift results)
reg7               EQU 0x00FE   ; change/segment register 7 = mem[0xFE..0xFF] BE (240+2*7).
step_0x12_pk       EQU 0x0244   ; PK (Pack) 1-byte; process UPWARD, 2 digits/byte, no sign.
step_0x14_ad       EQU 0x027E   ; AD (Add Decimal); preserve 1st-operand zone, CC F104=carry
step_0x19_sd       EQU 0x0432   ; SD (Sub Decimal); preserve zone, CC sign-based (<0/=0/>0).
opt_subcode        EQU 0x0E70   ; secondary option/sub-test code
fault_step_disp    EQU 0x0E71   ; failing step code copied here for the console display
fault_code_disp    EQU 0x0E76   ; fault code shown on the console
step_code_mem      EQU 0x2010   ; progress code for the memory-test phase

        ORG    0x0000
cold_start:          ; reset entry: NOP NOP -> main
        NOP2  
        NOP2  
        JC     0xF0, main   ; JMP/JANY

        ORG    0x0100
main:          ; -> select_test
        JC     0xF0, select_test   ; JMP/JANY
cpu_selftest:          ; seed reg7, run the instruction self-test
        MVC    2, reg7, selftest_vectors
        JC     0xF0, selftest_begin   ; JMP/JANY
oper_checkpoint:          ; operator-call / single-step checkpoint (also the fault
        MVC    2, 0x0006, reg7
        LON   
        MVC    1, fault_step_disp, step_code
        JC     0xF0, show_code_then_halt   ; JMP/JANY
selftest_begin:          ; instruction self-test, step 0x02 onward (walks the ISA)
        MVI    0x02, step_code
        MVI    0x2C, 0x00FF
        JRT    0x00, oper_checkpoint
selftest_continue:          ; resume point after operator "continue"
        MVI    0x03, step_code
        JRT    0xF0, chk_jrt_retaddr
        MVC    2, reg7, 0x04B4
        JC     0xF0, oper_checkpoint   ; JMP/JANY
chk_jrt_retaddr:          ; subroutine: verify JRT deposited the right return address
        CMC    2, reg7, 0x04B6
        JRT    0xD0, oper_checkpoint
        MVI    0x04, step_code
        NOP2  
        MVI    0x05, step_code
        CMI    0x01, 0x04B8
        JRT    0xD0, oper_checkpoint
        MVI    0x06, step_code
        CMI    0x00, 0x04B8
        JRT    0x20, oper_checkpoint
        MVI    0x07, step_code
        MVI    0x01, 0x04B9
        CMI    0x01, 0x04B9
        JRT    0xD0, oper_checkpoint
        MVI    0x08, step_code
        CMC    1, 0x04BF, 0x04BB
        JRT    0xD0, oper_checkpoint
        MVI    0x09, step_code
        CMC    3, 0x04BA, 0x04BE
        JRT    0x30, oper_checkpoint
        MVI    0x0A, step_code
        CMC    3, 0x04BB, 0x04BF
        JRT    0xE0, oper_checkpoint
        MVI    0x0B, step_code
        MVC    1, 0x04C2, 0x04BE
        CMI    0x04, 0x04C2
        JRT    0xD0, oper_checkpoint
        MVI    0x0C, step_code
        MVC    2, 0x04C2, 0x04BE
        CMC    2, 0x04C2, 0x04BE
        JRT    0xD0, oper_checkpoint
        MVI    0x0D, step_code
        MVI    0x40, 0x04C4
        OC     1, 0x04C4, 0x04C5
        CMI    0x41, 0x04C4
        JRT    0xD0, oper_checkpoint
        MVI    0x0E, step_code
        MVI    0x50, 0x04C6
        XC     1, 0x04C6, 0x04C7
        CMI    0x01, 0x04C6
        JRT    0xD0, oper_checkpoint
        MVI    0x0F, step_code
        MVI    0x55, 0x04C9
        NC     1, 0x04C9, 0x04CA
        CMI    0x51, 0x04C9
        JRT    0xD0, oper_checkpoint
        MVI    0x10, step_code
        MVI    0x01, 0x04CC
        TL     1, 0x04CC, xlate_table
        CMI    0x51, 0x04CC
        JRT    0xD0, oper_checkpoint
        MVI    0x11, step_code
        MVC    3, 0x04CD, 0x04D4
        TL     3, 0x04CD, xlate_table
        CMC    3, 0x04CD, 0x04D1
        JRT    0xD0, oper_checkpoint
        MVI    0x12, step_code
        MVC    2, 0x04DC, 0x04D8
        PK     1, 1, 0x04DC, 0x04E1
        CMI    0x56, 0x04DC
        JRT    0xD0, oper_checkpoint
        CMI    0x00, 0x04DD
        JRT    0xD0, oper_checkpoint
        MVI    0x13, step_code
        MVC    4, 0x04DC, 0x04D8
        PK     1, 4, 0x04DC, 0x04E3
        CMC    4, 0x04DC, 0x04EB
        JRT    0xD0, oper_checkpoint
        MVI    0x14, step_code
        MVC    2, 0x04F1, 0x04EF
        AD     1, 1, 0x04F2, 0x04F3
        JRT    0xC0, oper_checkpoint
        CMI    0x41, 0x04F2
        JRT    0xD0, oper_checkpoint
        CMI    0x40, 0x04F1
        JRT    0xD0, oper_checkpoint
        MVI    0x15, step_code
        MVC    2, 0x04F1, 0x04EF
        AD     1, 2, 0x04F2, 0x04F5
        JRT    0x30, oper_checkpoint
        CMI    0x44, 0x04F2
        JRT    0xD0, oper_checkpoint
        CMI    0x40, 0x04F1
        JRT    0xD0, oper_checkpoint
        MVI    0x16, step_code
        MVC    3, 0x04F9, 0x04F6
        AD     2, 1, 0x04FB, 0x04FC
        JRT    0x30, oper_checkpoint
        CMC    2, 0x04FA, 0x04FD
        JRT    0xD0, oper_checkpoint
        CMI    0x40, 0x04F9
        JRT    0xD0, oper_checkpoint
        MVI    0x17, step_code
        MVC    2, 0x0512, 0x0510
        AB     1, 2, 0x0513, 0x0515
        JRT    0x30, oper_checkpoint
        CMC    2, 0x0512, 0x0516
        JRT    0xD0, oper_checkpoint
        MVI    0x18, step_code
        MVC    2, 0x0512, 0x0518
        AB     1, 2, 0x0513, 0x051B
        JRT    0xC0, oper_checkpoint
        CMC    2, 0x0512, 0x051C
        JRT    0xD0, oper_checkpoint
        MVC    1, cold_start, cold_start
        MVI    0x19, step_code
        MVC    2, 0x0520, 0x051E
        SD     1, 2, 0x0521, 0x0523
        JRT    0xC0, oper_checkpoint
        CMC    2, 0x0520, 0x0524
        JRT    0xD0, oper_checkpoint
        MVI    0x1A, step_code
        MVC    2, 0x0528, 0x0526
        SB     1, 2, 0x0529, 0x052B
        JRT    0xC0, oper_checkpoint
        CMC    2, 0x0528, 0x052C
        JRT    0xD0, oper_checkpoint
        MVI    0x1B, step_code
        MVC    2, 0x0530, 0x052E
        MVQ    2, 0x0531, 0x0533
        JRT    0xA0, oper_checkpoint
        CMC    2, 0x0530, 0x0534
        JRT    0xD0, oper_checkpoint
        MVI    0x1C, step_code
        CMQ    2, 0x0534, 0x0536
        JRT    0xD0, oper_checkpoint
        MVI    0x1D, step_code
        MVC    4, 0x053C, 0x0538
        UPK    1, 1, 0x053C, 0x0540
        CMC    2, 0x053C, 0x0541
        JRT    0xD0, oper_checkpoint
        CMC    2, 0x053E, 0x0538
        JRT    0xD0, oper_checkpoint
        MVI    0x1E, step_code
        MVC    6, 0x054A, 0x0544
        UPK    1, 2, 0x054A, 0x0550
        CMC    4, 0x054A, 0x0552
        JRT    0xD0, oper_checkpoint
        CMC    2, 0x0548, 0x054E
        JRT    0xD0, oper_checkpoint
        MVI    0x1F, step_code
step_0x1f_sr:          ; self-test step 0x1F: SR (Search Right) 0x0556,0x0558 ->
        SR     2, 0x0556, 0x0558
        MVC    2, scratch_fc, reg7
        JRT    0xA0, oper_checkpoint
        CMC    2, 0x0559, scratch_fc
        JRT    0xD0, oper_checkpoint
        MVI    0x20, step_code
        SR     2, 0x0556, 0x055B
        MVC    2, scratch_fc, reg7
        JRT    0xA0, oper_checkpoint
        CMC    2, 0x055C, scratch_fc
        JRT    0xD0, oper_checkpoint
        MVI    0x21, step_code
        SR     2, 0x0556, 0x055E
        MVC    2, scratch_fc, reg7
        JRT    0x50, oper_checkpoint
        CMC    2, 0x0559, scratch_fc
        JRT    0xD0, oper_checkpoint
        MVI    0x22, step_code
        SL     2, 0x0557, 0x0558
        MVC    2, scratch_fc, reg7
        JRT    0xA0, oper_checkpoint
        CMC    2, 0x055F, scratch_fc
        JRT    0xD0, oper_checkpoint
        MVI    0x23, step_code
        MVC    4, 0x0568, 0x0564
        EDT    3, 0x0568, 0x056C
        JRT    0xE0, oper_checkpoint
        CMC    3, 0x0568, 0x056D
        JRT    0xD0, oper_checkpoint
        CMI    0x00, 0x056B
        JRT    0xD0, oper_checkpoint
        CMI    0x41, 0x056C
        JRT    0xD0, oper_checkpoint
        MVI    0x24, step_code
        MVC    9, 0x0579, 0x0570
        EDT    9, 0x0579, 0x0582
        JRT    0xE0, oper_checkpoint
        CMC    9, 0x0579, 0x0587
        JRT    0xD0, oper_checkpoint
selftest_switch_restart:          ; end of CPU self-test (after step 0x24): JS2 main
        JS2    main
        NOP2  
        JC     0xF0, selftest_decimal   ; JMP/JANY
        JC     0xF0, main   ; JMP/JANY
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
selftest_vectors:          ; CPU self-test data vectors (expected values, 0x04B0..0x06xx)
        DB     0x01        ; .
        DB     0x20        ; .
        DB     0x01        ; .
        DB     0x2C        ; .
        DB     0x01        ; .
        DB     0x48        ; 8
        DB     0x01        ; .
        DB     0x34        ; .
        DB     0x01        ; .
        DB     0x00        ; .
        DB     0x03        ; .
        DB     0x02        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x04        ; .
        DB     0x02        ; .
        DB     0x01        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JRT    0x00, 0x5101
        DB     0x00        ; .
        DB     0x51        ; A
        DB     0x00        ; .
        DB     0x01        ; .
        DB     0x05        ; .
        DB     0x08        ; .
        DB     0x0F        ; .
        DB     0x51        ; A
        DB     0x55        ; E
        DB     0x58        ; H
        DB     0x5F        ; \
        DB     0x05        ; .
        DB     0x08        ; .
        DB     0x0F        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x56        ; F
        DB     0x05        ; .
        DB     0x56        ; F
        JCC    0x40, 0x4243   ; JL/JLT
        DB     0x44        ; 4
        DB     0x45        ; 5
        DB     0x46        ; 6
        JU     0x2345
        DB     0x67        ; .
        JCC    0x40, cold_start   ; JL/JLT
        DB     0x49        ; 9
        JRT    0x42, 0x4041
        DB     0x48        ; 8
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JC     0x40, 0x4100   ; JL/JLT
xlate_table:          ; TL (translate) table used by the self-test
        DB     0x50        ;  
        DB     0x51        ; A
        DB     0x52        ; B
        DB     0x53        ; C
        DB     0x54        ; D
        DB     0x55        ; E
        DB     0x56        ; F
        DB     0x57        ; G
        DB     0x58        ; H
        DB     0x59        ; I
        DB     0x5A        ; &
        DB     0x5B        ; .
        DB     0x5C        ; ]
        DB     0x5D        ; (
        DB     0x5E        ; <
        DB     0x5F        ; \
        DB     0x00        ; .
        DB     0x0F        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x05        ; .
        NOP2  
        DB     0x16        ; .
        DB     0x00        ; .
        AD     1, 7, 0x0600, cold_start
        DB     0x44        ; 4
        DB     0x00        ; .
        DB     0x00        ; .
        JCC    0x40, 0x0042   ; JL/JLT
        DB     0x00        ; .
        SB     1, 1, 0x0007, 0xA00(7)
        DB     0x05        ; .
        JCC    0x40, cold_start   ; JL/JLT
        DB     0x55        ; E
        DB     0x57        ; G
        JCC    0x40, 0x4047   ; JL/JLT
        JCC    0x40, 0x4040   ; JL/JLT
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JRT    0x44, 0x4100
        JCC    0x40, 0x4040   ; JL/JLT
        JCC    0x40, cold_start   ; JL/JLT
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JRT    0x42, 0x4441
        DB     0x44        ; 4
        DB     0x42        ; 2
        DB     0x05        ; .
        DB     0x44        ; 4
        DB     0x44        ; 4
        DB     0x05        ; .
        DB     0x58        ; H
        DB     0x05        ; .
        DB     0x05        ; .
        DB     0x57        ; G
        DB     0x00        ; .
        DB     0x05        ; .
        DB     0x56        ; F
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x50        ;  
        DB     0x23        ; .
        DB     0x20        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JRT    0x50, 0x5041
        DB     0x50        ;  
        DB     0x23        ; .
        DB     0x21        ; .
        JCC    0x20, 0x2022   ; JE/JEQ/JZ
        DB     0x20        ; .
        DB     0x21        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x01        ; .
        DB     0x00        ; .
        JCC    0x00, 0x5050
        DB     0x00        ; .
        JCC    0x00, 0x0050
        DB     0x50        ;  
        DB     0x04        ; .
        DB     0x00        ; .
        DB     0x10        ; .
        DB     0x00        ; .
        DB     0x12        ; .
        DB     0x00        ; .
        DB     0x21        ; .
        DB     0x00        ; .
        DB     0x10        ; .
        DB     0x0C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x1C        ; .
        MP     6, 5, 0x0027, cold_start
        DB     0x00        ; .
        DB     0x12        ; .
        DB     0x01        ; .
        DB     0x11        ; .
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x1C        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x1D        ; .
        DB     0x12        ; .
        DB     0x00        ; .
        DB     0x1C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x1D        ; .
        SB     1, 1, 0x0002, 0x1C01
        DB     0x3D        ; .
        DB     0x00        ; .
        DB     0x27        ; .
        DB     0x3D        ; .
        DB     0x02        ; .
        DB     0x1C        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x00        ; .
        DB     0x12        ; .
        DB     0x0C        ; .
        DB     0x00        ; .
        DB     0x12        ; .
        DB     0x0C        ; .
        DB     0x2D        ; .
        DB     0x00        ; .
        DB     0x21        ; .
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x21        ; .
        DB     0x2C        ; .
        SB     1, 1, 0x0014, 0x5C01
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x14        ; .
        DB     0x5C        ; ]
        DB     0x00        ; .
        DB     0x1C        ; .
        DB     0x00        ; .
        DB     0x04        ; .
        DB     0x5C        ; ]
        DB     0x5C        ; ]
        DB     0x00        ; .
        DB     0x04        ; .
        DB     0x5C        ; ]
        DB     0x00        ; .
        PERI   0x0C, 0x0021
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x21        ; .
        DB     0x2C        ; .
        DB     0x20        ; .
        DB     0x0D        ; .
        DB     0x1D        ; .
        DB     0x01        ; .
        DB     0x2C        ; .
        SB     1, 1, 0x000C, 0x2C00
        DB     0x0C        ; .
        DB     0x0C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x0C        ; .
        DB     0x55        ; E
        SB     1, 1, 0xF55(7), 0xF55(7)
        DB     0x00        ; .
        SB     1, 1, 0x5500, 0xF00(7)
        DB     0x55        ; E
        DB     0x00        ; .
        SB     1, 1, 0x55FF, 0x500(1)
        CMI    0x04, 0x300(4)
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x27        ; .
        DB     0x53        ; C
        DB     0x00        ; .
        DB     0x00        ; .
        HLT   
        DB     0x10        ; .
        DB     0x26        ; .
        DB     0xB0        ; +
        DB     0x13        ; .
        DB     0x72        ; .
        DB     0x60        ; .
        DB     0x22        ; .
        DB     0x73        ; .
        DB     0xB0        ; +
        DB     0x13        ; .
        DB     0xB0        ; +
        DB     0x14        ; .
        SB     16, 16, cold_start, 0x5380
        DB     0x00        ; .
        DB     0x00        ; .
        JCC    0x40, 0x4042   ; JL/JLT
        DB     0x10        ; .
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x10        ; .
        DB     0x2D        ; .
        DB     0x00        ; .
        JCC    0x40, cold_start   ; JL/JLT
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x0C        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        JRT    0x42, 0x0012
        DB     0x0C        ; .
        SB     1, 1, 0x00FF, cold_start
        HLT   
        JCC    0x40, 0x000(2)   ; JL/JLT
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0xA0        ; .
        DB     0x02        ; .
        DB     0x0D        ; .
        SB     5, 1, 0x4040, 0x00FF
        SB     16, 16, 0xF00(7), 0x4501
        DB     0x25        ; .
        DB     0x00        ; .
        DB     0x65        ; .
        DB     0x45        ; 5
        DB     0x00        ; .
        RDC    0x25, 0x1364
        DB     0x03        ; .
        DB     0x85        ; .
        DB     0x00        ; .
        RDC    0x25, 0xF10(7)
        DB     0x7D        ; .
        DB     0x8C        ; .
        DB     0x09        ; .
        LPSR   0x10, 0x7D15
        DB     0x0C        ; .
        DB     0x15        ; .
        DB     0x0D        ; .
        DB     0x15        ; .
        DB     0x0C        ; .
        DB     0x46        ; 6
        DB     0x2C        ; .
        DB     0x15        ; .
        DB     0x3C        ; .
        DB     0x46        ; 6
        DB     0x2C        ; .
        DB     0x10        ; .
        DB     0x3D        ; .
        DB     0x15        ; .
        DB     0x1C        ; .
        DB     0x04        ; .
        DB     0x8C        ; .
        DB     0x10        ; .
        DB     0x3D        ; .
        SB     2, 1, 0x3C15, 0x1C04
        DB     0x8D        ; .
        DB     0x10        ; .
        DB     0x3C        ; .
        JRT    0x8C, 0x003D
        DB     0x42        ; 2
        DB     0x1C        ; .
        JRT    0x8C, 0x0280
        HLT   
        DB     0x00        ; .
        HLT   
        HLT   
        DB     0x00        ; .
        DB     0x23        ; .
        DB     0x5D        ; (
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        SB     16, 16, 0xFFF(7), 0x051C
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x14        ; .
        DB     0x0C        ; .
        DB     0x12        ; .
        DB     0x70        ; .
        DB     0x3D        ; .
        DB     0x34        ; .
        DB     0x0C        ; .
        CMI    0x1D, 0x000C
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x14        ; .
        DB     0x7C        ; .
        DB     0x25        ; .
        DB     0x8C        ; .
        DB     0x01        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        DB     0x54        ; D
        DB     0x01        ; .
        DB     0x00        ; .
        DB     0x0D        ; .
        LA     7, 0x06F0
        DB     0xF3        ; .
        DB     0x1A        ; .
        DB     0x06        ; .
        DB     0xF6        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0xF3        ; .
        DB     0x1A        ; .
        NOP2  
        NOP2  
        DB     0xAB        ; $
        DB     0x17        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0xAB        ; $
        DB     0x17        ; .
        DB     0x00        ; .
        DB     0x12        ; .
        DB     0x00        ; .
        DB     0x1D        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
selftest_decimal:          ; decimal MP/DP test block (steps 0x25..0x31).
        MVI    0x25, step_code
step_0x25_mp_ovf:          ; MP 10,9 -> overflow expected (cc=0). MP overflows when the
        MP     10, 9, 0x05A2, 0x0598
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x26, step_code
        MP     1, 2, 0x05A2, 0x05A4
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x27, step_code
step_0x27_mp_ovf2:          ; MP 6,5 -> overflow; on overflow MP CLEARS the V2(b) field
        MP     6, 5, 0x05AF, 0x05A9
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    5, 0x00E8, 0x05A5
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVC    5, 0x00E8, 0x0599
        MVI    0x28, step_code
        MVC    2, 0x05B3, 0x05B5
        MP     2, 1, 0x05B4, 0x05B2
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x05B3, 0x05B5
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x29, step_code
        MVC    4, 0x05B7, 0x05BE
        MP     4, 3, 0x05BA, 0x05BD
        CMC    4, 0x070D, 0x05B7
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x2A, step_code
        MVC    2, 0x05C5, 0x05CC
        MP     4, 2, 0x05C6, 0x05C8
        CMC    3, 0x05C9, 0x05C4
        JRT    0xD0, oper_checkpoint
step_0x2B_dp:          ; DP (Divide) block (0x2B..); quotient->A1, JRT 0x70 wants cc=0.
        JC     0x00, cold_start
        MVI    0x2B, step_code
        DP     3, 2, 0x05D2, 0x05CF
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    3, 0x05D0, 0x05D3
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x2C, step_code
        DP     2, 1, 0x05D9, 0x05D6
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x05D8, 0x05DB
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x2D, step_code
        MVC    4, 0x05DE, 0x05E4
        DP     4, 2, 0x05E1, 0x05E3
        JRT    0x80, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x05E2, 0x05DE
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x05E8, 0x05E0
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x2E, step_code
        MVC    3, 0x05EA, 0x05EE
        DP     3, 1, 0x05EC, 0x05ED
        CMC    3, 0x05EA, 0x05F1
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x2F, step_code
        MVC    3, 0x05F7, 0x05F4
        DP     3, 2, 0x05F9, 0x05FB
        CMC    2, 0x05F7, 0x05FC
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x30, step_code
        MVC    3, 0x0600, 0x0607
        DP     3, 1, 0x0602, 0x0603
        CMC    3, 0x0600, 0x0604
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x31, step_code
        MVC    2, 0x060A, 0x060E
        NI     0xAA, 0x060A
        CMC    2, 0x060A, 0x060C
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x32, step_code
step_0x32_ci:          ; logical-immediate block start (steps 0x32..0x36).
        MVC    2, 0x0610, 0x0614
        CI     0xAA, 0x0610
        CMC    2, 0x0610, 0x0612
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x33, step_code
step_0x33_xi:          ; XI (XOR Immediate) 0xAA; result checked.
        MVC    2, 0x0616, 0x061A
        XI     0xAA, 0x0616
        CMC    2, 0x0616, 0x0618
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        MVI    0x34, step_code
step_0x34_tm:          ; TM (Test under Mask) 0xAA; CC only (JRT 0xC0).
        TM     0xAA, 0x061C
        JRT    0xC0, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        MVI    0x35, step_code
        MVI    0x55, 0x061C
        TM     0xAA, 0x061C
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        CMI    0x55, 0x061C
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x36, step_code
        TM     0xAA, 0x061D
        JC     0x10, L_09A6   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_09A6:
        MVI    0x37, step_code
step_0x37_lr:          ; change-register op block start (steps 0x37..): LR/STR/AMR/
        CMI    0x95, step_0x37_lr
        MVC    3, 0x00FB, 0x061E
        LR     6, 0x0622
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        CMC    3, 0x0620, 0x00FB
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x38, step_code
        CMI    0x95, 0x09D2
        MVC    2, 0x00F4, 0x0627
        MVC    2, 0x0623, 0x0625
        DB     0xB4        ; U
        DB     0xA0        ; .
        DB     0x06        ; .
        DB     0x24        ; .
        CMC    2, 0x0623, 0x0627
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x39, step_code
        AD     1, 1, 0x0629, 0x062A
        JRT    0x70, oper_checkpoint
        MVC    2, 0x00F8, 0x062B
        CMR    4, 0x062C
        JRT    0x80, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        MVI    0x3A, step_code
        MVC    2, 0x00F2, 0x062D
        CMR    1, 0x062E
        CMC    2, 0x00F2, 0x062D
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x3B, step_code
        CMI    0x94, 0x0A38
        JRT    0xE0, oper_checkpoint
        MVC    2, 0x00F0, 0x062D
        CMR    0, 0x062E
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x3C, step_code
        MVC    2, 0x00F0, 0x062F
        AMR    0, 0x0632
        JC     0x10, L_0A68   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0A68:
        CMC    2, 0x00F0, 0x0633
        JRT    0xD0, oper_checkpoint
        NOP2  
        JC     0x00, cold_start
        MVI    0x3D, step_code
        MVC    2, 0x00F0, 0x0635
        SMR    0, 0x0638
        JC     0x40, L_0A8E   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0A8E:
        CMC    2, 0x00F0, 0x0639
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x3E, step_code
        MVC    2, 0x00F0, 0x063B
        LA     0, 0x10A2
        CMC    2, 0x00F0, 0x0AA8
        JC     0x20, L_0AB8   ; JE/JEQ/JZ
        JRT    0xF0, oper_checkpoint
L_0AB8:
        MVI    0x3F, step_code
        MVC    4, 0x0647, 0x063D
        UPKS   4, 2, 0x064A, 0x0646
        JC     0x10, L_0AD0   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0AD0:
        CMC    4, 0x0641, 0x0647
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        MVI    0x40, step_code
        MVC    3, 0x064B, 0x0653
        UPKS   2, 3, 0x064D, 0x064F
        JC     0x40, L_0AFA   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0AFA:
        CMC    3, 0x064B, 0x0650
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x41, step_code
        MVC    3, 0x0658, 0x065B
        UPKS   3, 2, 0x065A, 0x0657
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x42, step_code
        MVC    3, 0x0663, 0x0666
        PKS    3, 3, 0x0665, 0x0660
        JC     0x10, L_0B38   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0B38:
        CMC    3, 0x0660, 0x0663
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x43, step_code
        MVC    2, 0x066E, 0x0670
        PKS    2, 5, 0x066F, 0x066D
        JC     0x40, L_0B5E   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0B5E:
        CMC    3, 0x066D, 0x0672
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x44, step_code
        MVC    2, 0x067A, 0x067C
        PKS    2, 4, 0x067B, 0x0679
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x45, step_code
        CMI    0x96, 0x0B88
        MVC    1, 0x067F, 0x0684
        AP     1, 2, 0x067F, 0x0681
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x067E, 0x0682
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x46, step_code
        MVC    3, 0x0685, 0x068C
        AP     2, 2, 0x0687, 0x0689
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    2, 0x068A, 0x0686
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x47, step_code
        MVC    2, 0x0690, 0x0695
        AP     2, 1, 0x0691, 0x0692
        JC     0x40, L_0BEC   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0BEC:
        CMC    2, 0x0693, 0x0690
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        MVI    0x48, step_code
        CMI    0x96, 0x0C02
        MVC    2, 0x0697, 0x069B
        AP     2, 2, 0x0698, 0x069A
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x49, step_code
        MVC    2, 0x069D, 0x06A1
        AP     2, 2, 0x069E, 0x06A0
        JC     0x10, L_0C32   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0C32:
        MVI    0x4A, step_code
        MVC    2, 0x06A3, 0x06A9
        AP     2, 2, 0x06A4, 0x06A6
        CMC    2, 0x06A3, 0x06A7
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x4B, step_code
        MVC    2, 0x06AC, 0x06B2
        SP     2, 2, 0x06AD, 0x06AF
        CMC    2, 0x06AC, 0x06B0
        JRT    0xD0, oper_checkpoint
        NOP2  
        MVI    0x4C, step_code
        MVC    2, 0x06B4, 0x06BA
        SP     2, 2, 0x06B5, 0x06B7
        CMC    2, 0x06B4, 0x06B8
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x4D, step_code
        CMI    0x94, 0x0C8E
        MVC    3, 0x06BE, 0x06C3
        MVP    2, 3, 0x06C0, 0x06BE
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        CMC    3, 0x06BE, 0x06C1
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x4E, step_code
        MVC    2, 0x06C8, 0x06CA
        MVP    2, 2, 0x06C9, 0x06C7
        JC     0x40, L_0CCC   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0CCC:
        MVI    0x4F, step_code
        MVC    2, 0x06CE, 0x06D0
        MVP    2, 2, 0x06CF, 0x06CD
        JC     0x20, L_0CE4   ; JE/JEQ/JZ
        JRT    0xF0, oper_checkpoint
L_0CE4:
        MVI    0x50, step_code
        MVC    2, 0x06D4, 0x06D6
        MVP    2, 2, 0x06D5, 0x06D3
        JC     0x10, L_0CFC   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0CFC:
        MVI    0x51, step_code
        CMI    0x94, 0x0D00
        CMP    2, 3, 0x06D9, 0x06DC
        JRT    0x70, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x52, step_code
        CMP    2, 2, 0x06DE, 0x06E0
        JC     0x10, L_0D24   ; JH/JGT
        JRT    0xF0, oper_checkpoint
L_0D24:
        MVI    0x53, step_code
        CMP    2, 2, 0x06E2, 0x06E4
        JC     0x20, L_0D36   ; JE/JEQ/JZ
        JRT    0xF0, oper_checkpoint
L_0D36:
        MVI    0x54, step_code
        CMP    2, 2, 0x06E6, 0x06E8
        JC     0x40, L_0D48   ; JL/JLT
        JRT    0xF0, oper_checkpoint
L_0D48:
        MVI    0x55, step_code
        CMI    0x94, 0x0D4C
        LPSR   0x00, 0x06E9
        JC     0x80, L_0D5C   ; JOV
        JRT    0xF0, oper_checkpoint
L_0D5C:
        MVI    0x56, step_code
        LPSR   0x00, 0x06ED
        JRT    0xF0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x57, step_code
        LR     0, 0x06F3
        CMI    0xF3, 0x001(0)
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x58, step_code
        LR     1, 0x06F7
        CMC    2, 0x06F4, 0x00A(1)
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        MVI    0x59, step_code
        LR     2, 0x0703
        LR     0, 0x0705
        CMC    2, 0x001(2), 0x003(0)
        JRT    0xD0, oper_checkpoint
        JC     0x00, cold_start
        JC     0x00, cold_start
        JC     0xF0, L_0E84   ; JMP/JANY
        HLT   
        JC     0xF0, main   ; JMP/JANY
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
opt_image:          ; test-selection option image (operator sets from console)
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        DB     0x00        ; .
        DB     0x00        ; .
        HLT   
        NOP2  
        NOP2  
        NOP2  
show_code_then_halt:          ; display fault code; SWITCH2=continue else final_halt
        MVC    2, fault_code_disp, 0x0006
        JS2    op_resume
        JC     0xF0, final_halt   ; JMP/JANY
op_resume:          ; LOFF (lamp off) -> selftest_continue
        LOFF  
        JC     0xF0, selftest_continue   ; JMP/JANY
        DB     0x5C        ; ]
        DB     0x5D        ; (
        DB     0x00        ; .
        DB     0x2C        ; .
        DB     0x00        ; .
        DB     0x4C        ; @
        DB     0x00        ; .
        DB     0x2C        ; .
        DB     0x0C        ; .
        DB     0x0D        ; .
        DB     0x00        ; .
        DB     0x00        ; .
L_0E84:
        MVI    0x5A, step_code
        CMP    1, 1, 0x0E78, 0x0E79
        JRT    0x20, oper_checkpoint
        MVI    0x5B, step_code
        MVC    2, 0x0E7A, 0x0E7E
        AP     2, 2, 0x0E7B, 0x0E7D
        JRT    0x20, oper_checkpoint
        MVI    0x5C, step_code
        MVI    0x0C, 0x0E80
        AP     1, 1, 0x0E80, 0x0E81
        JRT    0xD0, oper_checkpoint
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        MVI    0x5D, step_code
        CMI    0x77, 0x0ECD
        JC     0x60, L_0ED4   ; JLE
        JRT    0xF0, oper_checkpoint
L_0ED4:
        MVI    0x5E, step_code
        CMI    0x06, 0x0EDD
        JC     0xB0, L_0EE4
        JRT    0xF0, oper_checkpoint
L_0EE4:
        MVI    0x5F, step_code
        CMI    0x33, 0x0EED
        JC     0x90, L_0EF4
        JRT    0xF0, oper_checkpoint
L_0EF4:
        JS2    selftest_decimal
        NOP2  
        NOP2  
        NOP2  
        NOP2  
L_0F00:
        MVI    0x60, step_code
        LA     1, L_1100
L_0F08:
        LA     0, 0x2000
L_0F0C:
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x107B
        CMR    0, 0x107D
        JC     0xD0, L_0F0C
        LA     0, 0x2000
L_0F22:
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x107B
        CMR    0, 0x107D
        JC     0xD0, L_0F22
        CMR    1, 0x107F
        JC     0xD0, L_0F48
        LA     1, 0x1200
        JC     0xF0, L_0F08   ; JMP/JANY
L_0F48:
        CMR    1, 0x1081
        JC     0xD0, L_0F58
        LA     1, 0x1300
        JC     0xF0, L_0F08   ; JMP/JANY
L_0F58:
        NOP2  
        NOP2  
        JS2    L_0F00
        JC     0xF0, L_1600   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
L_0F70:
        LA     0, main
        LA     1, 0x2100
L_0F78:
        MVC    256, 0x000(1), 0x000(0)
        AMR    0, 0x107B
        AMR    1, 0x107B
        CMR    0, 0x1083
        JC     0xD0, L_0F78
        JC     0xF0, 0x2F9E   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        MVC    2, 0x0006, 0x3084
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        MVI    0x61, step_code_mem
        LA     1, 0x3100
        LA     0, main
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x307B
        CMR    0, 0x3087
        JC     0xD0, 0x2FC2
        LA     0, main
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, 0x305E
        AMR    0, 0x307B
        CMR    0, 0x3087
        JC     0xD0, 0x2FD8
        NOP2  
        NOP2  
        NOP2  
        CMR    1, 0x308D
        JC     0xD0, 0x3004
        LA     1, 0x3200
        JC     0xF0, 0x2FBE   ; JMP/JANY
        CMR    1, 0x308F
        JC     0xD0, 0x3014
        LA     1, 0x3300
        JC     0xF0, 0x2FBE   ; JMP/JANY
        JS2    0x2FB0
        JC     0xF0, 0x3670   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        MVC    2, 0x0006, 0x307A
        NOP2  
        NOP2  
        NOP2  
        LA     0, main
        LA     1, 0x2100
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x307B
        AMR    1, 0x307B
        CMR    0, 0x3083
        JC     0xD0, 0x3038
        JC     0xF0, memtest_seg23_entry   ; JMP/JANY
memtest_seg23_entry:          ; memory test driven through change-register segment 2/3
        NOP2  
        JC     0xF0, L_17F2   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
memtest_error_halt:          ; FAULT handler for the memory tests
        MVC    2, 0x0006, reg7
        LON   
        MVC    1, 0x3079, step_code_mem
        MVC    2, 0x30B4, 0x0006
        JS2    0x30B0
        JC     0xF0, 0x3400   ; JMP/JANY
        DB     0x01        ; .
        DB     0x00        ; .
        JCC    0x00, L_1100
        DB     0x12        ; .
        DB     0x00        ; .
        DB     0x20        ; .
        DB     0x00        ; .
        DB     0x21        ; .
        DB     0x00        ; .
        DB     0x20        ; .
        DB     0x00        ; .
        NOP2  
        NOP2  
        DB     0x31        ; .
        DB     0x00        ; .
        DB     0x32        ; .
        DB     0x00        ; .
        NOP2  
        DB     0x00        ; .
        DB     0x01        ; .
        DB     0x00        ; .
        INS   
        DB     0x01        ; .
        DB     0x00        ; .
        DB     0xF0        ; .
        DB     0x00        ; .
        NOP2  
        SB     7, 1, 0x003F, 0xFC1(7)
        DB     0x42        ; 2
        DB     0x00        ; .
        DB     0x9F        ; .
        JCC    0x00, 0x000(0)
        DB     0x5F        ; \
        SB     1, 8, 0x0007, 0x0002
        JCC    0x40, 0x000(7)   ; JL/JLT
        DB     0x00        ; .
        DB     0x10        ; .
        DB     0x00        ; .
        DB     0x14        ; .
        DB     0x00        ; .
        DB     0x24        ; .
        DB     0x00        ; .
        DB     0x0F        ; .
        SB     1, 8, 0x0007, 0x0007
        DB     0x00        ; .
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
L_1100:
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        DB     0x01        ; .
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0xFFF(7)
        SB     16, 16, 0xFFF(7), 0x0A0A
final_halt:          ; HLT sled; show_code_then_halt lands at 0x1400+step_code, so
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        HLT   
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
        DB     0x5C        ; ]
        DB     0xC6        ; .
L_1600:
        MVI    0x62, step_code
        LA     0, 0x2000
L_1608:
        MVC    256, 0x000(0), 0x1500
        AMR    0, 0x107B
        CMR    0, 0x107D
        JC     0xD0, L_1608
        LA     0, 0x2000
L_161E:
        CMI    0x5C, 0x000(0)
        JRT    0xD0, oper_checkpoint
        CMI    0xC6, 0x001(0)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x1095
        CMR    0, 0x107D
        JC     0xD0, L_161E
        MVI    0x63, step_code
        LA     1, 0x3FFF
L_1642:
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x1095
        CMI    0x5C, 0x00F0
        JRT    0xD0, oper_checkpoint
        CMR    1, 0x1097
        JC     0xD0, L_1642
        JS2    L_1600
        JC     0xF0, L_0F70   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        MVI    0x64, step_code_mem
        MVC    246, 0x0008, 0x3500
        LA     0, 0x0008
        CMI    0x5C, 0x000(0)
        JRT    0xD0, 0x305E
        CMI    0xC6, 0x001(0)
        JRT    0xD0, 0x305E
        AMR    0, 0x3095
        CMR    0, 0x3099
        JC     0xD0, 0x367E
        MVI    0x65, step_code_mem
        LA     1, 0x00EF
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x3095
        CMI    0x5C, 0x00F0
        JRT    0xD0, 0x305E
        CMR    1, 0x309B
        JC     0xD0, 0x36A2
        NOP2  
        NOP2  
        NOP2  
        MVI    0x64, step_code_mem
        LA     0, main
        MVC    256, 0x000(0), 0x3500
        AMR    0, 0x307B
        CMR    0, 0x3087
        JC     0xD0, 0x36CC
        LA     0, main
        CMI    0x5C, 0x000(0)
        JRT    0xD0, 0x305E
        CMI    0xC6, 0x001(0)
        JRT    0xD0, 0x305E
        AMR    0, 0x3095
        CMR    0, 0x3087
        JC     0xD0, 0x36E2
        MVI    0x65, step_code_mem
        LA     1, 0x1FFF
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x3095
        CMI    0x5C, 0x00F0
        JRT    0xD0, 0x305E
        CMR    1, 0x309D
        JC     0xD0, 0x3706
        JS2    0x3670
        JC     0xF0, 0x301E   ; JMP/JANY
select_test:          ; read opt_image, dispatch by bit 0x10/0x20/0x40/0x80
        CMI    0x10, opt_image
        JC     0xD0, sel_chk_20
        MVI    0xF0, 0x19C3
        JC     0xF0, run_test_engine   ; JMP/JANY
sel_chk_20:          ; option 0x20 ?
        CMI    0x20, opt_image
        JC     0xD0, sel_chk_40
        MVI    0xF0, 0x18F5
        JC     0xF0, run_test_engine   ; JMP/JANY
sel_chk_40:          ; option 0x40 ?
        CMI    0x40, opt_image
        JC     0xD0, sel_chk_80
sel_enable_40:          ; enable the 0x40 test path
        MVI    0xF0, 0x17FF
        JC     0xF0, run_test_engine   ; JMP/JANY
idle_halt:          ; HLT: no test option selected (default landing)
        HLT   
        JC     0xF0, idle_halt   ; JMP/JANY
run_test_engine:          ; reset counters, print banner (PER), run self-test
        MVI    0x01, step_code
        MVC    1, 0x19D1, 0x0009
        MVC    6, 0x19EE, 0x0020
        MVC    1, 0x1783, 0x0009
        CMI    0xA0, 0x0023
        JC     0x20, engine_print_banner   ; JE/JEQ/JZ
        MVI    0x85, 0x178B
engine_print_banner:          ; PER: emit the test banner/result
        PER    0x00, 0x178A
        JC     0xF0, cpu_selftest   ; JMP/JANY
        DB     0x80        ; .
        DB     0x85        ; .
sel_chk_80:          ; option 0x80 ?
        CMI    0x80, opt_image
        JC     0xD0, idle_halt
        MVC    4, L_0F70, 0x17AA
        MVC    2, 0x107C, 0x17AE
        MVC    2, 0x1640, 0x17B0
        JC     0xF0, sel_enable_40   ; JMP/JANY
        JC     0xF0, memtest_seg23_entry   ; JMP/JANY
        DB     0x30        ; .
        DB     0x00        ; .
        DB     0x2F        ; .
        SB     1, 8, 0x0007, 0x0007
        DB     0x00        ; .
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
L_17F2:
        CMI    0x80, opt_image
        JC     0x20, memtest_move_lowhigh   ; JE/JEQ/JZ
memtest_4000_disp:
        NOP2  
        NOP2  
memtest_4000:          ; core test of the 0x4000-0x5FFF range (option 0x40)
        JC     0x00, report_and_end
L_1802:
        MVI    0x66, step_code
        LA     1, L_1100
mt4_fill_loop:          ; write 256-byte pattern, walk the address to the limit
        LA     0, 0x4000
L_180E:
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x107B
        CMR    0, 0x109F
        JC     0xD0, L_180E
        LA     0, 0x4000
mt4_verify_loop:          ; read back and compare (CMC 255)
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x107B
        CMR    0, 0x109F
        JC     0xD0, mt4_verify_loop
        CMR    1, 0x107F
        JC     0xD0, L_184A
        LA     1, 0x1200
        JC     0xF0, mt4_fill_loop   ; JMP/JANY
L_184A:
        CMR    1, 0x1081
        JC     0xD0, L_185A
        LA     1, 0x1300
        JC     0xF0, mt4_fill_loop   ; JMP/JANY
L_185A:
        JS2    L_1802
mt4_marker_pass:          ; address-uniqueness markers (0x5C / 0xC6)
        MVI    0x67, step_code
        LA     0, 0x4000
L_1866:
        MVC    256, 0x000(0), 0x1500
        AMR    0, 0x107B
        CMR    0, 0x109F
        JC     0xD0, L_1866
        LA     0, 0x4000
L_187C:
        CMI    0x5C, 0x000(0)
        JRT    0xD0, oper_checkpoint
        CMI    0xC6, 0x001(0)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x1095
        CMR    0, 0x109F
        JC     0xD0, L_187C
        MVI    0x68, step_code
        LA     1, 0x5FFF
mt4_countdown:          ; descending re-verify
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x1095
        CMI    0x5C, 0x00F0
        JRT    0xD0, oper_checkpoint
        CMR    1, 0x10A1
        JC     0xD0, mt4_countdown
        JS2    mt4_marker_pass
        JC     0xF0, memtest_6000   ; JMP/JANY
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
        NOP2  
memtest_6000:          ; core test of the 0x6000-0x7FFF range (option 0x20)
        NOP2  
        NOP2  
        JC     0x00, report_and_end
        NOP2  
        NOP2  
        NOP2  
        NOP2  
L_1900:
        MVI    0x69, step_code
        LA     1, L_1100
mt6_fill_loop:
        LA     0, 0x6000
L_190C:
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x107B
        CMR    0, 0x10A9
        JC     0xD0, L_190C
        LA     0, 0x6000
mt6_verify_loop:
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x107B
        CMR    0, 0x10A9
        JC     0xD0, mt6_verify_loop
        CMR    1, 0x107F
        JC     0xD0, L_1948
        LA     1, 0x1200
        JC     0xF0, mt6_fill_loop   ; JMP/JANY
L_1948:
        CMR    1, 0x1081
        JC     0xD0, L_1958
        LA     1, 0x1300
        JC     0xF0, mt6_fill_loop   ; JMP/JANY
L_1958:
        JS2    L_1900
mt6_marker_pass:
        MVI    0x6A, step_code
        LA     0, 0x6000
L_1964:
        MVC    256, 0x000(0), 0x1500
        AMR    0, 0x107B
        CMR    0, 0x10A9
        JC     0xD0, L_1964
        LA     0, 0x6000
L_197A:
        CMI    0x5C, 0x000(0)
        JRT    0xD0, oper_checkpoint
        CMI    0xC6, 0x001(0)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x1095
        CMR    0, 0x10A9
        JC     0xD0, L_197A
        MVI    0x6B, step_code
        LA     1, 0x7FFF
mt6_countdown:
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x1095
        CMI    0x5C, 0x00F0
        JRT    0xD0, oper_checkpoint
        CMR    1, 0x10AB
        JC     0xD0, mt6_countdown
        JS2    mt6_marker_pass
        NOP2  
        NOP2  
        JC     0x00, report_and_end
report_and_end:          ; JS1 cpu_selftest -> SWITCH 1 ON = re-run continuously;
        MVI    0x01, step_code
        NOP2  
        JS1    cpu_selftest
        PER    0x00, 0x19EE
        CMC    40, L_1100, 0x0036
        JC     0xD0, report_restart
end_halt:          ; End HLT (normal/success completion code 19DE)
        HLT   
        JC     0xF0, cpu_selftest   ; JMP/JANY
report_restart:          ; verify mismatch: copy results, restart at cold_start
        MVC    40, cold_start, 0x0036
        JC     0xF0, cold_start   ; JMP/JANY
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
memtest_move_lowhigh:          ; address-uniqueness move test (copy 0x0100<->0x2100)
        LA     0, main
        LA     1, 0x2100
L_19FC:
        MVC    256, 0x000(1), 0x000(0)
        AMR    0, 0x107B
        AMR    1, 0x107B
        CMR    0, 0x10B7
        JC     0xD0, L_19FC
        MVI    0x10, 0x1068
        MVI    0x10, 0x106E
        MVI    0x10, 0x1074
        MVI    0x14, 0x1078
memtest_0100:          ; core test of the 0x0100-0x0FFF range
        MVI    0x61, step_code_mem
        LA     1, L_1100
mt0_fill_loop:
        LA     0, main
L_1A2E:
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x107B
        CMR    0, 0x10B7
        JC     0xD0, L_1A2E
        LA     0, main
mt0_verify_loop:
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, memtest_error_halt
        AMR    0, 0x107B
        CMR    0, 0x10B7
        JC     0xD0, mt0_verify_loop
        AMR    1, 0x107B
        CMR    1, 0x10B9
        JC     0xD0, mt0_fill_loop
        JS2    memtest_0100
mt0_marker_pass:
        MVI    0x64, step_code_mem
        MVC    232, 0x0008, 0x1500
        LA     0, 0x0008
mt0_marker_chk:
        CMI    0x5C, 0x000(0)
        JRT    0xD0, memtest_error_halt
        CMI    0xC6, 0x001(0)
        JRT    0xD0, memtest_error_halt
        AMR    0, 0x1095
        JC     0xF0, mt0_limit_chk   ; JMP/JANY
mt0_countdown:
        MVI    0x65, step_code_mem
        LA     1, 0x00EF
L_1A98:
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x1095
        CMI    0x5C, 0x00F0
        JRT    0xD0, memtest_error_halt
        CMR    1, 0x109B
        JC     0xD0, L_1A98
        MVI    0x64, step_code_mem
        LA     0, main
mt0_marker_pass2:
        MVC    256, 0x000(0), 0x1500
        AMR    0, 0x107B
        CMR    0, 0x10B7
        JC     0xD0, mt0_marker_pass2
        LA     0, main
L_1AD2:
        CMI    0x5C, 0x000(0)
        JRT    0xD0, memtest_error_halt
        CMI    0xC6, 0x001(0)
        JRT    0xD0, memtest_error_halt
        AMR    0, 0x1095
        CMR    0, 0x10B7
        JC     0xD0, L_1AD2
        MVI    0x65, step_code_mem
        LA     1, 0x0FFF
mt0_countdown2:
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x1095
        CMI    0x5C, 0x00F0
        JRT    0xD0, memtest_error_halt
        CMR    1, 0x109D
        JC     0xD0, mt0_countdown2
        JS2    mt0_marker_pass
        LA     0, main
        LA     1, 0x2100
L_1B1E:
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x107B
        AMR    1, 0x107B
        CMR    0, 0x10B7
        JC     0xD0, L_1B1E
        MVI    0x30, 0x1068
        MVI    0x30, 0x106E
        MVI    0x30, 0x1074
        MVI    0x34, 0x1078
        LA     0, 0x1000
        LA     1, 0x2000
L_1B4C:
        MVC    256, 0x000(1), 0x000(0)
        AMR    0, 0x107B
        AMR    1, 0x107B
        CMR    0, 0x1083
        JC     0xD0, L_1B4C
        JC     0xF0, 0x2B66   ; JMP/JANY
memtest_1000:          ; core test of the 0x1000-0x1FFF range (seg-2 mirror)
        MVI    0x61, step_code
        MVI    0x24, opt_subcode
        LA     1, 0x2100
        LA     0, 0x1000
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x207B
        CMR    0, 0x2083
        JC     0xD0, 0x2B76
        LA     0, 0x1000
        CMC    256, 0x000(0), 0x000(1)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x207B
        CMR    0, 0x2083
        JC     0xD0, 0x2B8C
        AMR    1, 0x207B
        CMR    1, 0x20BB
        JC     0xD0, 0x2B72
        JS2    0x2B6E
        MVI    0x64, step_code
        LA     0, 0x1000
        MVC    256, 0x000(0), 0x2500
        AMR    0, 0x207B
        CMR    0, 0x2083
        JC     0xD0, 0x2BBA
        LA     0, 0x1000
        CMI    0x5C, 0x000(0)
        JRT    0xD0, oper_checkpoint
        CMI    0xC6, 0x001(0)
        JRT    0xD0, oper_checkpoint
        AMR    0, 0x2095
        CMR    0, 0x2083
        JC     0xD0, 0x2BD0
        MVI    0x65, step_code
        LA     1, 0x1FFF
        LA     0, 0xFFF(7)
        LR     0, 0x000(1)
        SMR    1, 0x2095
        CMI    0x5C, 0x00F0
        JRT    0xD0, oper_checkpoint
        CMR    1, 0x20BD
        JC     0xD0, 0x2BF4
        JS2    0x2BB2
        MVI    0x14, opt_subcode
        LA     0, 0x1000
        LA     1, 0x2000
        MVC    256, 0x000(0), 0x000(1)
        AMR    0, 0x207B
        AMR    1, 0x207B
        CMR    0, 0x2083
        JC     0xD0, 0x2C20
        JC     0xF0, memtest_4000_disp   ; JMP/JANY
mt0_limit_chk:
        CMR    0, 0x1099
        JC     0xD0, mt0_marker_chk
        JC     0xF0, mt0_countdown   ; JMP/JANY
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
        DB     0x00        ; .
