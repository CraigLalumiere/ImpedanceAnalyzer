#include "app_cli_commands.h"
#include "app_cli_manual_commands.h"
#include "bsp.h"
#include "posted_signals.h"
#include "qpc.h"
#include "qsafe.h"
#include "app_cli.h"
#include "blinky.h"
#include "gpio.h"
#include "bsp_manual.h"
#include "reset.h"
#include "reset_reason_print.h"
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIMENSION_OF(a) ((sizeof(a)) / (sizeof(a[0])))
#define CLI_PRINT_BUFFER_SIZE 128

Q_DEFINE_THIS_MODULE("app_cli_commands")

// Command Functions
static void on_cli_toggle_led(EmbeddedCli *cli, char *args, void *context);
static void on_print_reset_reason(EmbeddedCli *cli, char *args, void *context);
static void on_do_q_assert(EmbeddedCli *cli, char *args, void *context);
static void on_fault(EmbeddedCli *cli, char *args, void *context);

static CliCommandBinding cli_cmd_list[] = {
    (CliCommandBinding){
        "toggle-led",                     // command name (spaces are not allowed)
        "Activates the green blinky LED", // Optional help for a command (NULL for no help)
        false,                            // flag whether to tokenize arguments
        NULL,                             // optional pointer to any application context
        on_cli_toggle_led                 // binding function
    },

    (CliCommandBinding){
        "fault",                       // command name (spaces are not allowed)
        "print the active fault info", // Optional help for a command
        false,                         // flag whether to tokenize arguments
        NULL,                          // optional pointer to any application context
        on_fault                       // binding function
    },

    (CliCommandBinding){
        "resetReason",                 // command name (spaces are not allowed)
        "Print the last reset reason", // Optional help for
                                       // a command
        false,                         // flag whether to tokenize arguments
        NULL,                          // optional pointer to any application context
        on_print_reset_reason          // binding function
    },
    (CliCommandBinding){
        "assertNow",                                      // command name (spaces are not allowed)
        "force a q_assert. Opt arg1: other reset reason", // Optional help for a command
        true,                                             // flag whether to tokenize arguments
        NULL,                                             // optional pointer to any application context
        on_do_q_assert                                    // binding function
    },

    (CliCommandBinding){
        "digital-out-set",                                                     // command name (spaces are not allowed)
        "Config a port and pin as a digital output, and set it to the value.", // Optional help
                                                                               // for a command
        true,                                                                  // flag whether to tokenize arguments
        NULL,                                                                  // optional pointer to any application context
        on_cli_digital_out_set                                                 // binding function
    },

    (CliCommandBinding){
        "digital-in-read",                                              // command name (spaces are not allowed)
        "Config a port and pin as a digital input, and read its value", // Optional help for
                                                                        // a command
        true,                                                           // flag whether to tokenize arguments
        NULL,                                                           // optional pointer to any application context
        on_cli_digital_in_read                                          // binding function
    },
};

void AppCLI_AddCommandsToCLI(EmbeddedCli *cli)
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
    QACTIVE_POST(AO_Blinky, &ToggleLEDEvent, AO_AppCLI);
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
    uint16_t argCount = embeddedCliGetTokenCount(args);
    if (argCount > 0)
    {
        const char *arg1 = embeddedCliGetToken(args, 1);
        reason = strtoul(arg1, NULL, 10);
    }

    DebugForceFaultEvent_T *e = Q_NEW(DebugForceFaultEvent_T, POSTED_FORCE_FAULT_SIG);
    e->desiredFault           = reason;
    QACTIVE_POST(AO_AppCLI, &e->super, AO_AppCLI);
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

        snprintf(print_buffer, sizeof(print_buffer), "ID: %d", (int)this_fault.id);
        embeddedCliPrint(cli, print_buffer);
        snprintf(
            print_buffer,
            sizeof(print_buffer),
            "Code: %d",
            (int)Fault_Manager_Get_Code(this_fault.id));
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