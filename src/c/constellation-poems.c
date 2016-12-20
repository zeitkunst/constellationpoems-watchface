// To convert from OTF to TTF:
//  fontforge -script scripts/otf2ttf.sh FONTNAME.otf 
//
// Working with more than 3-byte unicode glyphs:
// https://forums.pebble.com/t/how-can-i-filter-3-byte-unicode-characters-glyphs-in-ttf/26024
//
// TODO
// * Figure out how to properly calculate the size of the scroll window for a given font size, have to take into account descenders, need to ensure that we scroll by an integral amount of maximum height + descenders
// * I like Fell English at 24, but need to recalculate the sizes accordingly
// * Try different fonts, sans-serif fonts too, also for the time line below 
// * Try text for time too
// * Draw "constallations" (new ones), while we're putting up random words
// * Consider IM Fell Flowers for flourishes in the poems
// * Need to look at fonts not in Windows font library (Mrs eaves, .fonts dir, etc.)
// * Create state machine with case/switch to easily move through how I want to do this

#include <pebble.h>
#define NUM_WORD_LAYERS 12
#define NUM_STARS 40
#define NUM_CONSTELLATION_STARS 16 // maximum number of stars in the constellation
#define NUM_TOTAL_WORDS sizeof(words)/sizeof(*words)

// State machine for stars and constellation poetry
typedef enum
{
    STATE_START = 0,
    STATE_TITLE,
    STATE_BLANK_1,
    STATE_WORDS,
    STATE_BLANK_2
} stars_state_t;

uint8_t state_periods[] = {
    2,
    3,
    2,
    NUM_WORD_LAYERS,
    2
};

stars_state_t stars_state = STATE_START;
static uint8_t current_period = 0;

// TODO
// Look through star chart book for appropriate words
static const char *words[] = {
    "azure",
    "indigo",
    "crimson",
    "dust",
    "fragment",
    "Luna",
    "rock",
    "void",
    "nova",
    "vast",
    "incessant",
    "continuous",
    "infinite",
    "night",
    "light",
    "point",
    "otherness",
    "sleep",
    "wake",
    "awe",
    "wave",
    "companion",
    "double",
    "brilliant",
    "nucleus",
    "dense",
    "visible",
    "obscured",
    "patch",
    "disk",
    "visible",
    "ominous",
    "spectrum",
    "gas",
};

static const char *prefixes[] = {
    "Bor",
    "Cen",
    "Ib",
    "Op",
    "Xe",
    "Ab",
    "Dec",
    "Hi",
    "Pur",
    "Neb",
    "Reg",
    "Zur",
    "Sex"
};

static const char *postfixes[] = {
    "tion",
    "able",
    "ser",
    "furg",
    "quest",
    "zeru",
    "yack",
    "kulp",
    "fed",
    "der"
};


static Window *s_main_window;

static Layer *window_layer;

static Layer *s_stars_layer;

static uint8_t margin = 4;

static TextLayer *s_time_layer;
static TextLayer *s_title_layer;

static TextLayer *word_layers[NUM_WORD_LAYERS];
static uint8_t currentWordLayer = 0;
static TextLayer *createWordLayer(void);
static int word_indices[NUM_WORD_LAYERS + 1];
static char is_word_used[NUM_TOTAL_WORDS] = { 0 };

static GFont s_title_font;
static GFont s_time_font;
static GFont s_word_font;

static GPoint stars[NUM_STARS];
static GPoint constellation_stars[NUM_CONSTELLATION_STARS];
static uint8_t constellation_lines[NUM_CONSTELLATION_STARS][2];
static uint8_t num_constellation_stars_chosen;

static GRect bounds;

static uint8_t wordPeriod = 1; // Add new word every wordPeriod seconds

static void generate_title_layer(char *title) {
    uint8_t text_height = 20 + 8 + 20 + 20;
    s_title_layer = text_layer_create(
            GRect(margin, PBL_IF_ROUND_ELSE(84 - (text_height/2), 84 - (text_height/2)), bounds.size.w - (2 * margin), text_height));

    s_title_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ADOBE_JENSON_20));

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(s_title_layer, GColorClear);
    text_layer_set_text_color(s_title_layer, GColorWhite);
    text_layer_set_text(s_title_layer, title);
    text_layer_set_font(s_title_layer, s_title_font);
    text_layer_set_text_alignment(s_title_layer, GTextAlignmentCenter);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_layer, text_layer_get_layer(s_title_layer));
}


