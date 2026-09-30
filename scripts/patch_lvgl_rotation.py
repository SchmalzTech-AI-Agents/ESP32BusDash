Import("env")

from pathlib import Path

# LVGL v8 splits software-rotated areas into arbitrary-height chunks. The
# ST77922 QSPI panel requires the resulting physical X span to be a multiple
# of four pixels, so force rotation chunks to a four-pixel row boundary.
path = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "lvgl" / "src" / "core" / "lv_refr.c"
if not path.is_file():
    print("LVGL rotation patch: dependency not present yet")
else:
    text = path.read_text(encoding="utf-8")
    old = """        lv_coord_t max_row = LV_MIN((lv_coord_t)((LV_DISP_ROT_MAX_BUF / sizeof(lv_color_t)) / area_w), area_h);
        lv_coord_t init_y_off;"""
    new = """        lv_coord_t max_row = LV_MIN((lv_coord_t)((LV_DISP_ROT_MAX_BUF / sizeof(lv_color_t)) / area_w), area_h);
        // QSPI ST77922 requires the post-rotation X span to be a multiple of 4.
        // Each rotated chunk's height becomes that span, so use 4-row chunks.
        max_row &= ~((lv_coord_t)3);
        if (max_row == 0) max_row = LV_MIN((lv_coord_t)4, area_h);
        lv_coord_t init_y_off;"""
    if new in text:
        print("LVGL rotation patch: already applied")
    elif old in text:
        path.write_text(text.replace(old, new, 1), encoding="utf-8")
        print("LVGL rotation patch: applied")
    else:
        raise RuntimeError("LVGL rotation patch: expected source pattern not found")
