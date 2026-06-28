<!-- GENERATED FILE — do not edit by hand. -->
<!-- Source: main/plugins/qtune_plugin_symbols.txt (regenerate with tools/gen_plugin_symbols_doc.py). -->

# Allowed Symbols

These are the **only** host symbols a Q-Tune plugin may reference. They are resolved by the firmware's ELF loader at load time; calling any other `lv_*` (or firmware) function leaves an unresolved relocation and the plugin will fail to load.

Total exported symbols: **226**.

In addition to the symbols below, standard C library functions (`printf`, `snprintf`, `memcpy`, `memset`, `malloc`, `free`, `strncpy`, …) and `<math.h>` functions (`fabsf`, `log2f`, `powf`, `roundf`, …) are available.

## LVGL subset

### Animation

`lv_anim_delete`, `lv_anim_init`, `lv_anim_set_completed_cb`, `lv_anim_set_duration`, `lv_anim_set_exec_cb`, `lv_anim_set_values`, `lv_anim_set_var`, `lv_anim_start`

### Arc widget

`lv_arc_create`, `lv_arc_set_angles`, `lv_arc_set_rotation`

### Canvas widget

`lv_canvas_create`, `lv_canvas_set_buffer`

### Color utilities

`lv_color_black`, `lv_color_darken`, `lv_color_hex`, `lv_color_white`

### Drawing

`lv_draw_triangle`, `lv_draw_triangle_dsc_init`

### Events

`lv_event_get_indev`, `lv_event_get_layer`, `lv_event_get_user_data`

### Image widget (LVGL 9 lv_image_*)

`lv_image_create`, `lv_image_set_antialias`, `lv_image_set_inner_align`, `lv_image_set_pivot`, `lv_image_set_scale`, `lv_image_set_src`

### Input device

`lv_indev_get_point`

### Label widget

`lv_label_create`, `lv_label_set_long_mode`, `lv_label_set_text`, `lv_label_set_text_fmt`, `lv_label_set_text_static`

### Line widget

`lv_line_create`, `lv_line_set_points`

### Object — core

`lv_obj_add_event_cb`, `lv_obj_add_flag`, `lv_obj_add_style`, `lv_obj_align`, `lv_obj_align_to`, `lv_obj_center`, `lv_obj_clean`, `lv_obj_create`, `lv_obj_get_child`, `lv_obj_get_height`, `lv_obj_get_width`, `lv_obj_get_x`, `lv_obj_get_y`, `lv_obj_has_flag`, `lv_obj_invalidate`, `lv_obj_move_to_index`, `lv_obj_remove_event_cb`, `lv_obj_remove_flag`, `lv_obj_remove_style`, `lv_obj_remove_style_all`, `lv_obj_set_height`, `lv_obj_set_pos`, `lv_obj_set_scroll_dir`, `lv_obj_set_scrollbar_mode`, `lv_obj_set_size`, `lv_obj_set_width`, `lv_obj_set_x`, `lv_obj_set_y`

### Object — flex layout

`lv_obj_set_flex_align`, `lv_obj_set_flex_flow`, `lv_obj_set_layout`

### Object — style setters

`lv_obj_set_style_arc_color`, `lv_obj_set_style_arc_opa`, `lv_obj_set_style_arc_width`, `lv_obj_set_style_bg_color`, `lv_obj_set_style_bg_grad_color`, `lv_obj_set_style_bg_grad_dir`, `lv_obj_set_style_bg_opa`, `lv_obj_set_style_border_color`, `lv_obj_set_style_border_opa`, `lv_obj_set_style_border_width`, `lv_obj_set_style_clip_corner`, `lv_obj_set_style_image_recolor`, `lv_obj_set_style_image_recolor_opa`, `lv_obj_set_style_length`, `lv_obj_set_style_line_color`, `lv_obj_set_style_line_opa`, `lv_obj_set_style_line_rounded`, `lv_obj_set_style_line_width`, `lv_obj_set_style_opa`, `lv_obj_set_style_pad_all`, `lv_obj_set_style_pad_bottom`, `lv_obj_set_style_pad_left`, `lv_obj_set_style_pad_right`, `lv_obj_set_style_pad_row`, `lv_obj_set_style_pad_top`, `lv_obj_set_style_radius`, `lv_obj_set_style_shadow_color`, `lv_obj_set_style_shadow_width`, `lv_obj_set_style_text_align`, `lv_obj_set_style_text_color`, `lv_obj_set_style_text_font`, `lv_obj_set_style_transform_rotation`, `lv_obj_set_style_transform_angle`, `lv_obj_set_style_transform_pivot_x`, `lv_obj_set_style_transform_pivot_y`

