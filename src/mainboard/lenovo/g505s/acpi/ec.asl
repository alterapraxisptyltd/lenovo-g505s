/*
 * Copyright (C) 2014 Edward O'Callaghan <eocallaghan@alterapraxis.com>
 * Subject to the GNU GPL v2.
 */

/* Defines EC bits specific to the mainboard, needed by EC ASL */
/* #include "mainboard.h" */

/* ACPI code for EC functions */
#include <ec/lenovo/h8/acpi/ec.asl>

Scope(\_SB.PCI0.LPCB.EC)
{
}
