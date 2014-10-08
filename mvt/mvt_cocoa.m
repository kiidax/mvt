/* Multi-purpose Virtual Terminal
 * Copyright (C) 2005-2011 Katsuya Iida
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#import <Cocoa/Cocoa.h>
#include <assert.h>
#include <mvt/mvt.h>
#include "misc.h"
#include "debug.h"
#include "driver.h"

#define MVT_COCOA_SCREEN_ALWAYSBRIGHT (1<<0)
#define MVT_COCOA_SCREEN_PSEUDOBOLD   (1<<1)

typedef struct _mvt_cocoa_screen mvt_cocoa_screen_t;

@interface MVTScreenView : NSView {
    mvt_cocoa_screen_t *screen;
    NSFont *font;
    float fontSize;
    float fontWidth;
    float fontHeight;
    float fontBaseline;
    uint32_t foregroundColor;
    uint32_t backgroundColor;
    int width;
    int height;
    int cursorX, cursorY;
    int selectionStartX, selectionStartY;
    int selectionEndX, selectionEndY;
    uint32_t flags;
}
- (id) init;
- (id) initWithScreen:(mvt_cocoa_screen_t*)screen arguments:(char **)args;
- (void) setAttribute:(const char *)value forName:(const char *)name;
- (NSRect) makePreferredRect;
- (void) drawTextWithX:(int)x y:(int)y string:(const mvt_char_t *)ws attribute:(const mvt_attribute_t *)attribute length:(size_t)len;
- (void) clearRectWithX1:(int)x1 y1:(int)y1 x2:(int)x2 y2:(int)y2 backgroundColor:(mvt_color_t)background_color;
- (void) getSizeWithWidth:(int*)pwidth height:(int*)pheight;
- (void) moveCursorWithCursor:(mvt_cursor_t)cursor x:(int)x y:(int)y;
- (void) scrollFrom:(int)y1 to:(int)y2 count:(int)count;
- (void) keyDown:(NSEvent *)theEvent;
@end

struct _mvt_cocoa_screen {
    mvt_screen_t parent;
    NSWindow *window;
    NSScroller *scroller;
    MVTScreenView *view;
};

@interface MVTEnvironment : NSObject {
}
- (void)handleRequest:(id)aObj;
@end

NSString *MVTFontAttributeName = @"font";
NSString *MVTWidthAttributeName = @"width";
NSString *MVTHeightAttributeName = @"height";
NSString *MVTForegroundColorAttributeName = @"foreground-color";
NSString *MVTBackgroundColorAttributeName = @"background-color";

static int loop;

static id myApp;
static id myPool;
static MVTEnvironment* globalEnvironment;

/* utilities */
static NSColor *mvt_color_value_to_cocoa(uint32_t color_value);

/* mvt_cocoa_screen_t */

static void *mvt_cocoa_screen_begin(mvt_screen_t *screen);
static void mvt_cocoa_screen_end(mvt_screen_t *screen, void *gc);
static void mvt_cocoa_screen_draw_text(mvt_screen_t *screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t len);
static void mvt_cocoa_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color);
static void mvt_cocoa_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y);
static void mvt_cocoa_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count);
static void mvt_cocoa_screen_beep(mvt_screen_t *screen);
static void mvt_cocoa_screen_get_size(mvt_screen_t *screen, int *width, int *height);
static int mvt_cocoa_screen_resize(mvt_screen_t *screen, int width, int height);
static void mvt_cocoa_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws);
static void mvt_cocoa_screen_set_scroll_info(mvt_screen_t *screen, int top, int height, int virtual_height);
static void mvt_cocoa_screen_set_mode(mvt_screen_t *screen, int mode, int value);
static int mvt_cocoa_set_screen_attribute0(mvt_cocoa_screen_t *screen, const char *name, const char *value);

