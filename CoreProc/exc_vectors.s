
                        .extern  abt_stack
                        .extern  und_stack

                        .global  da_handler_vector
                        .global  pa_handler_vector
                        .global  ui_handler_vector

                        .global  swi2_vector
                        .global  swi3_vector

                        .arm
                        @ Data Abort Handler

    da_handler_vector:

                         LDR       SP, =abt_stack+0x1000
                         MOV       R0, #1
                         B         handler_vector


                         @ Prefetch Abort Handler

    pa_handler_vector:

                         LDR       SP, =abt_stack+0x1000
                         MOV       R0, #2
                         B         handler_vector


                         @ Undefined Instruction Handler

    ui_handler_vector:

                         LDR       SP, =und_stack+0x1000
                         MOV       R0, #3
                         B         handler_vector


                         @(S)Exits SWI2 && SWI3

                         @ --- SWI2 ---


            swi2_vector:

                         MOV       R0, #4
                         B         handler_vector


                         @ --- SWI3 ---

            swi3_vector:

                         MOV       R0, #5
                         B         handler_vector





