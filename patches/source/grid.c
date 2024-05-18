#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <gctypes.h>
#include "reloc.h"

#include "grid.h"

// draw variables
const int xfbHeight = 448;

// ===============================================================================

#define MAX_ANIMS 100000
#define START_LINE 10
#define ANIM_DIRECTION_UP 0
#define ANIM_DIRECTION_DOWN 1

bool grid_setup_done = false;
line_backing_t browser_lines[128];

// ===============================================================================

// other vars
extern int selected_slot;
extern s32 top_line_num;

const int raw_y_top = 64;
const int base_y = 118;
const f32 offset_y = 56;

static f32 get_position_after(line_backing_t *line_backing) {
    anim_list_t *anims = &line_backing->anims;

    f32 position_y = line_backing->raw_position_y;
    if (anims->direction == ANIM_DIRECTION_UP) {
        return position_y - anims->remaining;
    } else {
        return position_y + anims->remaining;
    }
}

// other stuff
void grid_setup_func() {
    OSReport("browser_lines = %p\n", browser_lines);
    OSReport("number_of_lines = %d\n", number_of_lines);

    // test code
    for (int line_num = 0; line_num < number_of_lines; line_num++) {
        line_backing_t *line_backing = &browser_lines[line_num];
        // line_backing->relative_index = line_num;
 
        int row = line_num;
        f32 raw_pos_y = (row * offset_y);
        line_backing->raw_position_y = DRAW_BOUND_TOP - (offset_y * START_LINE) + raw_pos_y;
        line_backing->transparency = 1.0;
        if (line_num < START_LINE || line_num >= START_LINE + DRAW_TOTAL_ROWS) {
            line_backing->transparency = 0.0;
        }

        selected_slot = START_LINE * 8;
        top_line_num = START_LINE;

        anim_list_t *anims = &line_backing->anims;
        anims->pending_count = 0;
        anims->remaining = 0;
        // OSReport("Setting line position %d = %f\n", line_num, line_backing->raw_position_y);
    }

    grid_setup_done = true;
    return;
}

void grid_add_anim(int line_num, int direction, f32 distance) {
    line_backing_t *line_backing = &browser_lines[line_num];
    anim_list_t *anims = &line_backing->anims;

    if (anims->pending_count > 0 && anims->direction != direction) {
        anims->pending_count = 0;
        OSReport("ERROR: Clearing anims\n");
    }

    if (anims->pending_count < MAX_ANIMS) {
        anims->pending_count++;
        anims->remaining += distance;
        anims->direction = direction;
    } else {
        OSReport("ERROR: Max anims reached\n");
    }
}