static void destroy_title_layer(void) {
    text_layer_destroy(s_title_layer);

    // Unload GFont
    fonts_unload_custom_font(s_title_font);

}

/*
 * To generate costellations:
 * Choose number of stars in constellation
 * Decide if we're going to loop back to the beginning with the drawing
 */

static void generate_random_constellation(void) {
    num_constellation_stars_chosen = (rand() % (NUM_CONSTELLATION_STARS - 4)) + 4;

    GPoint start, star;
    star.x = rand() % (bounds.size.w/2) + (bounds.size.w/4);
    star.y = rand() % (bounds.size.h/2) + (bounds.size.h/4);
    constellation_stars[0] = GPoint(star.x, star.y);

    for (int i = 1; i < num_constellation_stars_chosen; i++) {
        star = GPoint(constellation_stars[i - 1].x, constellation_stars[i - 1].y);
        int x_offset = rand() % 25;
        int y_offset = rand() % 25;

        uint8_t dir = rand() % 2;
        if (dir == 1) {
            x_offset = -1 * x_offset;
        }

        dir = rand() % 2;
        if (dir == 1) {
            y_offset = -1 * y_offset;
        }

        star.x = star.x + x_offset;
        star.y = star.y + y_offset;

        constellation_stars[i] = GPoint(star.x, star.y);

        APP_LOG(APP_LOG_LEVEL_INFO, "offsets: %d, %d", x_offset, y_offset);
        APP_LOG(APP_LOG_LEVEL_INFO, "constellation stars: %d, %d", star.x, star.y);
    }

}

static void stars_update_proc(Layer *layer, GContext *ctx) {
    graphics_context_set_fill_color(ctx, GColorBlack);

    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_context_set_stroke_color(ctx, GColorWhite);

    GRect rect_bounds = GRect(0, 0, 2, 2);

    for (int i = 0; i < NUM_STARS; i++) {
        rect_bounds.origin = stars[i];
        graphics_fill_rect(ctx, rect_bounds, 0, GCornerNone);
    }

    for (int i = 0; i < NUM_CONSTELLATION_STARS; i++) {
        rect_bounds.origin = constellation_stars[i];
        graphics_fill_rect(ctx, rect_bounds, 0, GCornerNone);
    }

    for (int i = 1; i < num_constellation_stars_chosen; i++) {
        graphics_draw_line(ctx, constellation_stars[i - 1], constellation_stars[i]);
    }


    APP_LOG(APP_LOG_LEVEL_INFO, "drawing stars");
}

static void generate_random_stars(void) {
    GPoint origin;

    for (int i = 0; i < NUM_STARS; i++) {
        // Draw a rectangle
        origin.x = rand() % bounds.size.w;
        origin.y = rand() % bounds.size.h;
        stars[i] = origin;
    }
   
}

