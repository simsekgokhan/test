/*******************************************************************************
*   Ledger Blue
*   (c) 2016 Ledger
*
*  Licensed under the Apache License, Version 2.0 (the "License");
*  you may not use this file except in compliance with the License.
*  You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
*  Unless required by applicable law or agreed to in writing, software
*  distributed under the License is distributed on an "AS IS" BASIS,
*  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*  See the License for the specific language governing permissions and
*  limitations under the License.
********************************************************************************/

#include "os.h"
#include "cx.h"
#include "os_io_seproxyhal.h"

#include <stdbool.h>

unsigned char G_io_seproxyhal_spi_buffer[IO_SEPROXYHAL_BUFFER_SIZE_B];

static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e);
static const bagl_element_t *io_seproxyhal_touch_right(const bagl_element_t *e);
static const bagl_element_t *io_seproxyhal_touch_left(const bagl_element_t *e);
static void ui_idle(void);
//static void setText(const char* top, const char* mid, const char* bot);

static char address[100];
static char top[100];
static char mid[100];
static char bot[100];

static unsigned int path[5];
ux_state_t ux;

static const char NOT_AVAILABLE[] = "NOT_AVAILABLE";

//// Text Examples
// static const char aa2[] = "Welcome. You are in aa2"; // 23
// static const char bb1[] = "Welcome. bb1"; // 12
// static const char cc1[] = "This is the bottom ccc1-";

static bool game_started;
static bool game_lost;
static bool game_won;
static int game_score;

// Welcome
static const char welcomeTop[] = "Welcome to ";
static const char welcomeMid[] = "\"True or False\"";
static const char welcomeBot[] = "Game";

// Lost
static const char lostTop[] = "Incorrect answer :(";
static const char lostMid[] = "Your Score:";

// won
static const char wonTop[] = "Congrats. You won :)";
static const char wonMid[] = "Your Score:";

static const char quest[][30] = {
    "We see the Sun", "where it was", "8 mins, 20 secs ago",
    "The capital of", "Australia", "is Sydney",
    "Antibiotics kill", "viruses", "as well as bacteria.",     
    "Belarus is the", "silicon valley", "of Eastern Europe",
    "World's highest waterfall", "is Angel Falls","in Venezuela"
};

static bool answers[] = { true, false, false, true, true };
static int random_array[5];

// ********************************************************************************
// Ledger Blue specific UI
// ********************************************************************************

#ifdef TARGET_BLUE

static const bagl_element_t const bagl_ui_sample_blue[] = {};

static unsigned int
bagl_ui_sample_blue_button(unsigned int button_mask,
                           unsigned int button_mask_counter) {
    return 0;
}

#endif

// ********************************************************************************
// Ledger Nano S specific UI
// ********************************************************************************

#ifdef TARGET_NANOS

