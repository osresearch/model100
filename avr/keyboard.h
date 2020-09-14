/** \file
 * Model 100 keyboard matrix.
 */
#ifndef _model100_keyboard_h_
#define _model100_keyboard_h_

#include <avr/io.h>
#include <stdint.h>


/** Scan the row/col matrix for any keys held down.
 *
 * This will return the lowest key; multiple keys are not currently
 * supported.
 *
 * \return ASCII code for key, or 0 for no key.
 */
extern uint8_t
keyboard_scan(void);


#endif