// From: http://stackoverflow.com/questions/6127503/shuffle-array-in-c
static void shuffle(int *array, size_t n)
{
    if (n > 1) 
    {
        size_t i;
        for (i = 0; i < n - 1; i++) 
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

static void generate_random_word_list(void) {
    int max_words = (int)NUM_TOTAL_WORDS;
    int max_layers = (int)NUM_WORD_LAYERS;

    int in, im;

    im = 0;

    for (in = 0; (in < max_words) && (im < max_layers); ++in) {
        int rn = max_words - in;
        int rm = max_layers - im;

        if (rand() % rn < rm) {
            word_indices[im++] = in + 1;
            APP_LOG(APP_LOG_LEVEL_INFO, "index chosen: %d", in+1);
        }
    }

    shuffle(word_indices, NUM_WORD_LAYERS);

    /*
    for (in = NUM_TOTAL_WORDS - NUM_WORD_LAYERS; (in < NUM_TOTAL_WORDS) && (im < NUM_WORD_LAYERS); ++in) {
        int r = rand() % (in + 1);

        if (is_word_used[r])
            //we already have r
            r = in;

        word_indices[im++] = r + 1;
        APP_LOG(APP_LOG_LEVEL_INFO, "index chosen: %d", r+1);
        is_word_used[r] = 1;
    }
    */
}

static void update_time() {
    // Get a tm structure
    time_t temp = time(NULL);
    struct tm *tick_time = localtime(&temp);

    // Write the current hours and minutes into a buffer
    static char s_buffer[8];
    strftime(s_buffer, sizeof(s_buffer), clock_is_24h_style() ? "%H:%M" : "%I:%M", tick_time);

    // Display this time on the TextLayer
    text_layer_set_text(s_time_layer, s_buffer);
}

static void tick_handler_seconds(struct tm *tick_time, TimeUnits units_changed) {
    // Add new word every wordPeriod seconds
    if (tick_time->tm_sec % wordPeriod == 0) {
        APP_LOG(APP_LOG_LEVEL_INFO, "current_state: %d", stars_state);
        switch(stars_state) {
            case STATE_START:
                if (current_period < state_periods[STATE_START]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    generate_title_layer("CONSTELLATION KEYWORDS");
                    stars_state = STATE_TITLE;
                }
                break;
            case STATE_TITLE:

                if (current_period < state_periods[STATE_TITLE]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    destroy_title_layer();
                    stars_state = STATE_BLANK_1;
                }
                break;
            case STATE_BLANK_1:
                if (current_period < state_periods[STATE_BLANK_1]) {
                    current_period += 1;
                } else {
                    current_period = 0;
                    stars_state = STATE_WORDS;
                }

                break;
            case STATE_WORDS:
                if (current_period < state_periods[STATE_WORDS]) {
                    current_period += 1;
                    TextLayer *new_word_layer = createWordLayer();
                    layer_add_child(window_layer, text_layer_get_layer(new_word_layer));
                } else {
                    // Destroy word layers
                    for (int i = 0; i < NUM_WORD_LAYERS; i++) {
                        text_layer_destroy(word_layers[i]);
                    }

                    current_period = 0;
                    currentWordLayer = 0;
                    stars_state = STATE_BLANK_2;
                }
                break;
            case STATE_BLANK_2:
                if (current_period < state_periods[STATE_BLANK_2]) {
                    current_period += 1;
                } else {
                    generate_random_stars();
                    generate_random_constellation();
                    generate_random_word_list();
                    layer_mark_dirty(s_stars_layer);

                    current_period = 0;
                    stars_state = STATE_START;
                }
                break;
    
        }
    }
}
/*
static void tick_handler_seconds(struct tm *tick_time, TimeUnits units_changed) {
    int absOffset;

    // Add new word every wordPeriod seconds
    if (tick_time->tm_sec % wordPeriod == 0) {
        if (currentWordLayer < NUM_WORD_LAYERS) {
            TextLayer *new_word_layer = createWordLayer();
            layer_add_child(window_layer, text_layer_get_layer(new_word_layer));
        } else if (currentWordLayer == (NUM_WORD_LAYERS)) {
            // Destroy word layers
            for (int i = 0; i < NUM_WORD_LAYERS; i++) {
                text_layer_destroy(word_layers[i]);
            }

            currentWordLayer += 1;
        } else if (currentWordLayer > NUM_WORD_LAYERS) {
            if (currentWordLayer <= (NUM_WORD_LAYERS + 2)) {
                APP_LOG(APP_LOG_LEVEL_INFO, "adding 1 to word layer");
                currentWordLayer += 1;
            } else {
                currentWordLayer = 0;
                generate_random_stars();
                generate_random_constellation();
                generate_random_word_list();
            }
        } 

    } 

    // Update time every minute    
    if (tick_time->tm_sec % 60 == 0) {
        update_time();
        APP_LOG(APP_LOG_LEVEL_INFO, "updating time");
    }


}
*/


static TextLayer *createWordLayer(void) {
    TextLayer *word_layer;
    
    uint8_t randX, randY;

    randX = rand() % ((uint8_t)(bounds.size.w/2));
    randY = rand() % ((uint8_t)bounds.size.h - 40);


    word_layer = text_layer_create(GRect(randX - 2*margin, PBL_IF_ROUND_ELSE(randY - 2*margin, randY - 2*margin), bounds.size.w - (margin * 2), 140));

    // Style word layer text
    text_layer_set_background_color(word_layer, GColorClear);
    text_layer_set_text_color(word_layer, GColorWhite);
    //text_layer_set_text_alignment(word_layer, GTextAlignmentCenter);
    text_layer_set_overflow_mode(word_layer, GTextOverflowModeWordWrap);

    // For selecting random words
    // TODO: make code such that only unique words are chosen from list
    // Probably need some sort of algorithm like this:
    // http://stackoverflow.com/questions/1608181/unique-random-numbers-in-an-integer-array-in-the-c-programming-language
    //int index = rand() % (sizeof(words)/sizeof(*words));
    APP_LOG(APP_LOG_LEVEL_INFO, "currentWordLayer: %d", currentWordLayer);
    int index = word_indices[currentWordLayer];
    APP_LOG(APP_LOG_LEVEL_INFO, "selected word %d", index);
    text_layer_set_text(word_layer, words[index]);

    text_layer_set_font(word_layer, s_word_font);

    word_layers[currentWordLayer] = word_layer;
    currentWordLayer += 1;
    APP_LOG(APP_LOG_LEVEL_INFO, "Adding text %s at %d, %d: currentLayer: %d", words[index], randX, randY, currentWordLayer);

    return word_layer; 
}

static void main_window_load(Window *window) {

    // Get information about the window
    window_layer = window_get_root_layer(window);
    bounds = layer_get_frame(window_layer);
    GRect max_text_bounds = GRect(0, 0, bounds.size.w - (margin * 2), 2000);

    s_stars_layer = layer_create(bounds);
    layer_set_update_proc(s_stars_layer, stars_update_proc);
    generate_random_stars();
    generate_random_constellation();
    generate_random_word_list();

    //TextLayer *new_word_layer = createWordLayer();

    // Create time GFont
    // Maybe try: Corbel, Cordia new, Delicious, Franklin Gothic, Gadugi, Gill Sans, Kozuka Gothic, Myriad Pro, Segoe UI, Source Sans, Swiss 721, Yu Gothic UI
    s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_20));

    // Create word layer GFont
    s_word_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_ADOBE_JENSON_20));


    // Create the text layer with specific bounds
    s_time_layer = text_layer_create(
            GRect(margin, PBL_IF_ROUND_ELSE(144, 144), bounds.size.w - (2 * margin), 20));

    // Improve the layout to be more like a watchface
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text_color(s_time_layer, GColorWhite);
    text_layer_set_text(s_time_layer, "00:00");
    //text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
    text_layer_set_font(s_time_layer, s_time_font);
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);

    // Add it as a child layer to the Window's root layer
    layer_add_child(window_layer, text_layer_get_layer(s_time_layer));
    layer_add_child(window_layer, s_stars_layer);

    // TODO: Not sure why this part causes problems...
    //layer_add_child(window_layer, text_layer_get_layer(new_word_layer));
    //APP_LOG(APP_LOG_LEVEL_INFO, "Afer adding new_word_layer");
}

static void main_window_unload(Window *window) {
    // Destroy stars layer
    layer_destroy(s_stars_layer);

    // Destory TextLayer
    text_layer_destroy(s_time_layer);

    // Destroy word layers
    for (int i = 0; i < NUM_WORD_LAYERS; i++) {
        text_layer_destroy(word_layers[i]);
    }


    // Unload GFont
    fonts_unload_custom_font(s_time_font);

    // Unload word layer font
    fonts_unload_custom_font(s_word_font);

}

static void init() {
    s_main_window = window_create();

    window_set_window_handlers(s_main_window, (WindowHandlers) {
        .load = main_window_load,
        .unload = main_window_unload
    });    

    window_set_background_color(s_main_window, GColorBlack);

    // Only one subscription to this service is allowed
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler_seconds);

    window_stack_push(s_main_window, true);

    // Make sure the time is displayed from the start
    update_time();
}

static void deinit() {
    // Destroy window
    window_destroy(s_main_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
