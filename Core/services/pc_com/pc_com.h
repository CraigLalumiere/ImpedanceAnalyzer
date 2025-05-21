#ifndef PC_COM_AO_H_
#define PC_COM_AO_H_

#include "interfaces/serial_interface.h"
#include "qpc.h"
#include "stddef.h"

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************\
* Public macros
\**************************************************************************************************/

#define PC_COM_EVENT_MAX_MSG_LENGTH 64
#define CLI_DATA_MAX_LENGTH         64

/**************************************************************************************************\
* Public memory declarations
\**************************************************************************************************/
extern QActive *const AO_PC_COM; // opaque pointer

/**************************************************************************************************\
* Public type definitions
\**************************************************************************************************/
typedef struct
{
    QEvt super;

    uint32_t milliseconds;
    char msg[PC_COM_EVENT_MAX_MSG_LENGTH];
} PCCOMPrintEvent_T;

typedef struct
{
    QEvt super;

    char msg[CLI_DATA_MAX_LENGTH];
    uint8_t msg_size;
} PCCOMCliDataEvent_T;

//
// Debug/misc events
//
typedef struct
{
    QEvt super;
    uint32_t desiredFault;
} PCCOMForceFaultEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    char plot_title[64];
    char x_label[64];
    char x_units[8];
    char y_label[64];
    char y_units[8];
} ConfigPlotEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    char data_label[16];
    uint32_t milliseconds;
    float data_x;
    float data_y;
    float data_z;
} AddDataToPlotEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    char data_label[16];
    uint32_t *data_x;
    float *data_y;
    uint16_t data_len;
} DrawPlotEvent_T;

typedef struct
{
    QEvt super;
    uint8_t plot_number;
    char data_label[16];
    float *data_freq;
    float *data_mag;
    float *data_phase;
    uint16_t data_len;
} DrawBodePlotEvent_T;

/**************************************************************************************************\
* Public prototypes
\**************************************************************************************************/
void PC_COM_ctor(const Serial_IO_T *const serial_io_interface);
void PC_COM_print(const char *msg);
void PC_COM_clear_plots();
void PC_COM_config_plot(
    uint8_t plot_number,
    const char *plot_title,
    const char *x_label,
    const char *x_units,
    const char *y_label,
    const char *y_units);
// append a X,Y coordinate to a plot
void PC_COM_add_datapoint_to_plot(uint8_t plot_number, const char *data_label, float x, float y);
// append a freq,mag,phase coordinate to a bode plot
void PC_COM_add_datapoint_to_bode_plot(
    uint8_t plot_number, const char *data_label, float freq, float mag, float phase);
// append a millisecond time-stamped datapoint to plot
void PC_COM_log_data_to_plot(uint8_t plot_number, const char *data_label, float data_point);
// draw (or redraw) an entire plot all at once
void PC_COM_draw_plot(
    uint8_t plot_number, const char *data_label, uint32_t *data_x, float *data_y, int16_t data_len);
// draw (or redraw) an entire bode plot (magnitude + phase) on log-log axes
void PC_COM_draw_bode_plot(
    uint8_t plot_number,
    const char *data_label,
    float *data_freq,
    float *data_mag,
    float *data_phase,
    int16_t data_len);

#ifdef __cplusplus
}
#endif
#endif // PC_COM_AO_H_