int grid_dispatch_navigate_up() {
    OSReport("Up pressed %d\n", number_of_lines);

    // check all anims full or empty
    // calculate final pos after all pending anims would complete
    // if final pos is within bounds, add anims

    // int count_visible_after = 0;
    // int max_pending = 0;
    // for (int line_num = 0; line_num < number_of_lines; line_num++) {
    //     line_backing_t *line_backing = &browser_lines[line_num];
    //     anim_list_t *anims = &line_backing->anims;
    //     if (anims->pending_count > max_pending) {
    //         max_pending = anims->pending_count;
    //     }

    //     // no idea why this works
    //     if (get_position_after(line_backing) + 10 >= DRAW_BOUND_TOP && get_position_after(line_backing) - 10 < DRAW_BOUND_BOTTOM) {
    //         count_visible_after++;
    //     }
    // }

    // if (max_pending == MAX_ANIMS) {
    //     OSReport("ERROR: Max anims exceeded\n");
    //     return 1;
    // }

    // if (count_visible_after <= 4) {
    //     OSReport("ERROR: All visible (count_visible_after=%d)\n", count_visible_after);
    //     return 1;
    // }

    bool found_move_in = false;
    bool found_move_out = false;
    for (int line_num = 0; line_num < number_of_lines; line_num++) {
        line_backing_t *line_backing = &browser_lines[line_num];
        anim_list_t *anims = &line_backing->anims;

        f32 partial_position_y = get_position_after(line_backing);
        if (!found_move_in && partial_position_y + offset_y + 10 >= DRAW_BOUND_TOP && partial_position_y - 10 < DRAW_BOUND_TOP) {
            // OSReport("ERROR: End position is out of bounds (%d)\n", line_num);
            f32 anim_distance = offset_y * 0.5;
            line_backing->raw_position_y = DRAW_BOUND_TOP - anim_distance;
            line_backing->transparency = 0.25;
            line_backing->moving_in = true;
            line_backing->moving_out = false;

            found_move_in = true;
            OSReport("Moving in %d\n", line_num);
            grid_add_anim(line_num, ANIM_DIRECTION_DOWN, anim_distance);
        } else if (partial_position_y < (DRAW_BOUND_BOTTOM + offset_y - 10) && partial_position_y >= (DRAW_BOUND_TOP - offset_y + 10)) {
            grid_add_anim(line_num, ANIM_DIRECTION_DOWN, offset_y);
            // OSReport("Adding anim %d, current=%f sub=%f\n", line_num, position_y, offset_y);
        } else {
            line_backing->raw_position_y += offset_y;
        }

        f32 end_position_y = get_position_after(line_backing);
        if (!found_move_out && end_position_y + 10 >= DRAW_BOUND_BOTTOM && end_position_y - offset_y < DRAW_BOUND_BOTTOM) {
            line_backing->transparency = 0.999;
            line_backing->moving_out = true;
            line_backing->moving_in = false;

            found_move_out = true;
            OSReport("Moving out %d\n", line_num);
            // OSReport("Current position = %f\n", position_y);
            // OSReport("End position = %f\n", end_position_y);
        }
    }

    return 0;
}

int grid_dispatch_navigate_down() {
    OSReport("Down pressed %d\n", number_of_lines);

    // check all anims full or empty
    // calculate final pos after all pending anims would complete
    // if final pos is within bounds, add anims

    // int count_visible_after = 0;
    // int max_pending = 0;
    // for (int line_num = 0; line_num < number_of_lines; line_num++) {
    //     line_backing_t *line_backing = &browser_lines[line_num];
    //     anim_list_t *anims = &line_backing->anims;
    //     if (anims->pending_count > max_pending) {
    //         max_pending = anims->pending_count;
    //     }

    //     // no idea why this works
    //     if (get_position_after(line_backing) + 10 >= DRAW_BOUND_TOP && get_position_after(line_backing) - 10 < DRAW_BOUND_BOTTOM) {
    //         count_visible_after++;
    //     }
    // }

    // if (max_pending == MAX_ANIMS) {
    //     OSReport("ERROR: Max anims exceeded\n");
    //     return 1;
    // }

    // if (count_visible_after <= 4) {
    //     OSReport("ERROR: All visible (count_visible_after=%d)\n", count_visible_after);
    //     return 1;
    // }

    bool found_move_in = false;
    bool found_move_out = false;
    for (int line_num = 0; line_num < number_of_lines; line_num++) {
        line_backing_t *line_backing = &browser_lines[line_num];
        anim_list_t *anims = &line_backing->anims;

        f32 partial_position_y = get_position_after(line_backing);
        if (!found_move_in && partial_position_y + 10 >= DRAW_BOUND_BOTTOM && partial_position_y - offset_y - 10 < DRAW_BOUND_BOTTOM) {
            // OSReport("ERROR: End position is out of bounds (%d)\n", line_num);
            f32 anim_distance = offset_y * 0.5;
            line_backing->raw_position_y = DRAW_BOUND_BOTTOM - anim_distance;
            line_backing->transparency = 0.25;
            line_backing->moving_in = true;
            line_backing->moving_out = false;

            found_move_in = true;
            OSReport("Moving in %d\n", line_num);
            grid_add_anim(line_num, ANIM_DIRECTION_UP, anim_distance);
        } else if (partial_position_y < (DRAW_BOUND_BOTTOM + offset_y - 10) && partial_position_y >= (DRAW_BOUND_TOP - offset_y + 10)) {
            grid_add_anim(line_num, ANIM_DIRECTION_UP, offset_y);
            // OSReport("Adding anim %d, current=%f sub=%f\n", line_num, position_y, offset_y);
        } else {
            if (line_num == 0) {
                OSReport("Current position = %f\n", line_backing->raw_position_y);
                OSReport("Target position = %f\n", partial_position_y);
            }
            line_backing->raw_position_y -= offset_y;
        }

        f32 end_position_y = get_position_after(line_backing);
        if (!found_move_out && end_position_y + offset_y + 10 >= DRAW_BOUND_TOP && end_position_y < DRAW_BOUND_TOP) {
            line_backing->transparency = 0.999;
            line_backing->moving_out = true;
            line_backing->moving_in = false;

            found_move_out = true;
            OSReport("Moving out %d\n", line_num);
            // OSReport("Current position = %f\n", position_y);
            // OSReport("End position = %f\n", end_position_y);
        }
    }

    return 0;
}

