#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

// Forward declarations
typedef struct UIControl UIControl;

/**
 * State Management Module
 * Tracks control states and detects changes
 */

#define MAX_CONTROLS 256

void state_init(int* control_states);
int state_check_changes(UIControl* controls, int max_controls, int* last_states);

#endif // STATE_MANAGER_H
