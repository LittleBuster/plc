/*********************************************************************/
/*                                                                   */
/* Future City Programmable Logic Controller                         */
/*                                                                   */
/* Copyright (C) 2023 Denisov Smart Devices Limited                  */
/* License: GPLv3                                                    */
/* Written by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com) */
/*                                                                   */
/*********************************************************************/

#include <threads.h>

#include <glib-2.0/glib.h>

#include <core/lcd.h>

#ifdef __arm__
#include <wiringPiLite/wiringPi.h>
#include <wiringPiLite/lcd.h>
#endif

/*********************************************************************/
/*                                                                   */
/*                         PRIVATE VARIABLES                         */
/*                                                                   */
/*********************************************************************/

static GList    *lcds = NULL;
static int      lcd_fd = 0;

/*********************************************************************/
/*                                                                   */
/*                          PUBLIC FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

LCD *LcdNew(const char *name, unsigned rs, unsigned rw, unsigned e, unsigned k, unsigned d4, unsigned d5, unsigned d6, unsigned d7)
{
    LCD *lcd = (LCD *)malloc(sizeof(LCD));

    strncpy(lcd->name, name, SHORT_STR_LEN);
    lcd->rs = rs;
    lcd->rw = rw;
    lcd->e = e;
    lcd->k = k;
    lcd->d4 = d4;
    lcd->d5 = d5;
    lcd->d6 = d6;
    lcd->d7 = d7;

    return lcd;
}

GList **LcdsGet()
{
    return &lcds;
}

LCD *LcdGet(const char *name)
{
    for (GList *l = lcds; l != NULL; l = l->next) {
        LCD *lcd = (LCD *)l->data;
        if (!strcmp(lcd->name, name)) {
            return lcd;
        }
    }
    return NULL;
}

bool LcdAdd(const LCD *lcd)
{
#ifdef __arm__
    pinMode(lcd->rw, OUTPUT);
    digitalWrite(lcd->rw, LOW);
    pinMode(lcd->k, OUTPUT);
    digitalWrite(lcd->k, HIGH);

    for (int i = 0; i < LCD_INIT_RETRIES; i++)
    {
        lcd_fd = lcdInit(LCD_ROWS, LCD_COLS, LCD_BITS, lcd->rs, lcd->e, lcd->d4, lcd->d5, lcd->d6, lcd->d7, 0, 0, 0, 0);
        if (lcd_fd != 0)
            break;
        delay(100);
    }

    if (lcd_fd == 0) {
        return false;
    }

    lcdClear(lcd_fd);
    lcdPosition(lcd_fd, 0, 0);
    lcdPuts(lcd_fd, LCD_DEFAULT_TEXT_UP);
    lcdPosition(lcd_fd, 0, 1);
    lcdPuts(lcd_fd, LCD_DEFAULT_TEXT_DOWN);
#endif

    lcds = g_list_append(lcds, (void *)lcd);

    return true;
}

void LcdPrint(const LCD *lcd, char *text)
{
#ifdef __arm__
    lcdPuts(lcd_fd, text);
#endif
}

void LcdPosSet(const LCD *lcd, unsigned row, unsigned col)
{
#ifdef __arm__
    lcdPosition(lcd_fd, col, row);
#endif
}

void LcdClear(const LCD *lcd)
{
#ifdef __arm__
    lcdClear(lcd_fd);
#endif
}