static const mvt_screen_vt_t cocoa_screen_vt = {
    mvt_cocoa_screen_begin,
    mvt_cocoa_screen_end,
    mvt_cocoa_screen_draw_text,
    mvt_cocoa_screen_clear_rect,
    mvt_cocoa_screen_scroll,
    mvt_cocoa_screen_move_cursor,
    mvt_cocoa_screen_beep,
    mvt_cocoa_screen_get_size,
    mvt_cocoa_screen_resize,
    mvt_cocoa_screen_set_title,
    mvt_cocoa_screen_set_scroll_info,
    mvt_cocoa_screen_set_mode
};

int mvt_cocoa_screen_init(mvt_cocoa_screen_t *cocoa_screen)
{
    memset(cocoa_screen, 0, sizeof *cocoa_screen);
    cocoa_screen->parent.vt = &cocoa_screen_vt;
    cocoa_screen->view = nil;
    return 0;
}

void
mvt_cocoa_screen_destroy(mvt_cocoa_screen_t *cocoa_screen)
{
    memset(cocoa_screen, 0, sizeof *cocoa_screen);
}

static void *mvt_cocoa_screen_begin(mvt_screen_t *screen)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    [cocoa_screen->view lockFocus];
    return cocoa_screen->view;
}

static void mvt_cocoa_screen_end(mvt_screen_t *screen, void *gc)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    [cocoa_screen->view unlockFocus];
    [cocoa_screen->window flushWindow];
}

static void mvt_cocoa_screen_draw_text(mvt_screen_t* screen, void *gc, int x, int y, const mvt_char_t *ws, const mvt_attribute_t *attribute, size_t len)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    assert(cocoa_screen->view == (MVTScreenView*)gc);
    [cocoa_screen->view drawTextWithX:x y:y string:ws attribute:attribute length:len];
}

static void mvt_cocoa_screen_clear_rect(mvt_screen_t *screen, void *gc, int x1, int y1, int x2, int y2, mvt_color_t background_color)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    assert(cocoa_screen->view == (MVTScreenView*)gc);
    [cocoa_screen->view clearRectWithX1:x1 y1:y1 x2:x2 y2:y2 backgroundColor:background_color];
}

static void mvt_cocoa_screen_move_cursor(mvt_screen_t *screen, mvt_cursor_t cursor, int x, int y)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    [cocoa_screen->view moveCursorWithCursor:cursor x:x y:y];
}

static void mvt_cocoa_screen_scroll(mvt_screen_t *screen, int y1, int y2, int count)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    [cocoa_screen->view scrollFrom:y1 to:y2 count:count];
}

static void mvt_cocoa_screen_beep(mvt_screen_t *screen)
{
    NSBeep();
}

static void mvt_cocoa_screen_get_size(mvt_screen_t *screen, int *width, int *height)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    [cocoa_screen->view getSizeWithWidth:width height:height];
}

static int mvt_cocoa_screen_resize(mvt_screen_t *screen, int width, int height)
{
#if 0
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    int pixel_width, pixel_height;
    Uint32 flags;

    pixel_width = width * cocoa_screen->font_width + MVT_SCROLLBAR_WIDTH;
    pixel_height = height * cocoa_screen->font_height;
    flags = COCOA_ANYFORMAT | COCOA_SWSURFACE | COCOA_HWSURFACE | COCOA_RESIZABLE;
    surface = COCOA_SetVideoMode(pixel_width, pixel_height, 0, flags);
    if (surface == NULL) {
        MVT_DEBUG_PRINT2("Unable to set screen mode: %s\n", COCOA_GetError());
        return -1;
    }
    cocoa_screen->surface = surface;
    cocoa_screen->width = width;
    cocoa_screen->height = height;
#endif
    return 0;
}

static void mvt_cocoa_screen_set_title(mvt_screen_t *screen, const mvt_char_t *ws)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    NSString *str;
    if (ws) {
	size_t len = 0;
	const mvt_char_t *wp = ws;
	while (*wp++) len++;
	str = [[NSString alloc] initWithBytes:ws length:sizeof (mvt_char_t) * len encoding:NSUTF32LittleEndianStringEncoding];
    } else {
	str = @"mvt";
    }
    [[cocoa_screen->view window] setTitle:str];
    [str release];
}