### Palette

`lv_palette_darken`, `lv_palette_lighten`, `lv_palette_main`

### Percentage helper

`lv_pct`

### Scale widget

`lv_scale_add_section`, `lv_scale_create`, `lv_scale_section_set_range`, `lv_scale_section_set_style`, `lv_scale_set_angle_range`, `lv_scale_set_draw_ticks_on_top`, `lv_scale_set_label_show`, `lv_scale_set_line_needle_value`, `lv_scale_set_major_tick_every`, `lv_scale_set_mode`, `lv_scale_set_range`, `lv_scale_set_rotation`, `lv_scale_set_text_src`, `lv_scale_set_total_tick_count`

### Screen

`lv_screen_active`

### Style

`lv_style_init`, `lv_style_reset`, `lv_style_set_arc_color`, `lv_style_set_arc_width`, `lv_style_set_bg_color`, `lv_style_set_line_color`, `lv_style_set_line_width`, `lv_style_set_pad_all`, `lv_style_set_text_align`, `lv_style_set_text_color`, `lv_style_set_text_font`, `lv_style_set_transform_rotation`, `lv_style_set_transform_angle`, `lv_style_set_transform_pivot_x`, `lv_style_set_transform_pivot_y`, `lv_style_set_width`

### Timer

`lv_timer_create`, `lv_timer_delete`, `lv_timer_set_period`

## Q-Tune host accessors and screen globals

### Screen geometry globals

`screen_width`, `screen_height`, `is_landscape`

### User-setting read-only accessors

`qt_get_reference_frequency`, `qt_get_in_tune_cents_width`, `qt_get_monitoring_mode`, `qt_get_bypass_type`, `qt_get_note_name_palette`, `qt_get_show_cents`

### Note-name glyph image accessors

`qt_get_note_glyph`, `qt_get_sharp_glyph`, `qt_get_blank_glyph`, `qt_note_is_sharp`, `qt_get_mute_glyph`

### Misc utilities

`qt_uptime_ms`, `qt_random_u32`

### Plugin persistent state

`qt_state_set_blob`, `qt_state_get_blob`, `qt_state_has`, `qt_state_erase`, `qt_state_commit`, `qt_state_shared_set_blob`, `qt_state_shared_get_blob`, `qt_state_shared_erase`, `qt_state_shared_commit`

## LVGL fonts

`lv_font_montserrat_14`, `lv_font_montserrat_18`, `lv_font_montserrat_24`, `lv_font_montserrat_26`, `lv_font_montserrat_28`, `lv_font_montserrat_40`, `lv_font_montserrat_48`

## libc / libm subset

`snprintf`, `vsnprintf`, `strncpy`, `strncmp`, `fabsf`, `roundf`, `floorf`, `ceilf`, `powf`, `log2f`, `logf`, `expf`, `sqrtf`, `fmodf`, `sinf`, `cosf`, `atan2f`

## libgcc soft-float / runtime helpers

`__addsf3`, `__subsf3`, `__mulsf3`, `__divsf3`, `__negsf2`, `__adddf3`, `__subdf3`, `__muldf3`, `__divdf3`, `__negdf2`, `__extendsfdf2`, `__truncdfsf2`, `__floatsisf`, `__floatunsisf`, `__floatsidf`, `__floatunsidf`, `__floatdisf`, `__floatdidf`, `__fixsfsi`, `__fixunssfsi`, `__fixdfsi`, `__fixunsdfsi`, `__fixsfdi`, `__fixdfdi`, `__eqsf2`, `__nesf2`, `__ltsf2`, `__lesf2`, `__gtsf2`, `__gesf2`, `__unordsf2`, `__eqdf2`, `__nedf2`, `__ltdf2`, `__ledf2`, `__gtdf2`, `__gedf2`, `__unorddf2`

---

Need a symbol that isn't here? It must first be added to the firmware's `main/plugins/qtune_plugin_symbols.txt` and re-exported. If there's an additional symbol you'd like exported, let us know on our Discord server: https://discord.gg/evtjkEj9GX
