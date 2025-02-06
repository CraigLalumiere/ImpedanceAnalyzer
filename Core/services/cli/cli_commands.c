#include "cli_commands.h"
#include "analyzer.h"
#include "blinky.h"
#include "bsp.h"
#include "bsp_manual.h"
#include "cli_manual_commands.h"
#include "interfaces/gpio.h"
#include "pc_com.h"
#include "posted_signals.h"
#include "qpc.h"
#include "qsafe.h"
#include "reset.h"
#include "reset_reason_print.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIMENSION_OF(a)       ((sizeof(a)) / (sizeof(a[0])))
#define CLI_PRINT_BUFFER_SIZE 128

// Command Functions
static void on_cli_toggle_led(EmbeddedCli *cli, char *args, void *context);
static void on_print_reset_reason(EmbeddedCli *cli, char *args, void *context);
static void on_do_q_assert(EmbeddedCli *cli, char *args, void *context);
static void on_fault(EmbeddedCli *cli, char *args, void *context);
static void on_cli_set_freq_range(EmbeddedCli *cli, char *args, void *context);
static void on_cli_do_impedance_sweep(EmbeddedCli *cli, char *args, void *context);
static void on_cli_do_offset_cal(EmbeddedCli *cli, char *args, void *context);
static void on_cli_do_gain_cal(EmbeddedCli *cli, char *args, void *context);
static void on_cli_set_impedance(EmbeddedCli *cli, char *args, void *context);

static CliCommandBinding cli_cmd_list[] = {
    (CliCommandBinding) {
        "toggle-led",                     // command name (spaces are not allowed)
        "Activates the green blinky LED", // Optional help for a command (NULL for no help)
        false,                            // flag whether to tokenize arguments
        NULL,                             // optional pointer to any application context
        on_cli_toggle_led                 // binding function
    },

    (CliCommandBinding) {
        "fault",                       // command name (spaces are not allowed)
        "print the active fault info", // Optional help for a command
        false,                         // flag whether to tokenize arguments
        NULL,                          // optional pointer to any application context
        on_fault                       // binding function
    },

    (CliCommandBinding) {
        "resetReason",                 // command name (spaces are not allowed)
        "Print the last reset reason", // Optional help for
                                       // a command
        false,                         // flag whether to tokenize arguments
        NULL,                          // optional pointer to any application context
        on_print_reset_reason          // binding function
    },
    (CliCommandBinding) {
        "assertNow",                                      // command name (spaces are not allowed)
        "force a q_assert. Opt arg1: other reset reason", // Optional help for a command
        true,                                             // flag whether to tokenize arguments
        NULL,          // optional pointer to any application context
        on_do_q_assert // binding function
    },

    (CliCommandBinding) {
        "digital-out-set", // command name (spaces are not allowed)
        "Config a port and pin as a digital output, and set it to the value.", // Optional help
                                                                               // for a command
        true,                  // flag whether to tokenize arguments
        NULL,                  // optional pointer to any application context
        on_cli_digital_out_set // binding function
    },

    (CliCommandBinding) {
        "digital-in-read", // command name (spaces are not allowed)
        "Config a port and pin as a digital input, and read its value", // Optional help for
                                                                        // a command
        true,                  // flag whether to tokenize arguments
        NULL,                  // optional pointer to any application context
        on_cli_digital_in_read // binding function
    },

    (CliCommandBinding) {
        "set-freq-range", // command name (spaces are not allowed)
        "Set start and end frequencies and number of datapoints", // Optional help for
                                                                  // a command
        true,                 // flag whether to tokenize arguments
        NULL,                 // optional pointer to any application context
        on_cli_set_freq_range // binding function
    },

    (CliCommandBinding) {
        "impedance-sweep", // command name (spaces are not allowed)
        "Commence frequency sweep to generate impedance plot", // Optional help for
                                                               // a command
        true,                                                  // flag whether to tokenize arguments
        NULL,                     // optional pointer to any application context
        on_cli_do_impedance_sweep // binding function
    },

    (CliCommandBinding) {
        "offset-calibration", // command name (spaces are not allowed)
        "Perform offset calibration with open-circuit load", // Optional help for
                                                             // a command
        true,                                                // flag whether to tokenize arguments
        NULL,                // optional pointer to any application context
        on_cli_do_offset_cal // binding function
    },

    (CliCommandBinding) {
        "gain-calibration",                       // command name (spaces are not allowed)
        "Perform gain calibration with 10k load", // Optional help for
                                                  // a command
        true,                                     // flag whether to tokenize arguments
        NULL,                                     // optional pointer to any application context
        on_cli_do_gain_cal                        // binding function
    },

    (CliCommandBinding) {
        "source-impedance",                                 // command name (spaces are not allowed)
        "Set the source impedance of the signal generator", // Optional help for
                                                            // a command
        true,                                               // flag whether to tokenize arguments
        NULL,                // optional pointer to any application context
        on_cli_set_impedance // binding function
    },
};

void CLI_AddCommands(EmbeddedCli *cli)
{
    for (unsigned i = 0; i < DIMENSION_OF(cli_cmd_list); i++)
    {
        embeddedCliAddBinding(cli, cli_cmd_list[i]);
    }
}

static void on_cli_toggle_led(EmbeddedCli *cli, char *args, void *context)
{
    // statically allocated and const event to post to the Blinky active object
    static QEvt const ToggleLEDEvent = QEVT_INITIALIZER(POSTED_BLINKY_TOGGLE_USER_LED);

    // send (post) the event to the Blinky active object
    QACTIVE_POST(AO_Blinky, &ToggleLEDEvent, null);
}