static void mvt_cocoa_screen_set_scroll_info(mvt_screen_t *screen, int top, int height, int total_height)
{
#if 0
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    COCOA_Rect rect, inner_rect;
    unsigned int color_value;
    NSColor cocoa_color;
    rect.x = cocoa_screen->width * cocoa_screen->font_width;
    rect.y = 0;
    rect.w = MVT_SCROLLBAR_WIDTH;
    rect.h = cocoa_screen->height * cocoa_screen->font_height;
    color_value = cocoa_screen->scroll_background_color;
    cocoa_color = COCOA_MapRGB(cocoa_screen->surface->format,
                           (color_value >> 16) & 0xff,
                           (color_value >> 8 ) & 0xff,
                           (color_value      ) & 0xff);
    COCOA_FillRect(cocoa_screen->surface, &rect, cocoa_color);
    inner_rect = rect;
    inner_rect.y = top * rect.h / total_height;
    inner_rect.h = (top + height) * rect.h / total_height - inner_rect.y;
    color_value = cocoa_screen->scroll_foreground_color;
    cocoa_color = COCOA_MapRGB(cocoa_screen->surface->format,
                           (color_value >> 16) & 0xff,
                           (color_value >> 8 ) & 0xff,
                           (color_value      ) & 0xff);
    COCOA_FillRect(cocoa_screen->surface, &inner_rect, cocoa_color);
    COCOA_UpdateRect(cocoa_screen->surface,
                   rect.x, rect.y, rect.w, rect.h);
#endif
}

static void mvt_cocoa_screen_set_mode(mvt_screen_t *screen, int mode, int value)
{
}

static void mvt_cocoa_screen_convert_point(mvt_cocoa_screen_t *cocoa_screen, int x, int y, int *cx, int *cy)
{
#if 0
    int font_width = cocoa_screen->font_width;
    int font_height = cocoa_screen->font_height;
    if (cx != NULL) *cx = x / font_width;
    if (cy != NULL) *cy = y / font_height;
#endif
}

static int mvt_cocoa_update_screen(mvt_cocoa_screen_t *cocoa_screen)
{
#if 0
    const char *font_filename;
    int font_width, font_height;
    TTF_Font *font;

    font_filename = getenv("MVT_FONTPATH");
    if (!font_filename) font_filename = "./font.ttf";
    font = TTF_OpenFont(font_filename , cocoa_screen->font_size);
    if (!font) {
        MVT_DEBUG_PRINT1("Unable to open TrueType font\n");
        return -1;
    }
    if (cocoa_screen->font)
        TTF_CloseFont(cocoa_screen->font);
    cocoa_screen->font = font;
    if (TTF_SizeText(cocoa_screen->font, "0", &font_width, &font_height) != 0)
        return -1;
    cocoa_screen->font_width = font_width;
    cocoa_screen->font_height = TTF_FontHeight(cocoa_screen->font);
    if (mvt_cocoa_screen_resize((mvt_screen_t *)cocoa_screen, cocoa_screen->width,
                              cocoa_screen->height) == -1)
        return -1;
#endif
    return 0;
}

static mvt_screen_t *mvt_cocoa_open_screen(char **args)
{
    mvt_cocoa_screen_t *cocoa_screen;

    cocoa_screen = malloc(sizeof (mvt_cocoa_screen_t));
    if (mvt_cocoa_screen_init(cocoa_screen) == -1) {
        free(cocoa_screen);
	return NULL;
    }

    if (mvt_cocoa_update_screen(cocoa_screen) == -1) {
        free(cocoa_screen);
        return NULL;
    }
    cocoa_screen->view = [[MVTScreenView alloc] initWithScreen:cocoa_screen arguments:args];
    NSRect aRect = [cocoa_screen->view makePreferredRect];
    aRect.size.width += [NSScroller scrollerWidth];
    cocoa_screen->window = [[NSWindow alloc]
			       initWithContentRect: aRect
					 styleMask: NSTitledWindowMask | NSClosableWindowMask | NSResizableWindowMask
					   backing: NSBackingStoreBuffered
					     defer: NO];
    [cocoa_screen->window setTitle: @"MVT"];
    [cocoa_screen->window setContentView:cocoa_screen->view];
    [cocoa_screen->window makeKeyAndOrderFront:nil];
    [cocoa_screen->window setInitialFirstResponder:cocoa_screen->view];

    aRect.origin.x = aRect.size.width - [NSScroller scrollerWidth];
    aRect.origin.y = 0.0;
    aRect.size.width = [NSScroller scrollerWidth];
    cocoa_screen->scroller = [[NSScroller alloc] initWithFrame:aRect];
    [cocoa_screen->scroller setKnobProportion:0.05];
    [cocoa_screen->scroller setDoubleValue:0.5];
    [cocoa_screen->scroller setEnabled:YES];
    [[cocoa_screen->window contentView] addSubview:cocoa_screen->scroller];

    return (mvt_screen_t *)cocoa_screen;
}

