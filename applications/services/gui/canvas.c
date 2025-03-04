#include "canvas_i.h"
#include "icon_animation_i.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdint.h>
#include <u8g2_glue.h>
#include <momentum/momentum.h>

const CanvasFontParameters canvas_font_params[FontTotalNumber] = {
    [FontPrimary] = {.leading_default = 12, .leading_min = 11, .height = 8, .descender = 2},
    [FontSecondary] = {.leading_default = 11, .leading_min = 9, .height = 7, .descender = 2},
    [FontKeyboard] = {.leading_default = 11, .leading_min = 9, .height = 7, .descender = 2},
    [FontBigNumbers] = {.leading_default = 18, .leading_min = 16, .height = 15, .descender = 0},
    [FontBatteryPercent] = {.leading_default = 11, .leading_min = 9, .height = 6, .descender = 0},
};

Canvas* canvas_init() {
    Canvas* canvas = malloc(sizeof(Canvas));
    canvas->compress_icon = compress_icon_alloc();

    // Initialize mutex
    canvas->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    // Initialize callback array
    CanvasCallbackPairArray_init(canvas->canvas_callback_pair);

    // Setup u8g2
    u8g2_Setup_st756x_flipper(&canvas->fb, U8G2_R0, u8x8_hw_spi_stm32, u8g2_gpio_and_delay_stm32);
    canvas->orientation = CanvasOrientationHorizontal;
    // Initialize display
    u8g2_InitDisplay(&canvas->fb);
    // Wake up display
    u8g2_SetPowerSave(&canvas->fb, 0);

    // Clear buffer and send to device
    canvas_clear(canvas);
    canvas_commit(canvas);

    return canvas;
}

void canvas_free(Canvas* canvas) {
    furi_assert(canvas);
    compress_icon_free(canvas->compress_icon);
    CanvasCallbackPairArray_clear(canvas->canvas_callback_pair);
    furi_mutex_free(canvas->mutex);
    free(canvas);
}

static void canvas_lock(Canvas* canvas) {
    furi_assert(canvas);
    furi_check(furi_mutex_acquire(canvas->mutex, FuriWaitForever) == FuriStatusOk);
}

static void canvas_unlock(Canvas* canvas) {
    furi_assert(canvas);
    furi_check(furi_mutex_release(canvas->mutex) == FuriStatusOk);
}

void canvas_reset(Canvas* canvas) {
    furi_assert(canvas);

    canvas_clear(canvas);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_set_font_direction(canvas, CanvasDirectionLeftToRight);
}

void canvas_commit(Canvas* canvas) {
    furi_assert(canvas);
    u8g2_SendBuffer(&canvas->fb);

    // Iterate over callbacks
    canvas_lock(canvas);
    for
        M_EACH(p, canvas->canvas_callback_pair, CanvasCallbackPairArray_t) {
            p->callback(
                canvas_get_buffer(canvas),
                canvas_get_buffer_size(canvas),
                canvas_get_orientation(canvas),
                p->context);
        }
    canvas_unlock(canvas);
}

uint8_t* canvas_get_buffer(Canvas* canvas) {
    furi_assert(canvas);
    return u8g2_GetBufferPtr(&canvas->fb);
}

size_t canvas_get_buffer_size(const Canvas* canvas) {
    furi_assert(canvas);
    return u8g2_GetBufferTileWidth(&canvas->fb) * u8g2_GetBufferTileHeight(&canvas->fb) * 8;
}

void canvas_frame_set(
    Canvas* canvas,
    uint8_t offset_x,
    uint8_t offset_y,
    uint8_t width,
    uint8_t height) {
    furi_assert(canvas);
    canvas->offset_x = offset_x;
    canvas->offset_y = offset_y;
    canvas->width = width;
    canvas->height = height;
}

uint8_t canvas_width(const Canvas* canvas) {
    furi_assert(canvas);
    return canvas->width;
}

