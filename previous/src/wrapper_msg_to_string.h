#pragma once
/*This file is autogenerated by scripts/make_ip_vga_to-txt.pl. Do not edit*/

#include "interaction_parameters.h"
#include "sampler_wrapper_actiondata.h"

inline std::string debug_wrapper_ip_to_string(int in)
{
    switch (in)
    {
    case ip_partselect:
        return "ip_partselect";
    case ip_layerselect:
        return "ip_layerselect";
    case ip_kgv_or_list:
        return "ip_kgv_or_list";
    case ip_wavedisplay:
        return "ip_wavedisplay";
    case ip_lfo_load:
        return "ip_lfo_load";
    case ip_browser:
        return "ip_browser";
    case ip_browser_mode:
        return "ip_browser_mode";
    case ip_browser_searchtext:
        return "ip_browser_searchtext";
    case ip_browser_previewbutton:
        return "ip_browser_previewbutton";
    case ip_config_slidersensitivity:
        return "ip_config_slidersensitivity";
    case ip_config_controller_id:
        return "ip_config_controller_id";
    case ip_config_controller_mode:
        return "ip_config_controller_mode";
    case ip_config_browserdirs:
        return "ip_config_browserdirs";
    case ip_config_refresh_db:
        return "ip_config_refresh_db";
    case ip_config_autopreview:
        return "ip_config_autopreview";
    case ip_config_previewvolume:
        return "ip_config_previewvolume";
    case ip_sample_prevnext:
        return "ip_sample_prevnext";
    case ip_patch_prevnext:
        return "ip_patch_prevnext";
    case ip_replace_sample:
        return "ip_replace_sample";
    case ip_sample_name:
        return "ip_sample_name";
    case ip_sample_metadata:
        return "ip_sample_metadata";
    case ip_solo:
        return "ip_solo";
    case ip_select_layer:
        return "ip_select_layer";
    case ip_select_all:
        return "ip_select_all";
    case ip_polyphony:
        return "ip_polphyony";
    case ip_vumeter:
        return "ip_vumeter";
    case ip_zone_name:
        return "ip_zone_name";
    case ip_channel:
        return "ip_channel";
    case ip_low_key:
        return "ip_low_key";
    case ip_root_key:
        return "ip_root_key";
    case ip_high_key:
        return "ip_high_key";
    case ip_low_vel:
        return "ip_low_vel";
    case ip_high_vel:
        return "ip_high_vel";
    case ip_low_key_f:
        return "ip_low_key_f";
    case ip_high_key_f:
        return "ip_high_key_f";
    case ip_low_vel_f:
        return "ip_low_vel_f";
    case ip_high_vel_f:
        return "ip_high_vel_f";
    case ip_coarse_tune:
        return "ip_coarse_tune";
    case ip_fine_tune:
        return "ip_fine_tune";
    case ip_pitchcorrect:
        return "ip_pitchcorrect";
    case ip_reverse:
        return "ip_reverse";
    case ip_pbdepth:
        return "ip_pbdepth";
    case ip_playmode:
        return "ip_playmode";
    case ip_velsense:
        return "ip_velsense";
    case ip_keytrack:
        return "ip_keytrack";
    case ip_mute_group:
        return "ip_mute_group";
    case ip_EG_a:
        return "ip_EG_a";
    case ip_EG_h:
        return "ip_EG_h";
    case ip_EG_d:
        return "ip_EG_d";
    case ip_EG_s:
        return "ip_EG_s";
    case ip_EG_r:
        return "ip_EG_r";
    case ip_EG_s0:
        return "ip_EG_s0";
    case ip_EG_s1:
        return "ip_EG_s1";
    case ip_EG_s2:
        return "ip_EG_s2";
    case ip_lforate:
        return "ip_lforate";
    case ip_lfoshape:
        return "ip_lfoshape";
    case ip_lforepeat:
        return "ip_lforepeat";
    case ip_lfocycle:
        return "ip_lfocycle";
    case ip_lfosync:
        return "ip_lfosync";
    case ip_lfotrigger:
        return "ip_lfotrigger";
    case ip_lfoshuffle:
        return "ip_lfoshuffle";
    case ip_lfoonce:
        return "ip_lfoonce";
    case ip_lfosteps:
        return "ip_lfosteps";
    case ip_lag:
        return "ip_lag";
    case ip_filter_object:
        return "ip_filter_object";
    case ip_filter_type:
        return "ip_filter_type";
    case ip_filter_bypass:
        return "ip_filter_bypass";
    case ip_filter_mix:
        return "ip_filter_mix";
    case ip_filter1_fp:
        return "ip_filter1_fp";
    case ip_filter2_fp:
        return "ip_filter2_fp";
    case ip_filter1_ip:
        return "ip_filter1_ip";
    case ip_filter2_ip:
        return "ip_filter2_ip";
    case ip_mm_src:
        return "ip_mm_src";
    case ip_mm_src2:
        return "ip_mm_src2";
    case ip_mm_dst:
        return "ip_mm_dst";
    case ip_mm_amount:
        return "ip_mm_amount";
    case ip_mm_curve:
        return "ip_mm_curve";
    case ip_mm_active:
        return "ip_mm_active";
    case ip_nc_src:
        return "ip_nc_src";
    case ip_nc_low:
        return "ip_nc_low";
    case ip_nc_high:
        return "ip_nc_high";
    case ip_ignore_part_polymode:
        return "ip_ignore_part_polymode";
    case ip_mute:
        return "ip_mute";
    case ip_pfg:
        return "ip_pfg";
    case ip_zone_aux_level:
        return "ip_zone_aux_level";
    case ip_zone_aux_balance:
        return "ip_zone_aux_balance";
    case ip_zone_aux_output:
        return "ip_zone_aux_output";
    case ip_zone_aux_outmode:
        return "ip_zone_aux_outmode";
    case ip_part_name:
        return "ip_part_name";
    case ip_part_midichannel:
        return "ip_part_midichannel";
    case ip_part_polylimit:
        return "ip_part_polylimit";
    case ip_part_transpose:
        return "ip_part_transpose";
    case ip_part_formant:
        return "ip_part_formant";
    case ip_part_polymode:
        return "ip_part_polymode";
    case ip_part_portamento:
        return "ip_part_portamento";
    case ip_part_portamento_mode:
        return "ip_part_portamento_mode";
    case ip_part_userparam_name:
        return "ip_part_userparam_name";
    case ip_part_userparam_polarity:
        return "ip_part_userparam_polarity";
    case ip_part_userparam_value:
        return "ip_part_userparam_value";
    case ip_part_filter_object:
        return "ip_part_filter_object";
    case ip_part_filter_type:
        return "ip_part_filter_type";
    case ip_part_filter_bypass:
        return "ip_part_filter_bypass";
    case ip_part_filter_mix:
        return "ip_part_filter_mix";
    case ip_part_filter1_fp:
        return "ip_part_filter1_fp";
    case ip_part_filter2_fp:
        return "ip_part_filter2_fp";
    case ip_part_filter1_ip:
        return "ip_part_filter1_ip";
    case ip_part_filter2_ip:
        return "ip_part_filter2_ip";
    case ip_part_aux_level:
        return "ip_part_aux_level";
    case ip_part_aux_balance:
        return "ip_part_aux_balance";
    case ip_part_aux_output:
        return "ip_part_aux_output";
    case ip_part_aux_outmode:
        return "ip_part_aux_outmode";
    case ip_part_vs_layers:
        return "ip_part_vs_layers";
    case ip_part_vs_distribution:
        return "ip_part_vs_distribution";
    case ip_part_vs_xf_equality:
        return "ip_part_vs_xf_equality";
    case ip_part_vs_xfade:
        return "ip_part_vs_xfade";
    case ip_part_mm_src:
        return "ip_part_mm_src";
    case ip_part_mm_src2:
        return "ip_part_mm_src2";
    case ip_part_mm_dst:
        return "ip_part_mm_dst";
    case ip_part_mm_amount:
        return "ip_part_mm_amount";
    case ip_part_mm_curve:
        return "ip_part_mm_curve";
    case ip_part_mm_active:
        return "ip_part_mm_active";
    case ip_part_nc_src:
        return "ip_part_nc_src";
    case ip_part_nc_low:
        return "ip_part_nc_low";
    case ip_part_nc_high:
        return "ip_part_nc_high";
    case ip_multi_filter_object:
        return "ip_multi_filter_object";
    case ip_multi_filter_type:
        return "ip_multi_filter_type";
    case ip_multi_filter_bypass:
        return "ip_multi_filter_bypass";
    case ip_multi_filter_fp1:
        return "ip_multi_filter_fp1";
    case ip_multi_filter_fp2:
        return "ip_multi_filter_fp2";
    case ip_multi_filter_fp3:
        return "ip_multi_filter_fp3";
    case ip_multi_filter_fp4:
        return "ip_multi_filter_fp4";
    case ip_multi_filter_fp5:
        return "ip_multi_filter_fp5";
    case ip_multi_filter_fp6:
        return "ip_multi_filter_fp6";
    case ip_multi_filter_fp7:
        return "ip_multi_filter_fp7";
    case ip_multi_filter_fp8:
        return "ip_multi_filter_fp8";
    case ip_multi_filter_fp9:
        return "ip_multi_filter_fp9";
    case ip_multi_filter_ip1:
        return "ip_multi_filter_ip1";
    case ip_multi_filter_ip2:
        return "ip_multi_filter_ip2";
    case ip_multi_filter_output:
        return "ip_multi_filter_output";
    case ip_multi_filter_pregain:
        return "ip_multi_filter_pregain";
    case ip_multi_filter_postgain:
        return "ip_multi_filter_postgain";
    }
    return std::string("unknown ip_ ") + std::to_string(in);
}