static void mvt_cocoa_close_screen(mvt_screen_t *screen)
{
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    mvt_screen_dispatch_close(screen);
    mvt_cocoa_screen_destroy(cocoa_screen);
    free(screen);
}

#define MVT_SCREEN_TO_COCOA(screen) (((mvt_cocoa_screen_t *)screen)->driver)

static int mvt_cocoa_set_screen_attribute0(mvt_cocoa_screen_t *cocoa_screen, const char *name, const char *value)
{
    return 0;
}

static int mvt_cocoa_set_screen_attribute(mvt_screen_t *screen, const char *name, const char *value)
{
#if 0
    mvt_cocoa_screen_t *cocoa_screen = (mvt_cocoa_screen_t *)screen;
    if (mvt_cocoa_set_screen_attribute0(cocoa_screen, name, value) == -1)
        return -1;
    if (mvt_cocoa_update_screen(cocoa_screen) == -1)
        return -1;
    if (!cocoa_screen->terminal) return 0;
    mvt_terminal_repaint(cocoa_screen->terminal);
#endif
    return 0;
}

#if 0
int mvt_cocoa_sdkkey_to_mvtkey(SDLKey key)
{
    if (key >= SDLK_KP0 && key <= SDLK_KP9)
        return (int)(key - SDLK_KP0) + MVT_KEYPAD_0;
    if (key >= SDLK_F1 && key <= SDLK_F4)
        return (int)(key - SDLK_F1) + MVT_KEYPAD_PF1;
    if (key >= SDLK_F1 && key <= SDLK_F15)
        return (int)(key - SDLK_F1) + MVT_KEYPAD_F1;
    switch (key) {
    case SDLK_SPACE:
        return MVT_KEYPAD_SPACE;
    case SDLK_TAB:
        return MVT_KEYPAD_TAB;
    case SDLK_KP_ENTER:
        return MVT_KEYPAD_ENTER;
    case SDLK_HOME:
        return MVT_KEYPAD_HOME;
    case SDLK_LEFT:
        return MVT_KEYPAD_LEFT;
    case SDLK_UP:
        return MVT_KEYPAD_UP;
    case SDLK_RIGHT:
        return MVT_KEYPAD_RIGHT;
    case SDLK_DOWN:
        return MVT_KEYPAD_DOWN;
    case SDLK_PAGEUP:
        return MVT_KEYPAD_PAGEUP;
    case SDLK_PAGEDOWN:
        return MVT_KEYPAD_PAGEDOWN;
    case SDLK_END:
        return MVT_KEYPAD_END;
    case SDLK_INSERT:
        return MVT_KEYPAD_INSERT;
    case SDLK_KP_EQUALS:
        return MVT_KEYPAD_EQUAL;
    case SDLK_KP_MULTIPLY:
        return MVT_KEYPAD_MULTIPLY;
    case SDLK_KP_PLUS:
        return MVT_KEYPAD_ADD;
    case SDLK_KP_PERIOD:
        return MVT_KEYPAD_SEPARATOR;
    case SDLK_KP_MINUS:
        return MVT_KEYPAD_SUBTRACT;
    case SDLK_KP_DIVIDE:
        return MVT_KEYPAD_DIVIDE;
    default:
        return -1;
    }
}
#endif