static void on_print_reset_reason(EmbeddedCli *cli, char *args, void *context)
{
    char buffer[90];
    snprintf_reset_reason(buffer, 90, "\r\n");
    embeddedCliPrint(cli, buffer);
}
static void on_do_q_assert(EmbeddedCli *cli, char *args, void *context)
{
    Q_UNUSED_PAR(cli);
    Q_UNUSED_PAR(context);
    Reset_Reason_T reason = RESET_REASON_Q_ASSERT;
    uint16_t argCount     = embeddedCliGetTokenCount(args);
    if (argCount > 0)
    {
        const char *arg1 = embeddedCliGetToken(args, 1);
        reason           = strtoul(arg1, NULL, 10);
    }

    DebugForceFaultEvent_T *e = Q_NEW(DebugForceFaultEvent_T, POSTED_FORCE_FAULT_SIG);
    e->desiredFault           = reason;
    QACTIVE_POST(AO_PC_COM, &e->super, NULL);
}

static void on_fault(EmbeddedCli *cli, char *args, void *context)
{
    char print_buffer[CLI_PRINT_BUFFER_SIZE] = {0};

    Active_Fault_T *active_faults = Fault_Manager_Get_Active_Fault_List();
    if (active_faults[0].id == FAULT_ID_NONE)
    {
        embeddedCliPrint(cli, "No faults recorded");
        return;
    }

    for (uint8_t i = 0; i < FAULT_MANAGER_BUFFER_LENGTH; i++)
    {
        Active_Fault_T this_fault = active_faults[i];
        if (this_fault.id == FAULT_ID_NONE)
        {
            break;
        }

        snprintf(print_buffer, sizeof(print_buffer), "ID: %d", (int) this_fault.id);
        embeddedCliPrint(cli, print_buffer);
        snprintf(
            print_buffer,
            sizeof(print_buffer),
            "Code: %d",
            (int) Fault_Manager_Get_Code(this_fault.id));
        embeddedCliPrint(cli, print_buffer);
        snprintf(
            print_buffer,
            sizeof(print_buffer),
            "Description: %s",
            Fault_Manager_Get_Description(this_fault.id));
        embeddedCliPrint(cli, print_buffer);
        snprintf(print_buffer, sizeof(print_buffer), "Message: %s\r\n", this_fault.msg);
        embeddedCliPrint(cli, print_buffer);
    }
}

#define HELP_SET_FREQ_RANGE \
    "\r\n\
 Usage: set-freq-range START END NUM_POINTS\r\n\
 \r\n\
 START    start frequency [Hz]\r\n\
 END        end frequency [Hz]]\r\n\
 NUM_POINTS        number of datapoints\r\n\
"

static void on_cli_set_freq_range(EmbeddedCli *cli, char *args, void *context)
{
    char print_buffer[CLI_PRINT_BUFFER_SIZE] = {0};
    uint32_t arg_f_start;
    uint32_t arg_f_end;
    uint32_t arg_N;
    char *arg_end;

    if (embeddedCliGetTokenCount(args) != 3)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);
    const char *arg3 = embeddedCliGetToken(args, 3);

    arg_f_start = strtoul(arg1, &arg_end, 10);
    arg_f_end   = strtol(arg2, &arg_end, 10);
    arg_N       = strtol(arg3, &arg_end, 10);

    if (arg_f_start > arg_f_end)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    if (arg_f_start < FREQ_MIN)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    if (arg_f_end > FREQ_MAX)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    if (arg_N > FREQ_POINTS_MAX || arg_N < FREQ_POINTS_MIN)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    Analyzer_Set_Freq_Range(arg_f_start, arg_f_end, arg_N);

    snprintf(
        print_buffer,
        sizeof(print_buffer),
        "%d datapoints between %dHz and %dHz\r\n",
        (int) arg_N,
        (int) arg_f_start,
        (int) arg_f_end);
    embeddedCliPrint(cli, print_buffer);
}

static void on_cli_do_impedance_sweep(EmbeddedCli *cli, char *args, void *context)
{
    Analyzer_Begin_Impedance_Sweep();
}

static void on_cli_do_offset_cal(EmbeddedCli *cli, char *args, void *context)
{
    Analyzer_Begin_Offset_Calibration();
}

static void on_cli_do_gain_cal(EmbeddedCli *cli, char *args, void *context)
{
    Analyzer_Begin_Gain_Calibration();
}

#define HELP_SET_IMPEDANCE \
    "\r\n\
 Usage: source-impedance N\r\n\
 \r\n\
 N    [0, 7]\r\n\
"

static void on_cli_set_impedance(EmbeddedCli *cli, char *args, void *context)
{
    char print_buffer[CLI_PRINT_BUFFER_SIZE] = {0};
    uint32_t arg_f_start;
    uint32_t arg_f_end;
    uint32_t arg_N;
    char *arg_end;

    if (embeddedCliGetTokenCount(args) != 1)
    {
        embeddedCliPrint(cli, HELP_SET_IMPEDANCE);
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);

    arg_N = strtol(arg1, &arg_end, 10);

    if (arg_N > 7)
    {
        embeddedCliPrint(cli, HELP_SET_FREQ_RANGE);
        return;
    }

    SetSourceImpedanceEvent_T *event = Q_NEW(
        SetSourceImpedanceEvent_T, POSTED_SET_SOURCE_IMPEDANCE_SIG);
    event->impedance = arg_N;
    QACTIVE_POST(AO_Analyzer, &event->super, NULL);

    embeddedCliPrint(cli, print_buffer);
}