#ifndef PUBSUB_SIGNALS_H_
#define PUBSUB_SIGNALS_H_

#include "fault_manager.h"
#include "qpc.h"
#include <stddef.h>

enum PubSubSignals
{
    PUBSUB_FIRST_SIG = Q_USER_SIG,
    PUBSUB_FAULT_GENERATED_SIG,
    PUBSUB_PRESSURE_SIG,
    PUBSUB_TEMPERATURE_SIG,
    PUBSUB_MOTOR_DATA_SIG,
    PUBSUB_TACH_SIG,
    PUBSUB_ADD_DATA_TO_PLOT_SIG,
    PUBSUB_DRAW_PLOT_SIG,
    PUBSUB_MAX_SIG
};

typedef struct
{
    QEvt super;
    float num; // size of the buffer 'instructions', which should be >= actual size
} FloatEvent_T;

typedef struct
{
    QEvt super;
    int16_t num; // size of the buffer 'instructions', which should be >= actual size
} Int16Event_T;

typedef struct
{
    QEvt super;

    Fault_ID_T id;
    Fault_Type_T type;
    uint16_t code;
    char msg[FAULT_GEN_EVENT_MAX_MSG_LENGTH];
} FaultGeneratedEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    uint32_t milliseconds;
    char data_label[16];
    float32_t data_point;
} AddDataToPlotEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    char data_label[16];
    float32_t *data_points;
    uint16_t data_len;
} DrawPlotEvent_T;

#endif // PUBSUB_SIGNALS_H_
