#define _LANGUAGE_ASSEMBLY
#include "asm.h"
#include "patch_asm.h"

// patch_inst vNTSC_11(_change_background_color) 0x81481cc8 .4byte 0xFFFF00FF
// patch_inst vNTSC_11(_test_only) 0x813270c8 nop
// patch_inst vNTSC_11(_force_english_lang) 0x8130b73c li r0, 0 // NTSC Only
// patch_inst vNTSC_11(_gameselect_hide_cubes) 0x81327454 nop

patch_inst vNTSC_10(_draw_watermark) 0x81314898 bl alpha_watermark
patch_inst vNTSC_11(_draw_watermark) 0x81314bb0 bl alpha_watermark
patch_inst vNTSC_12_001(_draw_watermark) 0x81314f48 bl alpha_watermark
patch_inst vNTSC_12_101(_draw_watermark) 0x81314f60 bl alpha_watermark
patch_inst vPAL_10(_draw_watermark) 0x81315630 bl alpha_watermark
patch_inst vPAL_11(_draw_watermark) 0x81314adc bl alpha_watermark
patch_inst vPAL_12(_draw_watermark) 0x81315770 bl alpha_watermark

patch_inst vNTSC_10(_gameselect_replace_draw) 0x81314200 bl mod_gameselect_draw
patch_inst vNTSC_11(_gameselect_replace_draw) 0x81314518 bl mod_gameselect_draw
patch_inst vNTSC_12_001(_gameselect_replace_draw) 0x813148b0 bl mod_gameselect_draw
patch_inst vNTSC_12_101(_gameselect_replace_draw) 0x813148c8 bl mod_gameselect_draw
patch_inst vPAL_10(_gameselect_replace_draw) 0x81314e58 bl mod_gameselect_draw
patch_inst vPAL_11(_gameselect_replace_draw) 0x81314444 bl mod_gameselect_draw
patch_inst vPAL_12(_gameselect_replace_draw) 0x81314f98 bl mod_gameselect_draw

patch_inst vNTSC_10(_gameselect_replace_input) 0x81326818 bl handle_gameselect_inputs
patch_inst vNTSC_11(_gameselect_replace_input) 0x81326f94 bl handle_gameselect_inputs
patch_inst vNTSC_12_001(_gameselect_replace_input) 0x8132732c bl handle_gameselect_inputs
patch_inst vNTSC_12_101(_gameselect_replace_input) 0x81327344 bl handle_gameselect_inputs
patch_inst vPAL_10(_gameselect_replace_input) 0x81327968 bl handle_gameselect_inputs
patch_inst vPAL_11(_gameselect_replace_input) 0x81326ec0 bl handle_gameselect_inputs
patch_inst vPAL_12(_gameselect_replace_input) 0x81327aa8 bl handle_gameselect_inputs

.macro routine_gameselect_matrix_helper
    addi r3, r1, 0x74
    addi r4, r1, 0x14
    bl set_gameselect_view
    rept_inst 44 nop
.endm

# NOTE: this is a mid-finction patch
patch_inst vNTSC_10(_gameselect_draw_helper) 0x81326c14 routine_gameselect_matrix_helper
patch_inst vNTSC_11(_gameselect_draw_helper) 0x81327430 routine_gameselect_matrix_helper
patch_inst vNTSC_12_001(_gameselect_draw_helper) 0x813277c8 routine_gameselect_matrix_helper
patch_inst vNTSC_12_101(_gameselect_draw_helper) 0x813277e0 routine_gameselect_matrix_helper
patch_inst vPAL_10(_gameselect_draw_helper) 0x81327e04 routine_gameselect_matrix_helper
patch_inst vPAL_11(_gameselect_draw_helper) 0x8132735c routine_gameselect_matrix_helper
patch_inst vPAL_12(_gameselect_draw_helper) 0x81327f44 routine_gameselect_matrix_helper

// patch_inst vNTSC_11(_disable_main_menu_text) 0x81314d24 nop
// patch_inst vNTSC_11(_disable_main_menu_cube_a) 0x8130df44 nop
// patch_inst vNTSC_11(_disable_main_menu_cube_b) 0x8130dfac nop
// patch_inst vNTSC_11(_disable_menu_transition_cube) 0x8130de04 nop
// patch_inst vNTSC_11(_disable_menu_transition_outer) 0x8130ddc0 nop

