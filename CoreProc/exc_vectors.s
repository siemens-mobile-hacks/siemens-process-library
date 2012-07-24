
                        .extern  abt_stack
                        .extern  und_stack

                        .extern  da_def_handler
                        .extern  pa_def_handler
                        .extern  ui_def_handler

                        .extern  swi2_def_jumper
                        .extern  swi3_def_jumper

                        .extern  pid
                        .extern  AbortCommonHandler
                        .extern  get_stack_pointer_from_proc


                        .global  da_handler_vector
                        .global  pa_handler_vector
                        .global  ui_handler_vector

                        .global  swi2_vector
                        .global  swi3_vector

                        .arm
                        @ Data Abort Handler

     da_handler_vector:

                         LDR       SP, =abt_stack+0x1000

                         SUB       R5,  LR, #0x04
                         MRS       R6,  SPSR

                         BL        pid

                         CMP       R0, #-1
                         BEQ       no_elf_dabort

                         MOV       R1, R0
                         MOV       R2, R5
                         MOV       R3, R6

                         STMFD     SP!, {R1, R5-R6}
                         BL        get_stack_pointer_from_proc
                         LDMFD     SP!, {R1, R5-R6}

                         MOV       SP, R0

                         MOV       R0, R1
                         MOV       R1, R5
                         MOV       R2, R6
                         MOV       R3, #0x01

                         BIC       R6, R6, #0xC0
                         MSR       CPSR_c, R6

                         B         AbortCommonHandler

          no_elf_dabort:

                         ADD       LR, R5, #0x04
                         LDR       R5, =da_def_handler
                         LDR       R5, [R5, #0x00]
                         MOV       PC, R5


                         @ Prefetch Abort Handler

       pa_handler_vector:

                         LDR       SP, =abt_stack+0x1000

                         SUB       R5,  LR, #0x04
                         MRS       R6,  SPSR

                         BL        pid

                         CMP       R0, #-1
                         BEQ       no_elf_pabort

                         MOV       R1, R0
                         MOV       R2, R5
                         MOV       R3, R6

                         STMFD     SP!, {R1, R5-R6}
                         BL        get_stack_pointer_from_proc
                         LDMFD     SP!, {R1, R5-R6}

                         MOV       SP, R0

                         MOV       R0, R1
                         MOV       R1, R5
                         MOV       R2, R6
                         MOV       R3, #0x02

                         BIC       R6, R6, #0xC0
                         MSR       CPSR_c, R6

                         B         AbortCommonHandler

          no_elf_pabort:

                         ADD       LR, R5, #0x04
                         LDR       R5, =pa_def_handler
                         LDR       R5, [R5, #0x00]
                         MOV       PC, R5



                         @ Undefined Instruction Handler

       ui_handler_vector:

                         LDR       SP, =und_stack+0x1000

                         SUB       R5,  LR, #0x04
                         MRS       R6,  SPSR

                         BL        pid

                         CMP       R0, #-1
                         BEQ       no_elf_undef

                         MOV       R1, R0
                         MOV       R2, R5
                         MOV       R3, R6

                         STMFD     SP!, {R1, R5-R6}
                         BL        get_stack_pointer_from_proc
                         LDMFD     SP!, {R1, R5-R6}

                         MOV       SP, R0

                         MOV       R0, R1
                         MOV       R1, R5
                         MOV       R2, R6
                         MOV       R3, #0x03

                         BIC       R6, R6, #0xC0
                         MSR       CPSR_c, R6

                         B         AbortCommonHandler

          no_elf_undef:


                         ADD       LR, R5, #0x04
                         LDR       R5, =ui_def_handler
                         LDR       R5, [R5, #0x00]
                         MOV       PC, R5

                         @(S)Exits SWI2 && SWI3

                         @ --- SWI2 ---


            swi2_vector:

                         MOV       R3, LR

                         STMFD     SP!, {R3}
                         BL        pid
                         LDMFD     SP!, {R3}

                         CMP       R0, #-1
                         BEQ       no_elf_swi2

                         MOV       R1, R0

                         LDMFD     SP!, {R2}


                         STMFD     SP!, {R1-R3}
                         BL        get_stack_pointer_from_proc
                         LDMFD     SP!, {R1-R3}

                         MOV       SP, R0

                         MOV       R0, R1
                         MOV       R1, R3
                         MOV       R3, #0x04



                         MOV       R6, R2
                         BIC       R6, R6, #0xC0
                         MSR       CPSR_c, R6

                         B         AbortCommonHandler


            no_elf_swi2:

                         LDR       R0, =swi2_def_jumper
                         LDR       R0, [R0, #0x00]
                         MOV       PC, R0


                         @ --- SWI3 ---

            swi3_vector:

                         MOV       R3, LR

                         STMFD     SP!, {R3}
                         BL        pid
                         LDMFD     SP!, {R3}

                         CMP       R0, #-1
                         BEQ       no_elf_swi3


                         MOV       R1, R0

                         LDMFD     SP!, {R2}


                         STMFD     SP!, {R1-R3}
                         BL        get_stack_pointer_from_proc
                         LDMFD     SP!, {R1-R3}

                         MOV       SP, R0

                         MOV       R0, R1
                         MOV       R1, R3
                         MOV       R3, #0x04



                         MOV       R6, R2
                         BIC       R6, R6, #0xC0
                         MSR       CPSR_c, R6

                         B         AbortCommonHandler


            no_elf_swi3:

                         LDR       R0, =swi3_def_jumper
                         LDR       R0, [R0, #0x00]
                         MOV       PC, R0





