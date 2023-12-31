/*********************************************************************/
/*                                                                   */
/* Future City Programmable Logic Controller                         */
/*                                                                   */
/* Copyright (C) 2023 Denisov Smart Devices Limited                  */
/* License: GPLv3                                                    */
/* Written by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com) */
/*                                                                   */
/*********************************************************************/

#include <stdio.h>

#include <glib-2.0/glib.h>
#include <jansson.h>
#include <fcgiapp.h>

#include <net/web/handlers/securityh.h>
#include <net/web/response.h>
#include <utils/utils.h>
#include <utils/log.h>
#include <stack/rpc.h>

/*********************************************************************/
/*                                                                   */
/*                         PRIVATE FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

static bool HandlerStatusSet(FCGX_Request *req, GList **params)
{
    json_t  *root = json_object();
    bool    found = false;
    bool    status = false;

    for (GList *p = *params; p != NULL; p = p->next) {
        UtilsReqParam *param = (UtilsReqParam *)p->data;

        if (!strcmp(param->name, "status")) {
            if (!strcmp(param->value, "true")) {
                status = true;
                found = true;
            } else if (!strcmp(param->value, "false")) {
                found = true;
                status = false;
            }
        }
    }

    if (!found) {
        return ResponseFailSend(req, "SECURITYH", "Security command ivalid");
    }

    if (!RpcSecurityStatusSet(RPC_DEFAULT_UNIT, status)) {
        return ResponseFailSend(req, "SECURITYH", "Failed to set security status");
    }

    return ResponseOkSend(req, root);
}

static bool HandlerStatusGet(FCGX_Request *req, GList **params)
{
    json_t  *root = json_object();
    bool    status = false;

    if (!RpcSecurityStatusGet(RPC_DEFAULT_UNIT, &status)) {
        return ResponseFailSend(req, "SECURITYH", "Failed to get security status");
    }

    json_object_set_new(root, "status", json_boolean(status));

    return ResponseOkSend(req, root);
}

static bool HandlerAlarmSet(FCGX_Request *req, GList **params)
{
    json_t  *root = json_object();
    bool    found = false;
    bool    status = false;

    for (GList *p = *params; p != NULL; p = p->next) {
        UtilsReqParam *param = (UtilsReqParam *)p->data;

        if (!strcmp(param->name, "alarm")) {
            if (!strcmp(param->value, "true")) {
                status = true;
                found = true;
            } else if (!strcmp(param->value, "false")) {
                found = true;
                status = false;
            }
        }
    }

    if (!found) {
        return ResponseFailSend(req, "SECURITYH", "Security command ivalid");
    }

    if (!RpcSecurityAlarmSet(RPC_DEFAULT_UNIT, status)) {
        return ResponseFailSend(req, "SECURITYH", "Failed to set security alarm");
    }

    return ResponseOkSend(req, root);
}

static bool HandlerAlarmGet(FCGX_Request *req, GList **params)
{
    json_t  *root = json_object();
    bool    alarm = false;

    if (!RpcSecurityAlarmGet(RPC_DEFAULT_UNIT, &alarm)) {
        return ResponseFailSend(req, "SECURITYH", "Failed to get security alarm");
    }

    json_object_set_new(root, "alarm", json_boolean(alarm));

    return ResponseOkSend(req, root);
}

static bool HandlerSensorsGet(FCGX_Request *req, GList **params)
{
    json_t  *root = json_object();
    GList   *sensors = NULL;

    if (!RpcSecuritySensorsGet(RPC_DEFAULT_UNIT, &sensors)) {
        return ResponseFailSend(req, "SECURITYH", "Failed to get security sensors");
    }

    json_t *jsensors = json_array();

    for (GList *s = sensors; s != NULL; s = s->next) {
        RpcSecuritySensor *sensor = (RpcSecuritySensor *)s->data;

        json_t *jsensor = json_object();
        json_object_set_new(jsensor, "name", json_string(sensor->name));
        json_object_set_new(jsensor, "type", json_integer(sensor->type));
        json_object_set_new(jsensor, "detected", json_boolean(sensor->detected));
        json_array_append_new(jsensors, jsensor);

        free(sensor);
    }

    json_object_set_new(root, "sensors", jsensors);
    g_list_free(sensors);

    return ResponseOkSend(req, root);
}

/*********************************************************************/
/*                                                                   */
/*                          PUBLIC FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

bool HandlerSecurityProcess(FCGX_Request *req, GList **params)
{
    for (GList *p = *params; p != NULL; p = p->next) {
        UtilsReqParam *param = (UtilsReqParam *)p->data;

        if (!strcmp(param->name, "cmd")) {
            if (!strcmp(param->value, "status_set")) {
                return HandlerStatusSet(req, params);
            } else if (!strcmp(param->value, "status_get")) {
                return HandlerStatusGet(req, params);
            } else if (!strcmp(param->value, "sensors_get")) {
                return HandlerSensorsGet(req, params);
            } else if (!strcmp(param->value, "alarm_get")) {
                return HandlerAlarmGet(req, params);
            } else if (!strcmp(param->value, "alarm_set")) {
                return HandlerAlarmSet(req, params);
            } else {
                return false;
            }
        }
    }

    return true;
}