static const bagl_element_t bagl_ui_sample_nanos[] = {
    // {
    //     {type, userid, x, y, width, height, stroke, radius, fill, fgcolor,
    //      bgcolor, font_id, icon_id},
    //     text,
    //     touch_area_brim,
    //     overfgcolor,
    //     overbgcolor,
    //     tap,
    //     out,
    //     over,
    // },
    {
        {BAGL_RECTANGLE, 0x00, 0, 0, 128, 32, 0, 0, BAGL_FILL, 0x000000,
         0xFFFFFF, 0, 0},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_LABELINE, 0x02, 0, 10, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        top,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {   // type, 	  userid,  x,  y, width, height, stroke, radius, fill, fgcolor
        {BAGL_LABELINE, 0x02, 23, 20, 82, 11, 0x80 | 10, 0, 0, 0xFFFFFF,
         0x000000, BAGL_FONT_OPEN_SANS_REGULAR_11px |
         BAGL_FONT_ALIGNMENT_CENTER, 26},
        mid,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {   // type, 	  userid, x,  y, width, height, stroke, radius, fill, fgcolor
        {BAGL_LABELINE, 0x02, 0, 30, 128, 11, 0, 0, 0, 0xFFFFFF, 0x000000,
         BAGL_FONT_OPEN_SANS_REGULAR_11px | BAGL_FONT_ALIGNMENT_CENTER, 0},
        bot,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },    
    {
        {BAGL_ICON, 0x00, 3, 12, 7, 7, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CROSS},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
    {
        {BAGL_ICON, 0x00, 117, 13, 8, 6, 0, 0, 0, 0xFFFFFF, 0x000000, 0,
         BAGL_GLYPH_ICON_CHECK},
        NULL,
        0,
        0,
        0,
        NULL,
        NULL,
        NULL,
    },
};

static unsigned int
bagl_ui_sample_nanos_button(unsigned int button_mask,
                            unsigned int button_mask_counter) {
    switch (button_mask) {
    case BUTTON_EVT_RELEASED | BUTTON_LEFT:
        io_seproxyhal_touch_left(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_RIGHT:
        io_seproxyhal_touch_right(NULL);
        break;

    case BUTTON_EVT_RELEASED | BUTTON_LEFT | BUTTON_RIGHT: // EXIT
        io_seproxyhal_touch_exit(NULL);
        break;
    }
    return 0;
}

#endif

static const bagl_element_t *io_seproxyhal_touch_exit(const bagl_element_t *e) {
    // Go back to the dashboard

    // todo: move to score
    // os_memmove(top, scoreTop, sizeof(scoreTop));    
    // os_memmove(mid, scoreMid, sizeof(scoreMid));    
    // os_memmove(bot, scoreBot, sizeof(scoreBot)); 
    os_sched_exit(0);
    return NULL;
}

static const bagl_element_t *io_seproxyhal_touch_right(const bagl_element_t *e) {   
    if(game_lost || game_won) {
        os_sched_exit(0);        
        return NULL;
    }    

    if(sizeof(answers) == path[4]/3) {
        game_won = true;
        const char score[10] = {game_score + 1 + 48};        
        os_memmove(top, wonTop, sizeof(wonTop));    
        os_memmove(mid, wonMid, sizeof(wonMid));  
        os_memmove(bot, score, sizeof(score));                 
        return NULL;
    }

    if(!game_started){
        game_started = true;
        const int index = random_array[0]*3;
        os_memmove(top, quest[index], sizeof(quest[index]));    
        os_memmove(mid, quest[index+1], sizeof(quest[index+1]));  
        os_memmove(bot, quest[index+2], sizeof(quest[index+2])); 
        path[4] += 3;        
        return NULL;
    }

    const int level = path[4]/3;    
    const int index = random_array[level]*3;
    if(answers[random_array[level-1]]) { // correct answer
        os_memmove(top, quest[index], sizeof(quest[index]));    
        os_memmove(mid, quest[index+1], sizeof(quest[index+1]));  
        os_memmove(bot, quest[index+2], sizeof(quest[index+2])); 
        path[4] += 3;
        ++game_score;
    }
    else {
        game_lost = true;        
        const char score[10] = {game_score + 48};
        os_memmove(top, lostTop, sizeof(lostTop));    
        os_memmove(mid, lostMid, sizeof(lostMid));    
        os_memmove(bot, score, sizeof(score)); 
    }

    ui_idle();
    return NULL;
}

static const bagl_element_t *io_seproxyhal_touch_left(const bagl_element_t *e) {
    if(game_lost || game_won) {
        os_sched_exit(0);        
        return NULL;
    }    

    if(sizeof(answers) == path[4]/3) {
        game_won = true;
        const char score[10] = {game_score + 1 + 48};        
        os_memmove(top, wonTop, sizeof(wonTop));    
        os_memmove(mid, wonMid, sizeof(wonMid));  
        os_memmove(bot, score, sizeof(score));                 
        return NULL;
    }

    if(!game_started){
        game_started = true;
        const int index = random_array[0]*3;
        os_memmove(top, quest[index], sizeof(quest[index]));    
        os_memmove(mid, quest[index+1], sizeof(quest[index+1]));  
        os_memmove(bot, quest[index+2], sizeof(quest[index+2])); 
        path[4] += 3;        
        return NULL;
    }

    const int level = path[4]/3;    
    const int index = random_array[level]*3;
    if(!answers[random_array[level-1]]) { // correct answer
        os_memmove(top, quest[index], sizeof(quest[index]));    
        os_memmove(mid, quest[index+1], sizeof(quest[index+1]));  
        os_memmove(bot, quest[index+2], sizeof(quest[index+2])); 
        path[4] += 3;
        ++game_score;
    }
    else {
        game_lost = true;
        const char score[10] = {game_score + 48};
        os_memmove(top, lostTop, sizeof(lostTop));    
        os_memmove(mid, lostMid, sizeof(lostMid));    
        os_memmove(bot, score, sizeof(score));        
    }
         
    ui_idle();    
    return NULL;
}

unsigned short io_exchange_al(unsigned char channel, unsigned short tx_len) {
    switch (channel & ~(IO_FLAGS)) {
    case CHANNEL_KEYBOARD:
        break;

    // multiplexed io exchange over a SPI channel and TLV encapsulated protocol
    case CHANNEL_SPI:
        if (tx_len) {
            io_seproxyhal_spi_send(G_io_apdu_buffer, tx_len);

            if (channel & IO_RESET_AFTER_REPLIED) {
                reset();
            }
            return 0; // nothing received from the master so far (it's a tx
                      // transaction)
        } else {
            return io_seproxyhal_spi_recv(G_io_apdu_buffer,
                                          sizeof(G_io_apdu_buffer), 0);
        }

    default:
        THROW(INVALID_PARAMETER);
    }
    return 0;
}

static void ui_idle(void) {
#ifdef TARGET_BLUE
    UX_DISPLAY(bagl_ui_sample_blue, NULL);
#else
    UX_DISPLAY(bagl_ui_sample_nanos, NULL);
#endif
}

// todo: sizeoof will not work with char*
// static void setText(const char* _top, const char* _mid, const char* _bot) {
//     os_memmove(top, _top, sizeof(_top));    
//     os_memmove(mid, _mid, sizeof(_mid));    
//     os_memmove(bot, _bot, sizeof(_bot));
// }

static void sample_main(void) {
    volatile unsigned int rx = 0;
    volatile unsigned int tx = 0;
    volatile unsigned int flags = 0;

    // DESIGN NOTE: the bootloader ignores the way APDU are fetched. The only
    // goal is to retrieve APDU.
    // When APDU are to be fetched from multiple IOs, like NFC+USB+BLE, make
    // sure the io_event is called with a
    // switch event, before the apdu is replied to the bootloader. This avoid
    // APDU injection faults.
    for (;;) {
        volatile unsigned short sw = 0;

        BEGIN_TRY {
            TRY {
                rx = tx;
                tx = 0; // ensure no race in catch_other if io_exchange throws
                        // an error
                rx = io_exchange(CHANNEL_APDU | flags, rx);
                flags = 0;

                // no apdu received, well, reset the session, and reset the
                // bootloader configuration
                if (rx == 0) {
                    THROW(0x6982);
                }

                if (G_io_apdu_buffer[0] != 0x80) {
                    THROW(0x6E00);
                }

                // unauthenticated instruction
                switch (G_io_apdu_buffer[1]) {
                case 0x00: // reset
                    flags |= IO_RESET_AFTER_REPLIED;
                    THROW(0x9000);
                    break;

                case 0x01: // case 1
                    THROW(0x9000);
                    break;

                case 0x02: // echo
                    tx = rx;
                    THROW(0x9000);
                    break;

                case 0xFF: // return to dashboard
                    goto return_to_dashboard;

                default:
                    THROW(0x6D00);
                    break;
                }
            }
            CATCH_OTHER(e) {
                switch (e & 0xF000) {
                case 0x6000:
                case 0x9000:
                    sw = e;
                    break;
                default:
                    sw = 0x6800 | (e & 0x7FF);
                    break;
                }
                // Unexpected exception => report
                G_io_apdu_buffer[tx] = sw >> 8;
                G_io_apdu_buffer[tx + 1] = sw;
                tx += 2;
            }
            FINALLY {
            }
        }
        END_TRY;
    }

return_to_dashboard:
    return;
}

void io_seproxyhal_display(const bagl_element_t *element) {
    io_seproxyhal_display_default((bagl_element_t *)element);
}

unsigned char io_event(unsigned char channel) {
    // nothing done with the event, throw an error on the transport layer if
    // needed

    // can't have more than one tag in the reply, not supported yet.
    switch (G_io_seproxyhal_spi_buffer[0]) {
    case SEPROXYHAL_TAG_FINGER_EVENT:
        UX_FINGER_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_BUTTON_PUSH_EVENT: // for Nano S
        UX_BUTTON_PUSH_EVENT(G_io_seproxyhal_spi_buffer);
        break;

    case SEPROXYHAL_TAG_DISPLAY_PROCESSED_EVENT:
        if (UX_DISPLAYED()) {
            // TODO perform actions after all screen elements have been
            // displayed
        } else {
            UX_DISPLAYED_EVENT();
        }
        break;

    case SEPROXYHAL_TAG_TICKER_EVENT:
        UX_REDISPLAY();
        break;

    // unknown events are acknowledged
    default:
        break;
    }

    // close the event if not done previously (by a display or whatever)
    if (!io_seproxyhal_spi_is_status_sent()) {
        io_seproxyhal_general_status();
    }

    // command has been processed, DO NOT reset the current APDU transport
    return 1;
}

static const char BASE58ALPHABET[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    'G', 'H', 'J', 'K', 'L', 'M', 'N', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
    'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'm',
    'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z'};

static unsigned int encode_base58(const void *in, unsigned int length,
                                  char *out, unsigned int maxoutlen) {
    char tmp[164];
    char buffer[164];
    unsigned char j;
    unsigned char startAt;
    unsigned char zeroCount = 0;
    if (length > sizeof(tmp)) {
        THROW(INVALID_PARAMETER);
    }
    os_memmove(tmp, in, length);
    while ((zeroCount < length) && (tmp[zeroCount] == 0)) {
        ++zeroCount;
    }
    j = 2 * length;
    startAt = zeroCount;
    while (startAt < length) {
        unsigned short remainder = 0;
        unsigned char divLoop;
        for (divLoop = startAt; divLoop < length; divLoop++) {
            unsigned short digit256 = (unsigned short)(tmp[divLoop] & 0xff);
            unsigned short tmpDiv = remainder * 256 + digit256;
            tmp[divLoop] = (unsigned char)(tmpDiv / 58);
            remainder = (tmpDiv % 58);
        }
        if (tmp[startAt] == 0) {
            ++startAt;
        }
        buffer[--j] = BASE58ALPHABET[remainder];
    }
    while ((j < (2 * length)) && (buffer[j] == BASE58ALPHABET[0])) {
        ++j;
    }
    while (zeroCount-- > 0) {
        buffer[--j] = BASE58ALPHABET[0];
    }
    length = 2 * length - j;
    if (maxoutlen < length) {
        THROW(EXCEPTION_OVERFLOW);
    }
    os_memmove(out, (buffer + j), length);
    return length;
}

__attribute__((section(".boot"))) int main(void) {
    // exit critical section
    __asm volatile("cpsie i");

    // ensure exception will work as planned
    os_boot();

    UX_INIT();

    BEGIN_TRY {
        TRY {
            io_seproxyhal_init();

            // Invalidate the current authentication to demonstrate
            // reauthentication
            // Reauthenticate with "Auth" (Blue) or left button (Nano S)
            //os_global_pin_invalidate();

#ifdef LISTEN_BLE
            if (os_seph_features() &
                SEPROXYHAL_TAG_SESSION_START_EVENT_FEATURE_BLE) {
                BLE_power(0, NULL);
                // restart IOs
                BLE_power(1, NULL);
            }
#endif

            USB_power(0);
            USB_power(1);

            path[0] = 44 | 0x80000000;
            path[1] = 0 | 0x80000000;
            path[2] = 0 | 0x80000000;
            path[3] = 0;
            path[4] = 0;

            // Init random numbers array
            random_array[0] = -1;
            random_array[1] = -1;
            random_array[2] = -1;
            random_array[3] = -1;
            random_array[4] = -1;
            const int qsize = sizeof(answers);
            for (int i = 0; i < qsize; i++) {
                while (-1 == random_array[i]) {
                    const int ran = (int)cx_rng_u8() % qsize;
                    bool unique = true;
                    for (int k = 0; k < qsize; k++) {
                        if (ran == random_array[k])
                            unique = false;
                    }
                    if (unique)
                        random_array[i] = ran;
                };
            }

            game_started = false;
            game_lost = false;
            game_won = false;
            game_score = 0;

            os_memmove(address, NOT_AVAILABLE, sizeof(NOT_AVAILABLE));
            os_memmove(top, welcomeTop, sizeof(welcomeTop));
            os_memmove(mid, welcomeMid, sizeof(welcomeMid));
            os_memmove(bot, welcomeBot, sizeof(welcomeBot));

            ui_idle();
            sample_main();
        }
        CATCH_OTHER(e) {
        }
        FINALLY {
        }
    }
    END_TRY;
}
