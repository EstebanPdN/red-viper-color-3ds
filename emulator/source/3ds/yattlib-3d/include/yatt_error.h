#ifndef YATT_ERROR_H
#define YATT_ERROR_H

/**
 * Error handling standard across the YattLib-3D library.
 * Broadly speaking, 0 indicates success.
 */

// Generic return codes
enum {
   YATT_OK               = 0, // Success
   YATT_INPUT_MALFORMED,      // Input is malformed and cannot be used. Usually correctable by the programmer.
   YATT_EXTERNAL_FAILURE,     // Failed due to external factors (malloc, etc), and generally a runtime error.
};

#endif
