#include "cli_manual_commands.h"
#include "bsp_manual.h"
#include "pc_com.h"
#include "posted_signals.h"
#include "qsafe.h"
#include "stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Q_DEFINE_THIS_MODULE("cli_manual_cmds");

#define HELP_FULL_DIGITAL_OUT_SET \
    "\r\n\
 Usage: digital-out-set PORT PIN VALUE\r\n\
 \r\n\
 PORT   Character A through K\r\n\
 PIN    Number 0 through 15\r\n\
 Value  0 (for low) or 1 (for high)\r\n"

void on_cli_digital_out_set(EmbeddedCli *cli, char *args, void *context)
{
    char arg_port;
    unsigned long arg_pin;
    unsigned long arg_value;
    char *arg_end;
    bool is_invalid_arg = false;

    if (embeddedCliGetTokenCount(args) != 3)
    {
        embeddedCliPrint(cli, HELP_FULL_DIGITAL_OUT_SET);
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);
    const char *arg3 = embeddedCliGetToken(args, 3);

    arg_port  = *arg1;
    arg_pin   = strtoul(arg2, &arg_end, 10);
    arg_value = strtoul(arg3, &arg_end, 10);

    if (strlen(arg1) > 1 || arg_port < 'A' || arg_port > 'K')
    {
        embeddedCliPrint(cli, " PORT must be a character between A and K\r\n");
        is_invalid_arg = true;
    }

    if (arg_pin > 15)
    {
        embeddedCliPrint(cli, " PIN must be a number between 0 and 15\r\n");
        is_invalid_arg = true;
    }

    if (arg_value > 1)
    {
        embeddedCliPrint(cli, " VALUE must be 0 (for low) or 1 (for high)\r\n");
        is_invalid_arg = true;
    }

    if (is_invalid_arg)
    {
        embeddedCliPrint(cli, HELP_FULL_DIGITAL_OUT_SET);
        return;
    }

    BSP_Manual_Config_and_Set_Digital_Output(arg_port, (uint8_t) arg_pin, (arg_value > 0));
}

#define HELP_FULL_DIGITAL_IN_READ \
    "\r\n\
 Usage: digital-in-read PORT PIN\r\n\
 \r\n\
 PORT   Character A through K\r\n\
 PIN    Number 0 through 15\r\n"

void on_cli_digital_in_read(EmbeddedCli *cli, char *args, void *context)
{
    char arg_port;
    unsigned long arg_pin;
    char *arg_end;
    bool is_invalid_arg = false;

    if (embeddedCliGetTokenCount(args) != 2)
    {
        embeddedCliPrint(cli, HELP_FULL_DIGITAL_IN_READ);
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);

    arg_port = *arg1;
    arg_pin  = strtoul(arg2, &arg_end, 10);

    if (strlen(arg1) > 1 || arg_port < 'A' || arg_port > 'K')
    {
        embeddedCliPrint(cli, " PORT must be a character between A and K\r\n");
        is_invalid_arg = true;
    }

    if (arg_pin > 15)
    {
        embeddedCliPrint(cli, " PIN must be a number between 0 and 15\r\n");
        is_invalid_arg = true;
    }

    if (is_invalid_arg)
    {
        embeddedCliPrint(cli, HELP_FULL_DIGITAL_IN_READ);
        return;
    }

    bool is_set = BSP_Manual_Config_and_Read_Digital_Input(arg_port, (uint8_t) arg_pin);
    embeddedCliPrint(cli, is_set ? "\r\n1\r\n" : "\r\n0\r\n");
}