uint8_t canvas_height(const Canvas* canvas) {
    furi_assert(canvas);
    return canvas->height;
}

uint8_t canvas_current_font_height(const Canvas* canvas) {
    furi_assert(canvas);
    uint8_t font_height = u8g2_GetMaxCharHeight(&canvas->fb);

    if(canvas->fb.font == u8g2_font_haxrcorp4089_tr) {
        font_height += 1;
    }

    return font_height;
}

uint8_t canvas_current_font_width(const Canvas* canvas) {
    furi_assert(canvas);
    return (uint8_t)u8g2_GetMaxCharWidth(&canvas->fb);
}

const CanvasFontParameters* canvas_get_font_params(const Canvas* canvas, Font font) {
    furi_assert(canvas);
    furi_assert(font < FontTotalNumber);
    if(asset_packs.font_params[font]) {
        return asset_packs.font_params[font];
    }
    return &canvas_font_params[font];
}

void canvas_clear(Canvas* canvas) {
    furi_assert(canvas);
    if(momentum_settings.dark_mode) {
        u8g2_FillBuffer(&canvas->fb);
    } else {
        u8g2_ClearBuffer(&canvas->fb);
    }
}

void canvas_set_color(Canvas* canvas, Color color) {
    furi_assert(canvas);
    if(momentum_settings.dark_mode) {
        if(color == ColorBlack) {
            color = ColorWhite;
        } else if(color == ColorWhite) {
            color = ColorBlack;
        }
    }
    u8g2_SetDrawColor(&canvas->fb, color);
}

void canvas_set_font_direction(Canvas* canvas, CanvasDirection dir) {
    furi_assert(canvas);
    u8g2_SetFontDirection(&canvas->fb, dir);
}

void canvas_invert_color(Canvas* canvas) {
    canvas->fb.draw_color = !canvas->fb.draw_color;
}

void canvas_set_font(Canvas* canvas, Font font) {
    furi_assert(canvas);
    u8g2_SetFontMode(&canvas->fb, 1);
    if(asset_packs.fonts[font]) {
        u8g2_SetFont(&canvas->fb, asset_packs.fonts[font]);
        return;
    }
    switch(font) {
    case FontPrimary:
        u8g2_SetFont(&canvas->fb, u8g2_font_helvB08_tr);
        break;
    case FontSecondary:
        u8g2_SetFont(&canvas->fb, u8g2_font_haxrcorp4089_tr);
        break;
    case FontKeyboard:
        u8g2_SetFont(&canvas->fb, u8g2_font_profont11_mr);
        break;
    case FontBigNumbers:
        u8g2_SetFont(&canvas->fb, u8g2_font_profont22_tn);
        break;
    case FontBatteryPercent:
        u8g2_SetFont(&canvas->fb, u8g2_font_5x7_tr); //u8g2_font_micro_tr);
        break;
    default:
        furi_crash();
        break;
    }
}

void canvas_set_custom_u8g2_font(Canvas* canvas, const uint8_t* font) {
    furi_assert(canvas);
    u8g2_SetFontMode(&canvas->fb, 1);
    u8g2_SetFont(&canvas->fb, font);
}

void canvas_draw_str(Canvas* canvas, uint8_t x, uint8_t y, const char* str) {
    furi_assert(canvas);
    if(!str) return;
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawStr(&canvas->fb, x, y, str);
}

void canvas_draw_str_aligned(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    Align horizontal,
    Align vertical,
    const char* str) {
    furi_assert(canvas);
    if(!str) return;
    x += canvas->offset_x;
    y += canvas->offset_y;

    switch(horizontal) {
    case AlignLeft:
        break;
    case AlignRight:
        x -= u8g2_GetStrWidth(&canvas->fb, str);
        break;
    case AlignCenter:
        x -= (u8g2_GetStrWidth(&canvas->fb, str) / 2);
        break;
    default:
        furi_crash();
        break;
    }

    switch(vertical) {
    case AlignTop:
        y += u8g2_GetAscent(&canvas->fb);
        break;
    case AlignBottom:
        break;
    case AlignCenter:
        y += (u8g2_GetAscent(&canvas->fb) / 2);
        break;
    default:
        furi_crash();
        break;
    }

    u8g2_DrawStr(&canvas->fb, x, y, str);
}