@implementation MVTScreenView
- (id) init {
    self = [super init];
    if (!self) return nil;
    width = 80;
    height = 24;
    fontSize = 12;
    font = [NSFont userFixedPitchFontOfSize:fontSize];
    [font retain];
    flags = MVT_COCOA_SCREEN_PSEUDOBOLD;
    selectionStartX = -1;
    selectionStartY = -1;
    selectionEndX = -1;
    selectionEndY = -1;
    return self;
}

- (id) initWithScreen:(mvt_cocoa_screen_t*)aScreen arguments:(char **)args {
    const char *name, *value;
    char **p;
    screen = aScreen;
    self = [self init];
    if (!self) return nil;
    p = args;
    while (*p) {
        name = *p++;
        if (!*p) {
	    [self release];
            return NULL;
        }
        value = *p++;
        [self setAttribute:value forName:name];
    }
    return self;
}

- (void) dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [font release];
    [super dealloc];
}

- (void) viewDidMoveToWindow
{
    printf("viewDidMoveToWindow\n");
    [[NSNotificationCenter defaultCenter] addObserver:self
					     selector:@selector(windowDidResize:)
						 name:NSWindowDidResizeNotification object:[self window]];
}

- (void) windowDidResize:(NSNotification *)notification
{
    NSSize size = [[self window] frame].size;
    NSLog(@"size=%fx%f", size.width, size.height);
    NSRect bounds = [self bounds];
    size = bounds.size;
    NSLog(@"size=%fx%f", size.width, size.height);
    width = (size.width - [NSScroller scrollerWidth]) / fontWidth;
    height = size.height / fontHeight;
    mvt_screen_dispatch_resize((mvt_screen_t *)screen);
}

- (NSRect)makePreferredRect {
#if 1
    CGFontRef font = CGFontCreateWithFontName((CFStringRef)@"Menlo-Regular");
//    CGFontRef font = (CGFontRef)self->font;
    int advances[1];
    CGGlyph glyphs[1];
    int upem;
    glyphs[0] = 100;
    CGFontGetGlyphAdvances(font, glyphs, 1, advances);
    upem = CGFontGetUnitsPerEm(font);
    fontWidth = ceil((double)advances[0] * 11 * 75 / (72 * upem));
    fontHeight = ceil((double)(CGFontGetAscent(font) - CGFontGetDescent(font) + CGFontGetLeading(font)) * 11 * 75 / (72 * upem));
    fontBaseline = CGFontGetDescent(font) * 11 * 75 / (72 * upem);
#else
    fontWidth = [font maximumAdvancement].width;
    fontWidth = floor(fontWidth);
    fontHeight = [font ascender] - [font descender] + [font leading];
    fontHeight = ceil(fontHeight);
    fontBaseline = [font descender];
#endif
    return NSMakeRect(0, 0, width * fontWidth, height * fontHeight);
}

