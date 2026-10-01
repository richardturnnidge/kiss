/*
 * KISS, on AED's editing libraries (aed-libs 1.4.1, from AED's GitHub releases).
 *
 * The editor is AED's: libedui and libedcore do the text, the screen and the
 * editing keys (ED_KEYS). This file adds what makes it KISS -- CTRL+R saves the
 * file and runs it -- and the few keys KISS has always had on top: save, save
 * as, quit, and CTRL+C for the colour picker, with CTRL+E for the settings.
 */
#include <stdint.h>
#include <stdio.h>

#include "app.h"
#include "cmd_ops.h"
#include "config.h"
#include "editor.h"
#include "keys.h"
#include "settings.h"
#include "user_input.h"
#include "runtime.h"    // needs stdio.h and stdint.h first

// Where KISS keeps its files. It shares AED's grammars, themes and fonts, and
// keeps its settings -- tab width, colours, font -- in a file of its own.
static const app_context KISS_APP = {
    .name       = "kiss",
    .cfg_path   = CFG_DIR "/kiss.ini",
    .cfg_old    = NULL,
    .syntax_dir = "/config/aed/syntax",
    .theme_dir  = "/config/aed/themes",
    .font_dir   = "/config/aed",
};

// The editor's settings, in a file that says it is KISS's. The library writes
// one on the first run, holding the colours KISS started in.
static int kiss_render(const void* values, char* buf, int max) {
    return ed_settings_render(values, "KISS", "/config/aed", buf, max);
}

static const cfg_schema KISS_CONFIG = {
    ED_SETTINGS, ED_SETTINGS_COUNT, kiss_render,
};

// Read at startup, before anything is drawn.
static const char* kiss_settings(editor* ed) {
    return ed_settings_apply(ed, &KISS_CONFIG);
}

// CTRL+E: the settings screen, as in AED. Whatever is changed there is
// written back to /config/kiss.ini.
static void kiss_cmd_settings(editor* ed) {
    ed_cmd_settings(ed, &KISS_CONFIG);
}

// CTRL+R: save, run the program, then put the editor back.
static void kiss_run(editor* ed) {
    ed_cmd_save(ed);
    const char* fname = tb_fname(&ed->doc_->buf_);
    if (fname == NULL || fname[0] == 0) {
        return;                 // nothing saved, so nothing to run
    }
    runcode((char*) fname);

    // The editor queues every key pressed, and it went on queueing them while
    // the program ran: the arrows of a game, the answers to a quiz, the ESC
    // that stopped it. They belong to the program, so they are thrown away
    // here rather than typed into the file.
    key_press kp;
    while (keys_poll(&kp)) {
    }

    // The program ends by setting the screen mode back, which resets the
    // colours, the font and the cursor settings the editor had made.
    // ed_resume sends them again and repaints, where a plain repaint drew a
    // two-tone title bar and black borders.
    ed_resume(ed);
}

// CTRL+C: the colour picker, as KISS has always had it.
static void kiss_colours(editor* ed) {
    if (ui_color_picker(&ed->ui_, &ed->scr_) == YES_OPT) {
        ed_pick_syntax(ed);     // the theme follows the background
        cmd_restore_after_modal(ed, false);
    }
}

// KISS's keys, in front of AED's editing keys. A key here wins over ED_KEYS,
// which is how CTRL+C stays the colour picker rather than becoming copy.
static const key_binding KISS_BINDINGS[] = {
    { VK_r, MOD_CTRL,           0, kiss_run },
    { VK_R, MOD_CTRL,           0, kiss_run },
    { VK_c, MOD_CTRL,           0, kiss_colours },
    { VK_C, MOD_CTRL,           0, kiss_colours },
    { VK_s, MOD_CTRL | MOD_ALT, 0, cmd_save_as },
    { VK_S, MOD_CTRL | MOD_ALT, 0, cmd_save_as },
    { VK_s, MOD_CTRL,           0, ed_cmd_save },
    { VK_S, MOD_CTRL,           0, ed_cmd_save },
    { VK_q, MOD_CTRL,           0, ed_cmd_quit },
    { VK_Q, MOD_CTRL,           0, ed_cmd_quit },
    { VK_e, MOD_CTRL,           0, kiss_cmd_settings },
    { VK_E, MOD_CTRL,           0, kiss_cmd_settings },
};
static const keymap KISS_KEYS = {
    KISS_BINDINGS, (int) (sizeof(KISS_BINDINGS) / sizeof(KISS_BINDINGS[0])),
    &ED_KEYS,
};

static const ed_program KISS = {
    &KISS_APP, &KISS_KEYS, kiss_settings, NULL, " KISS ",
};

int main(int argc, char** argv) {
    static editor ed;
    const char* fname = argc > 1 ? argv[1] : NULL;
    if (ed_init_for(&ed, TB_DOC_KB, fname, &KISS) == NULL) {
        return 1;
    }
    ed_run(&ed);
    ed_destroy(&ed);

    return 0;
}