inline std::string debug_wrapper_vga_to_string(int in)
{
    switch (in)
    {
    case vga_floatval:
        return "vga_floatval";
    case vga_intval:
        return "vga_intval";
    case vga_intval_inc:
        return "vga_intval_inc";
    case vga_intval_dec:
        return "vga_intval_dec";
    case vga_boolval:
        return "vga_boolval";
    case vga_beginedit:
        return "vga_beginedit";
    case vga_endedit:
        return "vga_endedit";
    case vga_load_dropfiles:
        return "vga_load_dropfiles";
    case vga_load_patch:
        return "vga_load_patch";
    case vga_text:
        return "vga_text";
    case vga_click:
        return "vga_click";
    case vga_exec_external:
        return "vga_exec_external";
    case vga_url_external:
        return "vga_url_external";
    case vga_doc_external:
        return "vga_doc_external";
    case vga_disable_state:
        return "vga_disable_state";
    case vga_stuck_state:
        return "vga_stuck_state";
    case vga_hide:
        return "vga_hide";
    case vga_label:
        return "vga_label";
    case vga_temposync:
        return "vga_temposync";
    case vga_filter:
        return "vga_filter";
    case vga_datamode:
        return "vga_datamode";
    case vga_menu:
        return "vga_menu";
    case vga_entry_add:
        return "vga_entry_add";
    case vga_entry_add_ival_from_self:
        return "vga_entry_add_ival_from_self";
    case vga_entry_add_ival_from_self_with_id:
        return "vga_entry_add_ival_from_self_with_id";
    case vga_entry_replace_label_on_id:
        return "vga_entry_replace_label_on_id";
    case vga_entry_setactive:
        return "vga_entry_setactive";
    case vga_entry_clearall:
        return "vga_entry_clearall";
    case vga_select_zone_clear:
        return "vga_select_zone_clear";
    case vga_select_zone_primary:
        return "vga_select_zone_primary";
    case vga_select_zone_secondary:
        return "vga_select_zone_secondary";
    case vga_select_zone_previous:
        return "vga_select_zone_previous";
    case vga_select_zone_next:
        return "vga_select_zone_next";
    case vga_zone_playtrigger:
        return "vga_zone_playtrigger";
    case vga_deletezone:
        return "vga_deletezone";
    case vga_createemptyzone:
        return "vga_createemptyzone";
    case vga_clonezone:
        return "vga_clonezone";
    case vga_clonezone_next:
        return "vga_clonezone_next";
    case vga_movezonetopart:
        return "vga_movezonetopart";
    case vga_movezonetolayer:
        return "vga_movezonetolayer";
    case vga_zonelist_clear:
        return "vga_zonelist_clear";
    case vga_zonelist_populate:
        return "vga_zonelist_populate";
    case vga_zonelist_done:
        return "vga_zonelist_done";
    case vga_zonelist_mode:
        return "vga_zonelist_mode";
    case vga_toggle_zoom:
        return "vga_toggle_zoom";
    case vga_note:
        return "vga_note";
    case vga_audition_zone:
        return "vga_audition_zone";
    case vga_request_refresh:
        return "vga_request_refresh";
    case vga_set_zone_keyspan:
        return "vga_set_zone_keyspan";
    case vga_set_zone_keyspan_clone:
        return "vga_set_zone_keyspan_clone";
    case vga_openeditor:
        return "vga_openeditor";
    case vga_closeeditor:
        return "vga_closeeditor";
    case vga_wavedisp_sample:
        return "vga_wavedisp_sample";
    case vga_wavedisp_multiselect:
        return "vga_wavedisp_multiselect";
    case vga_wavedisp_plot:
        return "vga_wavedisp_plot";
    case vga_wavedisp_editpoint:
        return "vga_wavedisp_editpoint";
    case vga_steplfo_repeat:
        return "vga_steplfo_repeat";
    case vga_steplfo_shape:
        return "vga_steplfo_shape";
    case vga_steplfo_data:
        return "vga_steplfo_data";
    case vga_steplfo_data_single:
        return "vga_steplfo_data_single";
    case vga_browser_listptr:
        return "vga_browser_listptr";
    case vga_browser_entry_next:
        return "vga_browser_entry_next";
    case vga_browser_entry_prev:
        return "vga_browser_entry_prev";
    case vga_browser_entry_load:
        return "vga_browser_entry_load";
    case vga_browser_category_next:
        return "vga_browser_category_next";
    case vga_browser_category_prev:
        return "vga_browser_category_prev";
    case vga_browser_category_parent:
        return "vga_browser_category_parent";
    case vga_browser_category_child:
        return "vga_browser_category_child";
    case vga_browser_preview_start:
        return "vga_browser_preview_start";
    case vga_browser_preview_stop:
        return "vga_browser_preview_stop";
    case vga_browser_is_refreshing:
        return "vga_browser_is_refreshing";
    case vga_inject_database:
        return "vga_inject_database";
    case vga_database_samplelist:
        return "vga_database_samplelist";
    case vga_save_patch:
        return "vga_save_patch";
    case vga_save_multi:
        return "vga_save_multi";
    case vga_vudata:
        return "vga_vudata";
    case vga_set_range_and_units:
        return "vga_set_range_and_units";
    }

    jassertfalse;
    return std::string("unknown vga_ ") + std::to_string(in);
}
