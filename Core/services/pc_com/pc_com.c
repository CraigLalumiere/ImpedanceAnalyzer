#include "pc_com.h"
#include "bsp.h"
#include "c/AddToBodePlot.pb.h"
#include "c/AddXYToPlot.pb.h"
#include "c/CLIData.pb.h"
#include "c/ClearPlots.pb.h"
#include "c/ConfigPlot.pb.h"
#include "c/DrawBodePlot.pb.h"
#include "c/DrawPlot.pb.h"
#include "c/LogPrint.pb.h"
#include "c/LogToPlot.pb.h"
#include "c/MessageType.pb.h"
#include "cli_commands.h"
#include "crc16.h"
#include "hdlc.h"
#include "pb_decode.h"
#include "pb_encode.h"
#include "private_signal_ranges.h"
#include "pubsub_signals.h"
#include "reset.h"
#include "safe_strncpy.h"
#include "stdio.h"
#include <string.h>

#define EMBEDDED_CLI_IMPL
#include "embedded_cli.h"

Q_DEFINE_THIS_MODULE("pc_com")

/**************************************************************************************************\
* Private macros
\**************************************************************************************************/
#define CLI_BUFFER_SIZE 1024

/**************************************************************************************************\
* Private type definitions
\**************************************************************************************************/
enum PC_COM_Signals
{
    SERIAL_DATA_AVAILABLE_SIG = PRIVATE_SIGNAL_PC_COM_START,
    CLI_PROCESS_TICK_SIG,
    CLEAR_PLOTS_SIG,
    CONFIG_PLOT_SIG,
    LOG_TO_PLOT_SIG,
    ADD_XY_TO_PLOT_SIG,
    ADD_TO_BODE_PLOT_SIG,
    DRAW_PLOT_SIG,
    DRAW_BODE_PLOT_SIG,
    TEST_SIG
};

typedef uint16_t Packet_CRC_T;
typedef uint8_t Packet_Type_T;

// encoded PB messages are variable length, so this union ensures allocation to get the largest
// encoded message
typedef union
{
    uint8_t LogPrint_max[LOGPRINT_PB_H_MAX_SIZE];
    uint8_t CLIData_max[CLIDATA_PB_H_MAX_SIZE];
    uint8_t LogToPlot_max[LOGTOPLOT_PB_H_MAX_SIZE];
    uint8_t AddXYToPlot_max[ADDXYTOPLOT_PB_H_MAX_SIZE];
    uint8_t AddToBodePlot_max[ADDTOBODEPLOT_PB_H_MAX_SIZE];
    uint8_t DrawPlot_max[DrawPlot_size];
    uint8_t DrawBodePlot_max[DRAWBODEPLOT_PB_H_MAX_SIZE];
    uint8_t ClearPlots_max[CLEARPLOTS_PB_H_MAX_SIZE];
    uint8_t ConfigPlot_max[CONFIGPLOT_PB_H_MAX_SIZE];
} TX_Message_Buffer_T;

typedef union
{
    uint8_t LogPrint_max[LOGPRINT_PB_H_MAX_SIZE];
    uint8_t CLIData_max[CLIDATA_PB_H_MAX_SIZE];
} RX_Message_Buffer_T;

typedef union
{
    CLIData CLI_data;
} RX_Message_Decoded_T;

typedef struct
{
    Packet_CRC_T crc;
    Packet_Type_T type;
    TX_Message_Buffer_T message;
} __attribute__((packed, aligned(1))) PC_COM_TX_Packet_T;

typedef struct
{
    Packet_CRC_T crc;
    Packet_Type_T type;
    RX_Message_Buffer_T message;
} __attribute__((packed, aligned(1))) PC_COM_RX_Packet_T;

typedef struct
{
    QActive super; // inherit QActive
    const Serial_IO_T *serial_io_interface;

    PC_COM_TX_Packet_T tx_packet;
    PC_COM_RX_Packet_T rx_packet;

    HDLC_Unpacker_T hdlc_unpacker;
    RX_Message_Decoded_T rx_message_decoded;

    EmbeddedCli *embedded_cli;
    CLI_UINT cliBuffer[BYTES_TO_CLI_UINTS(CLI_BUFFER_SIZE)];

    QTimeEvt testEvt;
    QTimeEvt cli_process_tick_evt;
} PC_COM;

/**************************************************************************************************\
* Private memory declarations
\**************************************************************************************************/
static PC_COM pc_com_inst;
QActive *const AO_PC_COM = &pc_com_inst.super;