uint16_t canvas_string_width(Canvas* canvas, const char* str) {
    furi_assert(canvas);
    if(!str) return 0;
    return u8g2_GetStrWidth(&canvas->fb, str);
}

uint8_t canvas_glyph_width(Canvas* canvas, uint16_t symbol) {
    furi_assert(canvas);
    return u8g2_GetGlyphWidth(&canvas->fb, symbol);
}

void canvas_draw_bitmap(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    const uint8_t* compressed_bitmap_data) {
    furi_assert(canvas);

    x += canvas->offset_x;
    y += canvas->offset_y;
    uint8_t* bitmap_data = NULL;
    compress_icon_decode(canvas->compress_icon, compressed_bitmap_data, &bitmap_data);
    canvas_draw_u8g2_bitmap(&canvas->fb, x, y, width, height, bitmap_data, IconRotation0);
}

void canvas_draw_icon_animation(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    IconAnimation* icon_animation) {
    furi_assert(canvas);
    furi_assert(icon_animation);

    x += canvas->offset_x;
    y += canvas->offset_y;
    uint8_t* icon_data = NULL;
    compress_icon_decode(
        canvas->compress_icon, icon_animation_get_data(icon_animation), &icon_data);
    canvas_draw_u8g2_bitmap(
        &canvas->fb,
        x,
        y,
        icon_animation_get_width(icon_animation),
        icon_animation_get_height(icon_animation),
        icon_data,
        IconRotation0);
}

static void canvas_draw_u8g2_bitmap_int(
    u8g2_t* u8g2,
    u8g2_uint_t x,
    u8g2_uint_t y,
    u8g2_uint_t w,
    u8g2_uint_t h,
    bool mirror,
    bool rotation,
    const uint8_t* bitmap) {
    u8g2_uint_t blen;
    blen = w;
    blen += 7;
    blen >>= 3;

    if(rotation && !mirror) {
        x += w + 1;
    } else if(mirror && !rotation) {
        y += h - 1;
    }

    while(h > 0) {
        const uint8_t* b = bitmap;
        uint16_t len = w;
        uint16_t x0 = x;
        uint16_t y0 = y;
        uint8_t mask;
        uint8_t color = u8g2->draw_color;
        uint8_t ncolor = (color == 0 ? 1 : 0);
        mask = 1;

        while(len > 0) {
            if(u8x8_pgm_read(b) & mask) {
                u8g2->draw_color = color;
                u8g2_DrawHVLine(u8g2, x0, y0, 1, 0);
            } else if(u8g2->bitmap_transparency == 0) {
                u8g2->draw_color = ncolor;
                u8g2_DrawHVLine(u8g2, x0, y0, 1, 0);
            }

            if(rotation) {
                y0++;
            } else {
                x0++;
            }

            mask <<= 1;
            if(mask == 0) {
                mask = 1;
                b++;
            }
            len--;
        }

        u8g2->draw_color = color;
        bitmap += blen;

        if(mirror) {
            if(rotation) {
                x++;
            } else {
                y--;
            }
        } else {
            if(rotation) {
                x--;
            } else {
                y++;
            }
        }
        h--;
    }
}

