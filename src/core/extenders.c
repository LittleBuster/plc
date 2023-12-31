/*********************************************************************/
/*                                                                   */
/* Future City Programmable Logic Controller                         */
/*                                                                   */
/* Copyright (C) 2023 Denisov Smart Devices Limited                  */
/* License: GPLv3                                                    */
/* Written by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com) */
/*                                                                   */
/*********************************************************************/

#include <glib-2.0/glib.h>

#include <core/extenders.h>

#ifdef __arm__
#include <wiringPiLite/wiringPi.h>
#include <wiringPiLite/pcf8574.h>
#include <wiringPiLite/mcp23017.h>
#include <wiringPiLite/ads1115.h>
#endif

/*********************************************************************/
/*                                                                   */
/*                         PRIVATE VARIABLES                         */
/*                                                                   */
/*********************************************************************/

static GList *exts = NULL;

/*********************************************************************/
/*                                                                   */
/*                          PUBLIC FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

Extender *ExtenderNew(const char *name, ExtenderType type, unsigned bus, unsigned addr, unsigned base)
{
    Extender *ext = (Extender *)malloc(sizeof(Extender));

    strncpy(ext->name, name, SHORT_STR_LEN);
    ext->type = type;
    ext->addr = addr;
    ext->base = base;
    ext->bus = bus;

    return ext;    
}

bool ExtenderAdd(const Extender *ext, char *err)
{
#ifdef __arm__
    switch (ext->type) {
        case EXT_TYPE_PCF_8574:
            if (!pcf8574Setup(ext->bus, ext->addr, ext->base)) {
                return false;
            }
            break;

        case EXT_TYPE_MCP_23017:
            if (!mcp23017Setup(ext->bus, ext->addr, ext->base)) {
                return false;
            }
            break;

        case EXT_TYPE_ADS_1115:
            if (!ads1115Setup(ext->bus, ext->addr, ext->base)) {
                return false;
            }
            break;
    }
#endif
    exts = g_list_append(exts, (void *)ext);
    return true;
}
