/* Automatically generated nanopb header */
/* Generated by nanopb-0.4.9-dev */

#ifndef PB_CLEARPLOTS_PB_H_INCLUDED
#define PB_CLEARPLOTS_PB_H_INCLUDED
#include <pb.h>

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Struct definitions */
/* Indicates to Python to wipe all plots clear */
typedef struct _ClearPlots {
    char dummy_field;
} ClearPlots;


#ifdef __cplusplus
extern "C" {
#endif

/* Initializer values for message structs */
#define ClearPlots_init_default                  {0}
#define ClearPlots_init_zero                     {0}

/* Field tags (for use in manual encoding/decoding) */

/* Struct field encoding specification for nanopb */
#define ClearPlots_FIELDLIST(X, a) \

#define ClearPlots_CALLBACK NULL
#define ClearPlots_DEFAULT NULL

extern const pb_msgdesc_t ClearPlots_msg;

/* Defines for backwards compatibility with code written before nanopb-0.4.0 */
#define ClearPlots_fields &ClearPlots_msg

/* Maximum encoded size of messages (where known) */
#define CLEARPLOTS_PB_H_MAX_SIZE                 ClearPlots_size
#define ClearPlots_size                          0

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
