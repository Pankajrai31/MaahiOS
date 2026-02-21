#ifndef HIT_TEST_H
#define HIT_TEST_H

// Forward declarations
typedef struct UIControl UIControl;

/**
 * Hit Testing Module
 * Determines which control is under the mouse cursor
 */

int hit_test_control(int mx, int my, UIControl* controls, int max_controls);

#endif // HIT_TEST_H
