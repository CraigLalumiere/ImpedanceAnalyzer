#ifndef POSTED_SIGNALS_H_
#define POSTED_SIGNALS_H_

#include "pubsub_signals.h"
#include "qp.h"

// These are signals that are directly posted from one Active Object to another
//  and do not traverse the Pub Sub framework
enum PostedSignals
{
    POSTED_FIRST_SIG = PUBSUB_MAX_SIG,
    POSTED_BLINKY_TOGGLE_USER_LED,
    POSTED_APP_CLI_PRINT_SIG,
    POSTED_PC_COM_PRINT_SIG,
    POSTED_PC_COM_CLI_DATA_SIG,
    POSTED_FORCE_FAULT_SIG,
    POSTED_WAVEFORM_CAPTURE_COMPLETE_SIG,
    POSTED_MAX_SIG
};

// These are signals that are directly dispatched from an Active Object to a component
enum DispatchedSignals
{
    DISPATCHED_FIRST_SIG = POSTED_MAX_SIG,
    DISPATCHED_MAX_SIG
};

/**************************************************************************************************\
* Debug/misc events
\**************************************************************************************************/

typedef struct
{
    QEvt super;
    uint32_t desiredFault;
} DebugForceFaultEvent_T;

#endif // POSTED_SIGNALS_H_