void canvas_draw_u8g2_bitmap(
    u8g2_t* u8g2,
    u8g2_uint_t x,
    u8g2_uint_t y,
    u8g2_uint_t w,
    u8g2_uint_t h,
    const uint8_t* bitmap,
    IconRotation rotation) {
    u8g2_uint_t blen;
    blen = w;
    blen += 7;
    blen >>= 3;
#ifdef U8G2_WITH_INTERSECTION
    if(u8g2_IsIntersection(u8g2, x, y, x + w, y + h) == 0) return;
#endif /* U8G2_WITH_INTERSECTION */

    switch(rotation) {
    case IconRotation0:
        canvas_draw_u8g2_bitmap_int(u8g2, x, y, w, h, 0, 0, bitmap);
        break;
    case IconRotation90:
        canvas_draw_u8g2_bitmap_int(u8g2, x, y, w, h, 0, 1, bitmap);
        break;
    case IconRotation180:
        canvas_draw_u8g2_bitmap_int(u8g2, x, y, w, h, 1, 0, bitmap);
        break;
    case IconRotation270:
        canvas_draw_u8g2_bitmap_int(u8g2, x, y, w, h, 1, 1, bitmap);
        break;
    default:
        break;
    }
}

void canvas_draw_icon_ex(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    const Icon* icon,
    IconRotation rotation) {
    furi_assert(canvas);
    furi_assert(icon);

    x += canvas->offset_x;
    y += canvas->offset_y;
    uint8_t* icon_data = NULL;
    compress_icon_decode(canvas->compress_icon, icon_get_data(icon), &icon_data);
    canvas_draw_u8g2_bitmap(
        &canvas->fb, x, y, icon_get_width(icon), icon_get_height(icon), icon_data, rotation);
}

void canvas_draw_icon(Canvas* canvas, uint8_t x, uint8_t y, const Icon* icon) {
    furi_assert(canvas);
    furi_assert(icon);

    x += canvas->offset_x;
    y += canvas->offset_y;
    uint8_t* icon_data = NULL;
    compress_icon_decode(canvas->compress_icon, icon_get_data(icon), &icon_data);
    canvas_draw_u8g2_bitmap(
        &canvas->fb, x, y, icon_get_width(icon), icon_get_height(icon), icon_data, IconRotation0);
}

void canvas_draw_dot(Canvas* canvas, uint8_t x, uint8_t y) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawPixel(&canvas->fb, x, y);
}

void canvas_draw_box(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawBox(&canvas->fb, x, y, width, height);
}

void canvas_draw_rbox(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint8_t radius) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawRBox(&canvas->fb, x, y, width, height, radius);
}

void canvas_draw_frame(Canvas* canvas, uint8_t x, uint8_t y, uint8_t width, uint8_t height) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawFrame(&canvas->fb, x, y, width, height);
}

void canvas_draw_rframe(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t width,
    uint8_t height,
    uint8_t radius) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawRFrame(&canvas->fb, x, y, width, height, radius);
}

void canvas_draw_line(Canvas* canvas, uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2) {
    furi_assert(canvas);
    x1 += canvas->offset_x;
    y1 += canvas->offset_y;
    x2 += canvas->offset_x;
    y2 += canvas->offset_y;
    u8g2_DrawLine(&canvas->fb, x1, y1, x2, y2);
}

void canvas_draw_circle(Canvas* canvas, uint8_t x, uint8_t y, uint8_t radius) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawCircle(&canvas->fb, x, y, radius, U8G2_DRAW_ALL);
}

void canvas_draw_disc(Canvas* canvas, uint8_t x, uint8_t y, uint8_t radius) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawDisc(&canvas->fb, x, y, radius, U8G2_DRAW_ALL);
}

void canvas_draw_triangle(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t base,
    uint8_t height,
    CanvasDirection dir) {
    furi_assert(canvas);
    if(dir == CanvasDirectionBottomToTop) {
        canvas_draw_line(canvas, x - base / 2, y, x + base / 2, y);
        canvas_draw_line(canvas, x - base / 2, y, x, y - height + 1);
        canvas_draw_line(canvas, x, y - height + 1, x + base / 2, y);
    } else if(dir == CanvasDirectionTopToBottom) {
        canvas_draw_line(canvas, x - base / 2, y, x + base / 2, y);
        canvas_draw_line(canvas, x - base / 2, y, x, y + height - 1);
        canvas_draw_line(canvas, x, y + height - 1, x + base / 2, y);
    } else if(dir == CanvasDirectionRightToLeft) {
        canvas_draw_line(canvas, x, y - base / 2, x, y + base / 2);
        canvas_draw_line(canvas, x, y - base / 2, x - height + 1, y);
        canvas_draw_line(canvas, x - height + 1, y, x, y + base / 2);
    } else if(dir == CanvasDirectionLeftToRight) {
        canvas_draw_line(canvas, x, y - base / 2, x, y + base / 2);
        canvas_draw_line(canvas, x, y - base / 2, x + height - 1, y);
        canvas_draw_line(canvas, x + height - 1, y, x, y + base / 2);
    }
}

