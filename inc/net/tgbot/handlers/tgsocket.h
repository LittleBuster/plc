/*********************************************************************/
/*                                                                   */
/* Future City Programmable Logic Controller                         */
/*                                                                   */
/* Copyright (C) 2023 Denisov Smart Devices Limited                  */
/* License: GPLv3                                                    */
/* Written by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com) */
/*                                                                   */
/*********************************************************************/

#ifndef __TG_SOCKET_H__
#define __TG_SOCKET_H__

#include <stdbool.h>

/**
 * @brief Process Socket selection Menu commands
 * 
 * @param token Telegram Bot uniq token
 * @param from From user id
 * @param message Telegram message 
 */
void TgSocketSelectProcess(const char *token, unsigned from, const char *message);

/**
 * @brief Process Socket Menu commands
 * 
 * @param token Telegram Bot uniq token
 * @param from From user id
 * @param message Telegram message 
 */
void TgSocketProcess(const char *token, unsigned from, const char *message);

/**
 * @brief Process Light selection Menu commands
 * 
 * @param token Telegram Bot uniq token
 * @param from From user id
 * @param message Telegram message 
 */
void TgLightSelectProcess(const char *token, unsigned from, const char *message);

/**
 * @brief Process Light Menu commands
 * 
 * @param token Telegram Bot uniq token
 * @param from From user id
 * @param message Telegram message 
 */
void TgLightProcess(const char *token, unsigned from, const char *message);

#endif /* __TG_SOCKET_H__ */