void grid_update_icon_positions() {
    // OSReport("UPDATE\n");
    if (!grid_setup_done) {
        return;
    }

    // only currently showing or "about to be shown"
    // cells get anim entries added

    // other cells get moved by one entire place

    int count_partially_visibile = 0;
    int count_visible = 0;
    for (int line_num = 0; line_num < number_of_lines; line_num++) {
        line_backing_t *line_backing = &browser_lines[line_num];
        // anim_list_t *anims = &line_backing->anims;
        if (line_backing->raw_position_y - offset_y >= DRAW_BOUND_TOP && line_backing->raw_position_y + offset_y < DRAW_BOUND_BOTTOM) {
            count_visible++;
            if (line_backing->transparency < 1.0) {
                count_partially_visibile++;
            }
        }

        // remove boxes that are out of bounds
        if (line_backing->raw_position_y <= DRAW_BOUND_TOP - (offset_y * 0.85)  || line_backing->raw_position_y >= DRAW_BOUND_BOTTOM + (offset_y * 0.85)) {
            line_backing->transparency = 0.0;
        }
    }

    // OSReport("Partially Visible %d\n", count_partially_visibile);

    for (int line_num = 0; line_num < number_of_lines; line_num++) {
        line_backing_t *line_backing = &browser_lines[line_num];
        anim_list_t *anims = &line_backing->anims;

        f32 multiplier = 3 * 2;
        // if (anims->pending_count) {
        //     multiplier += anims->pending_count;
        //     if (multiplier > 8) multiplier = 8;
        // }

        // transparency
        if (line_backing->transparency < 1.0) {
            f32 delta = 0.01 * multiplier;
            if (line_backing->moving_in) {
                line_backing->transparency += (delta * 0.4);
                if (line_backing->transparency > 1.0) {
                    line_backing->transparency = 1.0;
                }
            }

            if (line_backing->moving_out) {
                line_backing->transparency -= (delta * 0.8);
                if (line_backing->transparency < 0.0) {
                    line_backing->transparency = 0.0;
                    line_backing->moving_out = false;
                }
            }
        }

        if (anims->pending_count) {
            // position
            // if (line_backing->moving_in && count_visible < 4 && line_backing->transparency < 1.0) {
            //     multiplier /= 2;
            // }

            // if (count_visible - count_partially_visibile > 4) {
            //     multiplier *= 12;
            // } else if (count_visible > 6) {
            //     multiplier *= count_visible;
            // }
            
            f32 delta = offset_y * 0.01 * multiplier / 1.5;
            if (delta > anims->remaining) {
                delta = anims->remaining;
            }

            anims->remaining -= delta;
            if (anims->direction == ANIM_DIRECTION_UP) {
                line_backing->raw_position_y -= delta;
            } else {
                line_backing->raw_position_y += delta;
            }

            if (anims->remaining < 0.1) {
                // OSReport("Final pos = %f\n", line_backing->raw_position_y);
                anims->pending_count--;
                anims->remaining = 0;
            }
        }

        // OSReport("Update %d\n ", line_backing->relative_index);
    }
}