- (void)drawTextWithX:(int)x y:(int)y string:(const mvt_char_t *)ws attribute:(const mvt_attribute_t *)attribute length:(size_t)len {
    float px, py;
    int i;

    for (i = 0; i < len; i++) {
        int color256 = attribute->background_color;
	uint32_t colorValue;
	if ((x + i == cursorX && y == cursorY) || attribute->reverse) {
	    colorValue = foregroundColor;
        } else if (color256 == MVT_DEFAULT_COLOR) {
	    colorValue = backgroundColor;
	} else {
	    colorValue = mvt_color_value(color256);
	}
	NSColor *color = mvt_color_value_to_cocoa(colorValue);
	[color setFill];
	int px = (x + i) * fontWidth;
	int py = (height - 1 - y) * fontHeight;
	NSRectFill(NSMakeRect(px, py, fontWidth, fontHeight));
    }
#if 0
    NSString *str = [[NSString alloc] initWithCharacters:(const unichar *)ws length:sizeof (mvt_char_t) * len];
    NSDictionary *dict = [NSDictionary dictionaryWithObjectsAndKeys:
					   font, NSFontAttributeName,
				       [NSColor whiteColor], NSForegroundColorAttributeName, nil];
    px = x * fontWidth;
    py = (height - 1 - y) * fontHeight - fontBaseline;
    [str drawAtPoint:NSMakePoint(px, py) withAttributes:dict];
    [str release];
#else
    CGContextRef contextRef = [[NSGraphicsContext currentContext] graphicsPort];
    px = x * fontWidth;
    py = (height - 1 - y) * fontHeight - fontBaseline;
    CGContextSetTextMatrix(contextRef, CGAffineTransformIdentity);
    CGContextSetTextDrawingMode(contextRef, kCGTextFill);
#if 0
    CGContextSetRGBFillColor(contextRef, 1, 1, 1, 1);
    CGContextSelectFont(contextRef, "Menlo-Regular", 11, kCGEncodingMacRoman);
    char *s = malloc(len);
    for (i = 0; i < len; i++) {
	if (ws[i] < 0x20)
	    s[i] = 0x20;
	else
	    s[i] = ws[i];
    }
    CGContextShowTextAtPoint(contextRef, px, py, s, len);
    free(s);
#else
    CGFontRef font = CGFontCreateWithFontName((CFStringRef)@"HiraKakuProN-W3");
    CGContextSetFont(contextRef, font);
    CGContextSetFontSize(contextRef, 11.0 * 75 / 72);
    unichar *s = malloc(sizeof (unichar) * len);
    for (i = 0; i < len; i++) {
	if (ws[i] < 0x20)
	    s[i] = 0x20;
	else
	    s[i] = ws[i];
    }
    CGGlyph *glyphs = malloc(sizeof (CGGlyph) * len);
    CGFontGetGlyphsForUnichars(font, s, glyphs, len);
    for (i = 0; i < len; i++) {
        mvt_color_t color = attribute->foreground_color;
        int color_value = color == MVT_DEFAULT_COLOR ?
            0xffffff : mvt_color_value(color);
	CGContextSetRGBFillColor(contextRef,
				 (double)((color_value >> 16) & 255) / 255.0,
				 (double)((color_value >> 8) & 255) / 255.0,
				 (double)(color_value & 255) / 255.0, 1.0);
	CGContextShowGlyphsAtPoint(contextRef, px, py, &glyphs[i], 1);
	px += fontWidth;
    }
    free(s);
    free(glyphs);
#endif
#endif
}

- (void) clearRectWithX1:(int)x1 y1:(int)y1 x2:(int)x2 y2:(int)y2 backgroundColor:(mvt_color_t)background_color
{
    mvt_color_t color256 = background_color;
    uint32_t colorValue;
    assert(x1 >= 0 && x2 >= x1 && width > x2);
    assert(y1 >= 0 && y2 >= y2 && height > y2);
    if (color256 == MVT_DEFAULT_COLOR) {
	colorValue = backgroundColor;
    } else {
	colorValue = mvt_color_value(color256);
    }
    NSColor *color = mvt_color_value_to_cocoa(colorValue);
    [color setFill];
    float px = (x1) * fontWidth;
    float py = (height - 1 - y2) * fontHeight;
    float pwidth = (x2 - x1 + 1) * fontWidth;
    float pheight = (y2 - y1 + 1) * fontHeight;
    NSRectFill(NSMakeRect(px, py, pwidth, pheight));
}

- (void) moveCursorWithCursor:(mvt_cursor_t)cursor x:(int)x y:(int)y {
    switch (cursor) {
    case MVT_CURSOR_CURRENT:
        cursorX = x;
        cursorY = y;
        break;
    case MVT_CURSOR_SELECTION_START:
        selectionStartX = x;
        selectionStartY = y;
        break;
    case MVT_CURSOR_SELECTION_END:
        selectionEndX = x;
        selectionEndY = y;
        break;
    }
}

- (void) getSizeWithWidth:(int*)pwidth height:(int*)pheight {
    *pwidth = width;
    *pheight = height;
}