PCCOMCliDataEvent_T *cli_data_event;

/**************************************************************************************************\
* Private prototypes`
\**************************************************************************************************/

static QState initial(PC_COM *const me, void const *const par);
static QState active(PC_COM *const me, QEvt const *const e);

static void calculate_crc_and_send_packet(PC_COM *const me, size_t message_len);
static void Serial_Data_Ready(void *cb_data);
static void parse_and_handle_pc_packet(PC_COM *const me);
static void handle_cli_char_received(PC_COM *const me);

static void DoForcedFault(PC_COM *const me, const PCCOMForceFaultEvent_T *pEvt);

static void cli_write_char(EmbeddedCli *embeddedCli, char c);

/**************************************************************************************************\
* Public functions
\**************************************************************************************************/

/**
 ***************************************************************************************************
 * @brief   Constructor
 **************************************************************************************************/
void PC_COM_ctor(const Serial_IO_T *const serial_io_interface)
{
    PC_COM *const me = &pc_com_inst;

    pc_com_inst.serial_io_interface = serial_io_interface;

    // CLI config
    EmbeddedCliConfig *config = embeddedCliDefaultConfig();

    // use static allocated buffer for CLI storage
    config->cliBuffer       = me->cliBuffer;
    config->cliBufferSize   = CLI_BUFFER_SIZE;
    config->maxBindingCount = 30;

    // create an instance of CLI
    me->embedded_cli = embeddedCliNew(config);

    // store char function
    me->embedded_cli->writeChar = cli_write_char;

    // add commands from command table
    CLI_AddCommands(me->embedded_cli);

    // initialize the HDLC frame unpacker by pointing it to the receive buffer
    hdlc_unpacker_init(
        &pc_com_inst.hdlc_unpacker,
        (uint8_t *) (&pc_com_inst.rx_packet),
        sizeof(PC_COM_RX_Packet_T));

    // init cli data buffer
    cli_data_event           = Q_NEW(PCCOMCliDataEvent_T, POSTED_PC_COM_CLI_DATA_SIG);
    cli_data_event->msg_size = 0;
    memset(cli_data_event->msg, 0, CLI_DATA_MAX_LENGTH);

    QActive_ctor(&me->super, Q_STATE_CAST(&initial));

    QTimeEvt_ctorX(&me->cli_process_tick_evt, &me->super, CLI_PROCESS_TICK_SIG, 0U);

    // test
    // QTimeEvt_ctorX(&me->testEvt, &me->super, TEST_SIG, 0U);
}

/**
 ***************************************************************************************************
 *
 * @brief   Publish message to PC COM with string message
 *
 **************************************************************************************************/