// patch_inst vNTSC_11(_disable_menu_transition_text_a) 0x8130de88 nop
// patch_inst vNTSC_11(_disable_menu_transition_text_b) 0x8130de70 nop

// patch_inst vNTSC_11(_disable_options_menu_transition) 0x81327c4c nop
// patch_inst vNTSC_11(_disable_memory_card_detect) 0x813010ec nop

patch_inst_ntsc "_stub_dvdwait" 0x00000000 0x8130108c 0x81301440 0x81301444 nop
patch_inst_pal  "_stub_dvdwait" 0x8130108c 0x8130108c 0x813011f8 nop

patch_inst_ntsc "_replace_bs2tick" 0x81300a70 0x81300968 0x81300d08 0x81300d0c b bs2tick
patch_inst_pal  "_replace_bs2tick" 0x81300968 0x81300968 0x81300ac0 b bs2tick

patch_inst_ntsc "_replace_bs2start_a" 0x813023e0 0x813021e8 0x81302590 0x813025a8 bl bs2start
patch_inst_pal  "_replace_bs2start_a" 0x813021e8 0x813021e8 0x8130235c bl bs2start

patch_inst vNTSC_10(_replace_bs2start_b) 0x813024a4 bl bs2start // TODO: find offsets
patch_inst vNTSC_11(_replace_bs2start_b) 0x813022ac bl bs2start // TODO: find offsets
patch_inst vNTSC_12_001(_replace_bs2start_b) 0x81302648 bl bs2start // TODO: find offsets
patch_inst vNTSC_12_101(_replace_bs2start_b) 0x81302660 bl bs2start // TODO: find offsets
patch_inst vPAL_10(_replace_bs2start_b) 0x813022ac bl bs2start // TODO: find offsets
patch_inst vPAL_11(_replace_bs2start_b) 0x813022ac bl bs2start // TODO: find offsets
patch_inst vPAL_12(_replace_bs2start_b) 0x81302414 bl bs2start // TODO: find offsets

patch_inst_ntsc "_replace_report" 0x8133491c 0x8135a344 0x81300520 0x81300520 b custom_OSReport
patch_inst_pal  "_replace_report" 0x8135d924 0x8135a264 0x81300520 b custom_OSReport

patch_inst vNTSC_10(_patch_menu_init) 0x8130128c bl pre_menu_init // TODO: find offsets
patch_inst vNTSC_11(_patch_menu_init) 0x81301094 bl pre_menu_init // TODO: find offsets
patch_inst vNTSC_12_001(_patch_menu_init) 0x81301448 bl pre_menu_init // TODO: find offsets
patch_inst vNTSC_12_101(_patch_menu_init) 0x8130144c bl pre_menu_init // TODO: find offsets
patch_inst vPAL_10(_patch_menu_init) 0x81301094 bl pre_menu_init // TODO: find offsets
patch_inst vPAL_11(_patch_menu_init) 0x81301094 bl pre_menu_init // TODO: find offsets
patch_inst vPAL_12(_patch_menu_init) 0x81301200 bl pre_menu_init // TODO: find offsets

patch_inst vNTSC_10(_patch_menu_alpha_setup) 0x81312108 bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vNTSC_11(_patch_menu_alpha_setup) 0x81312358 bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vNTSC_12_001(_patch_menu_alpha_setup) 0x813126f0 bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vNTSC_12_101(_patch_menu_alpha_setup) 0x81312708 bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vPAL_10(_patch_menu_alpha_setup) 0x81312c3c bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vPAL_11(_patch_menu_alpha_setup) 0x81312284 bl pre_menu_alpha_setup // TODO: find offsets
patch_inst vPAL_12(_patch_menu_alpha_setup) 0x81312d7c bl pre_menu_alpha_setup // TODO: find offsets

patch_inst_pal "_fix_video_mode_init" 0x81300520 0x81300520 0x81300610 bl get_tvmode

patch_inst_global "_patch_pre_main" 0x81300090 bl pre_main