- (void) setAttribute:(const char *)value forName:(const char *)name {
    if (strcmp(name, "width") == 0)
        width = atoi(value);
    else if (strcmp(name, "height") == 0)
        height = atoi(value);
    else if (strcmp(name, "font-size") == 0) {
        fontSize = atoi(value);
    } else if (strcmp(name, "foreground-color") == 0)
        foregroundColor = mvt_atocolor(value);
    else if (strcmp(name, "background-color") == 0)
        backgroundColor = mvt_atocolor(value);
}

- (void) keyDown:(NSEvent *)theEvent {
    NSString *str = [theEvent characters];
    int code = [str characterAtIndex:0];
    mvt_screen_dispatch_keydown((mvt_screen_t *)screen, FALSE, code);
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)becomeFirstResponder
{
    return YES;
}

- (BOOL)resignFirstResponder
{
    return YES;
}

- (void) mouseDown:(NSEvent*)theEvent {
    printf("mouse\n");
}

- (void) scrollFrom:(int)y1 to:(int)y2 count:(int)count
{
    if (y1 == -1)
        y1 = 0;
    if (y2 == -1)
        y2 = height - 1;

    assert(y1 >= 0 && y2 >= 0);
    assert(count != 0 && count <= y2 - y1 + 1 && count >= -(y2 - y1 + 1));

    [self scrollRect:NSMakeRect(0, (height - y2 - 1) * fontHeight, fontWidth * width, (y2 - y1 + 1) * fontHeight) by:NSMakeSize(0, -count * fontHeight)];
    [self lockFocus];
    NSColor *color = mvt_color_value_to_cocoa(backgroundColor);
    [color setFill];
    if (count > 0) {
	NSRectFill(NSMakeRect(0, (height - y1 - 1) * fontHeight, fontWidth * width, count * fontHeight));
    } else {
	NSRectFill(NSMakeRect(0, (height - y2 - 1) * fontHeight, fontWidth * width, -count * fontHeight));
    }
    [self unlockFocus];
}
@end

@implementation MVTEnvironment
- (void) handleRequest:(id)aObj {
    mvt_handle_request();
}
@end

static int mvt_cocoa_init(int *argc, char ***argv, mvt_event_func_t event_func)
{
    myPool = [[NSAutoreleasePool alloc] init];
    globalEnvironment = [[MVTEnvironment alloc] init];
    myApp = [NSApplication sharedApplication];
    mvt_worker_init(event_func);
    return 0;
}

static void mvt_cocoa_main(void)
{
    [myApp run];
    globalEnvironment = nil;
}

void mvt_cocoa_main_quit(void)
{
    loop = FALSE;
}

void mvt_cocoa_exit(void)
{
    mvt_worker_exit();
    [globalEnvironment release];
    [myPool release];
    myPool = nil;
}

static const mvt_driver_vt_t mvt_cocoa_driver_vt = {
    mvt_cocoa_init,
    mvt_cocoa_main,
    mvt_cocoa_main_quit,
    mvt_cocoa_exit,
    mvt_cocoa_open_screen,
    mvt_cocoa_close_screen,
    mvt_worker_open_terminal,
    mvt_worker_close_terminal,
    mvt_cocoa_set_screen_attribute,
    mvt_worker_set_terminal_attribute,
    mvt_worker_connect,
    mvt_worker_suspend,
    mvt_worker_resume,
    mvt_worker_shutdown
};

static const mvt_driver_t mvt_cocoa_driver = {
    &mvt_cocoa_driver_vt, "cocoa"
};

const mvt_driver_t *mvt_get_driver(void)
{
    return &mvt_cocoa_driver;
}

static NSColor *mvt_color_value_to_cocoa(uint32_t color_value)
{
    float r = ((float)((color_value >> 16) & 0xff)) / 255;
    float g = ((float)((color_value >> 8) & 0xff)) / 255;
    float b = ((float)(color_value & 0xff)) / 255;
    return [NSColor colorWithDeviceRed:r green:g blue:b alpha:1.0];
}

void mvt_notify_request(void)
{
    [globalEnvironment performSelectorOnMainThread:@selector(handleRequest:)
					withObject:nil
				     waitUntilDone:NO];
}