void PC_COM_print(const char *msg)
{
    PCCOMPrintEvent_T *event = Q_NEW(PCCOMPrintEvent_T, POSTED_PC_COM_PRINT_SIG);

    event->milliseconds = BSP_Get_Milliseconds_Tick();
    safe_strncpy(event->msg, msg, sizeof(event->msg));

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Clear all plots
 *
 **************************************************************************************************/

void PC_COM_clear_plots()
{
    PC_COM *const me        = &pc_com_inst;
    static QEvt const event = QEVT_INITIALIZER(CLEAR_PLOTS_SIG);
    QACTIVE_POST(&me->super, &event, me);
}

/**
 ***************************************************************************************************
 *
 * @brief   Set plot title and axis labels
 *
 **************************************************************************************************/

void PC_COM_config_plot(
    uint8_t plot_number,
    const char *plot_title,
    const char *x_label,
    const char *x_units,
    const char *y_label,
    const char *y_units)
{
    ConfigPlotEvent_T *event = Q_NEW(ConfigPlotEvent_T, CONFIG_PLOT_SIG);
    event->plot_number       = plot_number;
    safe_strncpy(event->plot_title, plot_title, sizeof(event->plot_title));
    safe_strncpy(event->x_label, x_label, sizeof(event->x_label));
    safe_strncpy(event->x_units, x_units, sizeof(event->x_units));
    safe_strncpy(event->y_label, y_label, sizeof(event->y_label));
    safe_strncpy(event->y_units, y_units, sizeof(event->y_units));

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Append X/Y datapoint to a plot
 *
 **************************************************************************************************/

void PC_COM_add_datapoint_to_plot(uint8_t plot_number, const char *data_label, float x, float y)
{
    AddDataToPlotEvent_T *event = Q_NEW(AddDataToPlotEvent_T, ADD_XY_TO_PLOT_SIG);
    event->plot_number          = plot_number;
    safe_strncpy(event->data_label, data_label, sizeof(event->data_label));
    event->data_x = x;
    event->data_y = y;

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Append Freq,Mag,Phase datapoint to a bode plot
 *
 **************************************************************************************************/

void PC_COM_add_datapoint_to_bode_plot(
    uint8_t plot_number, const char *data_label, float freq, float mag, float phase)
{
    AddDataToPlotEvent_T *event = Q_NEW(AddDataToPlotEvent_T, ADD_TO_BODE_PLOT_SIG);
    event->plot_number          = plot_number;
    safe_strncpy(event->data_label, data_label, sizeof(event->data_label));
    event->data_x = freq;
    event->data_y = mag;
    event->data_z = phase;

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Add millisecond time-stamped datapoint to a plot
 *
 **************************************************************************************************/

void PC_COM_log_data_to_plot(uint8_t plot_number, const char *data_label, float y)
{
    AddDataToPlotEvent_T *event = Q_NEW(AddDataToPlotEvent_T, LOG_TO_PLOT_SIG);
    event->plot_number          = plot_number;
    safe_strncpy(event->data_label, data_label, sizeof(event->data_label));
    event->milliseconds = BSP_Get_Milliseconds_Tick();
    event->data_y       = y;

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Draw (or wipe and redraw) up to 512 datapoints to a plot
 *
 **************************************************************************************************/

void PC_COM_draw_plot(
    uint8_t plot_number, const char *data_label, uint32_t *data_x, float *data_y, int16_t data_len)
{
    DrawPlotEvent_T *event = Q_NEW(DrawPlotEvent_T, DRAW_PLOT_SIG);
    event->plot_number     = plot_number;
    safe_strncpy(event->data_label, data_label, sizeof(event->data_label));
    // To avoid having a massive QP message (and running out of RAM due to the massive pool), the AO
    // calling this function is responsible for keeping a static array of data and not screwing with
    // it until pc_com is done with it. Yes, this breaks many QP rules but whatever.
    event->data_x = data_x; // pass the pointer
    event->data_y = data_y; // pass the pointer
    if (data_len > 512)
    {
        event->data_len = 512;
        PC_COM_print("Data length is too long for PC_COM_draw_plot");
    }
    else
        event->data_len = data_len;

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**
 ***************************************************************************************************
 *
 * @brief   Draw (or wipe and redraw) up to 512 magnitude + phase datapoints to a log-log plot
 *
 **************************************************************************************************/
void PC_COM_draw_bode_plot(
    uint8_t plot_number,
    const char *data_label,
    float *data_freq,
    float *data_mag,
    float *data_phase,
    int16_t data_len)
{
    DrawBodePlotEvent_T *event = Q_NEW(DrawBodePlotEvent_T, DRAW_BODE_PLOT_SIG);
    event->plot_number         = plot_number;
    safe_strncpy(event->data_label, data_label, sizeof(event->data_label));
    // To avoid having a massive QP message (and running out of RAM due to the massive pool), the AO
    // calling this function is responsible for keeping a static array of data and not screwing with
    // it until pc_com is done with it. Yes, this breaks many QP rules but whatever.
    event->data_freq  = data_freq;  // pass the pointer
    event->data_mag   = data_mag;   // pass the pointer
    event->data_phase = data_phase; // pass the pointer
    event->data_len   = data_len;

    QACTIVE_POST(AO_PC_COM, (QEvt *) (event), AO_PC_COM);
}

/**************************************************************************************************\
* Private functions
\**************************************************************************************************/

/**
 ***************************************************************************************************
 *
 * @brief   HSM
 *
 **************************************************************************************************/
static QState initial(PC_COM *const me, void const *const par)
{
    Q_UNUSED_PAR(par);

    // Process CLI  every 25ms
    QTimeEvt_armX(
        &me->cli_process_tick_evt, MILLISECONDS_TO_TICKS(25U), MILLISECONDS_TO_TICKS(25U));

    // test
    // QTimeEvt_armX(&me->testEvt, MILLISECONDS_TO_TICKS(1000), MILLISECONDS_TO_TICKS(1000));

    // Register callback with the SerialIO interface to be called when new data is available
    me->serial_io_interface->register_cb_func(Serial_Data_Ready, me);
    return Q_TRAN(&active);
}

static QState active(PC_COM *const me, QEvt const *const e)
{
    QState status;

    switch (e->sig)
    {
        case Q_ENTRY_SIG: {
            status = Q_HANDLED();
            break;
        }

        case TEST_SIG: {
            PC_COM_print("Hello World!");
            status = Q_HANDLED();
            break;
        }

        case POSTED_FORCE_FAULT_SIG: {
            DoForcedFault(me, (const PCCOMForceFaultEvent_T *const) e);
            status = Q_HANDLED();
            break;
        }

        case SERIAL_DATA_AVAILABLE_SIG: {
            uint8_t rx_byte;
            HDLC_Unpack_State_T unpack_state;

            while (me->serial_io_interface->rx_func(&rx_byte, 1) == 1)
            {
                unpack_state = hdlc_unpacker_add_byte(&me->hdlc_unpacker, rx_byte);

                if (unpack_state == FRAME_UNPACK_COMPLETE)
                {
                    parse_and_handle_pc_packet(me);
                }
            }

            status = Q_HANDLED();
            break;
        }

        case CLI_PROCESS_TICK_SIG: {
            // check to see if there is CLI data in buffer to be sent
            if (cli_data_event->msg_size > 0)
            {
                QACTIVE_POST(AO_PC_COM, (QEvt *) (cli_data_event), AO_PC_COM);

                // init a new cli data event
                cli_data_event           = Q_NEW(PCCOMCliDataEvent_T, POSTED_PC_COM_CLI_DATA_SIG);
                cli_data_event->msg_size = 0;
                memset(cli_data_event->msg, 0, CLI_DATA_MAX_LENGTH);
            }

            embeddedCliProcess(me->embedded_cli);
            status = Q_HANDLED();
            break;
        }

        case POSTED_PC_COM_CLI_DATA_SIG: {
            // set message type
            me->tx_packet.type = MessageType_CLI_DATA;

            // create pb message
            CLIData message = CLIData_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            const PCCOMCliDataEvent_T *cli_data_evt = Q_EVT_CAST(PCCOMCliDataEvent_T);

            memcpy(message.msg.bytes, cli_data_evt->msg, cli_data_evt->msg_size);
            message.msg.size = cli_data_evt->msg_size;

            bool ok = pb_encode(&stream, CLIData_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case POSTED_PC_COM_PRINT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_LOG_PRINT;

            // create pb message
            LogPrint message = LogPrint_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            message.milliseconds_tick = Q_EVT_CAST(PCCOMPrintEvent_T)->milliseconds;
            safe_strncpy(message.msg, Q_EVT_CAST(PCCOMPrintEvent_T)->msg, sizeof(message.msg));

            bool ok = pb_encode(&stream, LogPrint_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case CLEAR_PLOTS_SIG: {
            // set message type
            me->tx_packet.type = MessageType_CLEAR_PLOTS;

            // create pb message
            ClearPlots message = ClearPlots_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            bool ok = pb_encode(&stream, ClearPlots_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case CONFIG_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_CONFIG_PLOT;

            // create pb message
            ConfigPlot message = ConfigPlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            message.plot_number = Q_EVT_CAST(ConfigPlotEvent_T)->plot_number;
            safe_strncpy(
                message.plot_title,
                Q_EVT_CAST(ConfigPlotEvent_T)->plot_title,
                sizeof(message.plot_title));
            safe_strncpy(
                message.x_label, Q_EVT_CAST(ConfigPlotEvent_T)->x_label, sizeof(message.x_label));
            safe_strncpy(
                message.y_label, Q_EVT_CAST(ConfigPlotEvent_T)->y_label, sizeof(message.y_label));
            safe_strncpy(
                message.x_units, Q_EVT_CAST(ConfigPlotEvent_T)->x_units, sizeof(message.x_units));
            safe_strncpy(
                message.y_units, Q_EVT_CAST(ConfigPlotEvent_T)->y_units, sizeof(message.y_units));

            bool ok = pb_encode(&stream, ConfigPlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case ADD_XY_TO_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_ADD_XY_TO_PLOT;

            // create pb message
            AddXYToPlot message = AddXYToPlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            safe_strncpy(
                message.data_label,
                Q_EVT_CAST(AddDataToPlotEvent_T)->data_label,
                sizeof(message.data_label));
            message.plot_number = Q_EVT_CAST(AddDataToPlotEvent_T)->plot_number;
            message.data_x      = Q_EVT_CAST(AddDataToPlotEvent_T)->data_x;
            message.data_y      = Q_EVT_CAST(AddDataToPlotEvent_T)->data_y;

            bool ok = pb_encode(&stream, AddXYToPlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case ADD_TO_BODE_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_ADD_TO_BODE_PLOT;

            // create pb message
            AddToBodePlot message = AddToBodePlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            safe_strncpy(
                message.data_label,
                Q_EVT_CAST(AddDataToPlotEvent_T)->data_label,
                sizeof(message.data_label));
            message.plot_number = Q_EVT_CAST(AddDataToPlotEvent_T)->plot_number;
            message.data_freq   = Q_EVT_CAST(AddDataToPlotEvent_T)->data_x;
            message.data_mag    = Q_EVT_CAST(AddDataToPlotEvent_T)->data_y;
            message.data_phase  = Q_EVT_CAST(AddDataToPlotEvent_T)->data_z;

            bool ok = pb_encode(&stream, AddToBodePlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case LOG_TO_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_LOG_TO_PLOT;

            // create pb message
            LogToPlot message = LogToPlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            safe_strncpy(
                message.data_label,
                Q_EVT_CAST(AddDataToPlotEvent_T)->data_label,
                sizeof(message.data_label));
            message.plot_number       = Q_EVT_CAST(AddDataToPlotEvent_T)->plot_number;
            message.milliseconds_tick = Q_EVT_CAST(AddDataToPlotEvent_T)->milliseconds;
            message.data_y            = Q_EVT_CAST(AddDataToPlotEvent_T)->data_y;

            bool ok = pb_encode(&stream, LogToPlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case DRAW_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_DRAW_PLOT;

            // create pb message
            DrawPlot message = DrawPlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            const DrawPlotEvent_T *event = Q_EVT_CAST(DrawPlotEvent_T);

            safe_strncpy(message.data_label, event->data_label, sizeof(message.data_label));
            message.plot_number = event->plot_number;
            Q_ASSERT(event->data_len * sizeof(event->data_y[0]) <= sizeof(message.data_y));
            memcpy(message.data_x, event->data_x, event->data_len * sizeof(event->data_x[0]));
            message.data_x_count = event->data_len;
            memcpy(message.data_y, event->data_y, event->data_len * sizeof(event->data_y[0]));
            message.data_y_count = event->data_len;

            bool ok = pb_encode(&stream, DrawPlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        case DRAW_BODE_PLOT_SIG: {
            // set message type
            me->tx_packet.type = MessageType_DRAW_BODE_PLOT;

            // create pb message
            DrawBodePlot message = DrawBodePlot_init_zero;

            // populate message and encode it
            pb_ostream_t stream = pb_ostream_from_buffer(
                ((uint8_t *) &me->tx_packet.message), sizeof(TX_Message_Buffer_T));

            const DrawBodePlotEvent_T *event = Q_EVT_CAST(DrawBodePlotEvent_T);

            safe_strncpy(message.data_label, event->data_label, sizeof(message.data_label));
            message.plot_number = event->plot_number;
            Q_ASSERT(event->data_len * sizeof(event->data_freq[0]) <= sizeof(message.data_freq));
            memcpy(
                message.data_freq, event->data_freq, event->data_len * sizeof(event->data_freq[0]));
            message.data_freq_count = event->data_len;
            memcpy(message.data_mag, event->data_mag, event->data_len * sizeof(event->data_mag[0]));
            message.data_mag_count = event->data_len;
            memcpy(
                message.data_phase,
                event->data_phase,
                event->data_len * sizeof(event->data_phase[0]));
            message.data_phase_count = event->data_len;

            bool ok = pb_encode(&stream, DrawBodePlot_fields, &message);
            Q_ASSERT(ok);

            // calculate CRC and transmit
            calculate_crc_and_send_packet(me, stream.bytes_written);
            status = Q_HANDLED();
            break;
        }

        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }

    return status;
}

static void calculate_crc_and_send_packet(PC_COM *const me, size_t message_len)
{
    // calculate CRC, including packet type and data (ignore the CRC itself)
    uint16_t crc_calc = crc_calculate(
        &((uint8_t *) &me->tx_packet)[sizeof(Packet_CRC_T)], sizeof(Packet_Type_T) + message_len);

    // add CRC to the packet
    me->tx_packet.crc = crc_calc;

    // transmit packet
    hdlc_transmit_packet(
        me->serial_io_interface->tx_func,
        ((uint8_t *) &me->tx_packet),
        sizeof(Packet_CRC_T) + sizeof(Packet_Type_T) + message_len);
}

/**
 ***************************************************************************************************
 *
 * @brief   Serial data ready callback, called by external context.
 *
 **************************************************************************************************/
static void Serial_Data_Ready(void *cb_data)
{
    static QEvt const event = QEVT_INITIALIZER(SERIAL_DATA_AVAILABLE_SIG);

    QActive *me = (QActive *) cb_data;
    QACTIVE_POST(me, &event, me);
}

/**
 ***************************************************************************************************
 *
 * @brief   Adapter for CLI write char
 *
 **************************************************************************************************/
static void cli_write_char(EmbeddedCli *embeddedCli, char c)
{
    cli_data_event->msg[cli_data_event->msg_size] = c;
    cli_data_event->msg_size++;

    if (cli_data_event->msg_size == CLI_DATA_MAX_LENGTH)
    {
        QACTIVE_POST(AO_PC_COM, (QEvt *) (cli_data_event), AO_PC_COM);

        // init a new cli data event
        cli_data_event           = Q_NEW(PCCOMCliDataEvent_T, POSTED_PC_COM_CLI_DATA_SIG);
        cli_data_event->msg_size = 0;
        memset(cli_data_event->msg, 0, CLI_DATA_MAX_LENGTH);
    }
}

/**
 ***************************************************************************************************
 *
 * @brief   Parse packet recieved from PC and handle it
 *
 **************************************************************************************************/
static void parse_and_handle_pc_packet(PC_COM *const me)
{
    uint16_t crc_calc;

    // calculate the CRC of the received packet, excluding the CRC itself (the initial bytes)
    crc_calc = crc_calculate(
        &((uint8_t *) &me->rx_packet)[sizeof(Packet_CRC_T)],
        me->hdlc_unpacker.packet_length - sizeof(Packet_CRC_T));

    // does crc in the packet match the calculated crc?
    if (me->rx_packet.crc == crc_calc)
    {
        switch (me->rx_packet.type)
        {
            // CLI character(s) received
            case MessageType_CLI_DATA:
                handle_cli_char_received(me);
                break;

            // command not found, let it go
            default:
                break;
        }
    }
}

static void handle_cli_char_received(PC_COM *const me)
{
    pb_istream_t stream = pb_istream_from_buffer(
        ((uint8_t *) &me->rx_packet.message), sizeof(RX_Message_Buffer_T));

    pb_decode(&stream, CLIData_fields, &me->rx_message_decoded);

    for (size_t i = 0; i < me->rx_message_decoded.CLI_data.msg.size; i++)
    {
        embeddedCliReceiveChar(me->embedded_cli, me->rx_message_decoded.CLI_data.msg.bytes[i]);
    }
}

static void DoForcedFault(PC_COM *const me, const PCCOMForceFaultEvent_T *pEvt)
{
    switch (pEvt->desiredFault)
    {
        case RESET_REASON_Q_ASSERT:
            Q_ASSERT(false == true);
            break;

        case RESET_REASON_HARD_FAULT: {
            // https://interrupt.memfault.com/blog/cortex-m-hardfault-debug#bad-pc-mpu-fault
            int (*bad_instruction)(void) = (void *) 0xE0000000;
            bad_instruction();
            break;
        }

        case RESET_REASON_MEM_MANAGE_FAULT: {
            // access a null pointer. If the MPU is enabled, this will
            // generate a memory management fault. If not enabled, it might
            // do nothing or generate a hard fault.
            volatile int *bad_addr = NULL;
            volatile int fault     = *bad_addr;
            *bad_addr              = 0x343234;
            Q_UNUSED_PAR(fault);
            Q_UNUSED_PAR(bad_addr);
            break;
        }

        case RESET_REASON_BUS_FAULT: {
            typedef void (*fn_t)();
            fn_t foo = (fn_t) (0x8004000);
            foo();
            break;
        }

        case RESET_REASON_USAGE_FAULT: {
            volatile int r;
            volatile unsigned int a    = 1;
            volatile unsigned int zero = 0;
            r                          = a / zero;
            Q_UNUSED_PAR(r);
            break;
        }
        default:
            embeddedCliPrint(me->embedded_cli, "Unhandled reset reason");
            break;
    }
}