void canvas_draw_xbm(
    Canvas* canvas,
    uint8_t x,
    uint8_t y,
    uint8_t w,
    uint8_t h,
    const uint8_t* bitmap) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    canvas_draw_u8g2_bitmap(&canvas->fb, x, y, w, h, bitmap, IconRotation0);
}

void canvas_draw_glyph(Canvas* canvas, uint8_t x, uint8_t y, uint16_t ch) {
    furi_assert(canvas);
    x += canvas->offset_x;
    y += canvas->offset_y;
    u8g2_DrawGlyph(&canvas->fb, x, y, ch);
}

void canvas_set_bitmap_mode(Canvas* canvas, bool alpha) {
    u8g2_SetBitmapMode(&canvas->fb, alpha ? 1 : 0);
}

void canvas_set_orientation(Canvas* canvas, CanvasOrientation orientation) {
    furi_assert(canvas);
    const u8g2_cb_t* rotate_cb = NULL;
    bool need_swap = false;
    if(canvas->orientation != orientation) {
        switch(orientation) {
        case CanvasOrientationHorizontal:
            need_swap = canvas->orientation == CanvasOrientationVertical ||
                        canvas->orientation == CanvasOrientationVerticalFlip;
            rotate_cb = U8G2_R0;
            break;
        case CanvasOrientationHorizontalFlip:
            need_swap = canvas->orientation == CanvasOrientationVertical ||
                        canvas->orientation == CanvasOrientationVerticalFlip;
            rotate_cb = U8G2_R2;
            break;
        case CanvasOrientationVertical:
            need_swap = canvas->orientation == CanvasOrientationHorizontal ||
                        canvas->orientation == CanvasOrientationHorizontalFlip;
            rotate_cb = U8G2_R3;
            break;
        case CanvasOrientationVerticalFlip:
            need_swap = canvas->orientation == CanvasOrientationHorizontal ||
                        canvas->orientation == CanvasOrientationHorizontalFlip;
            rotate_cb = U8G2_R1;
            break;
        default:
            furi_crash();
        }

        if(need_swap) FURI_SWAP(canvas->width, canvas->height);
        u8g2_SetDisplayRotation(&canvas->fb, rotate_cb);
        canvas->orientation = orientation;
    }
}

CanvasOrientation canvas_get_orientation(const Canvas* canvas) {
    return canvas->orientation;
}

void canvas_add_framebuffer_callback(Canvas* canvas, CanvasCommitCallback callback, void* context) {
    furi_assert(canvas);

    const CanvasCallbackPair p = {callback, context};

    canvas_lock(canvas);
    furi_assert(!CanvasCallbackPairArray_count(canvas->canvas_callback_pair, p));
    CanvasCallbackPairArray_push_back(canvas->canvas_callback_pair, p);
    canvas_unlock(canvas);
}

void canvas_remove_framebuffer_callback(
    Canvas* canvas,
    CanvasCommitCallback callback,
    void* context) {
    furi_assert(canvas);

    const CanvasCallbackPair p = {callback, context};

    canvas_lock(canvas);
    furi_assert(CanvasCallbackPairArray_count(canvas->canvas_callback_pair, p) == 1);
    CanvasCallbackPairArray_remove_val(canvas->canvas_callback_pair, p);
    canvas_unlock(canvas);
}