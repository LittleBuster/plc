/*********************************************************************/
/*                                                                   */
/* Future City Programmable Logic Controller                         */
/*                                                                   */
/* Copyright (C) 2023 Denisov Smart Devices Limited                  */
/* License: GPLv3                                                    */
/* Written by Sergey Denisov aka LittleBuster (DenisovS21@gmail.com) */
/*                                                                   */
/*********************************************************************/

#include <string.h>
#include <stdlib.h>

#include <jansson.h>

#include <utils/utils.h>
#include <utils/log.h>
#include <utils/configs/configs.h>
#include <utils/configs/cfgsecurity.h>
#include <utils/configs/cfgmeteo.h>
#include <utils/configs/cfgsocket.h>
#include <utils/configs/cfgtank.h>
#include <utils/configs/cfgwaterer.h>
#include <core/gpio.h>
#include <core/extenders.h>
#include <core/lcd.h>
#include <net/notifier.h>
#include <net/web/webserver.h>
#include <net/tgbot/tgbot.h>
#include <net/tgbot/tgmenu.h>
#include <db/database.h>
#include <stack/stack.h>
#include <scenario/scenario.h>
#include <cam/camera.h>
#include <plc/plc.h>
#include <plc/menu.h>
#include <controllers/meteo.h>
#include <controllers/socket.h>

/*********************************************************************/
/*                                                                   */
/*                         PRIVATE FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

static bool FactoryRead(const char *path, ConfigsFactory *factory)
{
    json_error_t    error;
    char            full_path[STR_LEN];

    strncpy(full_path, path, STR_LEN);
    strcat(full_path, CONFIGS_FACTORY_FILE);

    json_t *data = json_load_file(full_path, 0, &error);
    if (!data) {
        return false;
    }

    strncpy(factory->board, json_string_value(json_object_get(data, "board")), SHORT_STR_LEN);
    strncpy(factory->revision, json_string_value(json_object_get(data, "revision")), SHORT_STR_LEN);

    json_decref(data);
    return true;
}

static bool BoardRead(const char *path, const ConfigsFactory *factory)
{
    char            full_path[STR_LEN];
    json_error_t    error;
    size_t          index;
    json_t          *value;
    char            err[ERROR_STR_LEN];

    strncpy(full_path, path, STR_LEN);
    strcat(full_path, "boards/");
    strcat(full_path, factory->board);
    strcat(full_path, "-");
    strcat(full_path, factory->revision);
    strcat(full_path, ".json");

    json_t *data = json_load_file(full_path, 0, &error);
    if (!data) {
        return false;
    }

    /**
     * Reading Extenders configs
     */

    json_array_foreach(json_object_get(data, "extenders"), index, value) {
        ExtenderType type;

        const char *type_str = json_string_value(json_object_get(value, "type"));
        if (!strcmp(type_str, "pcf8574")) {
            type = EXT_TYPE_PCF_8574;
        } else if (!strcmp(type_str, "mcp23017")) {
            type = EXT_TYPE_MCP_23017;
        } else if (!strcmp(type_str, "ads1115")) {
            type = EXT_TYPE_ADS_1115;
        } else {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown Extender type \"%s\"", type_str);
            return false;
        }

        Extender *ext = ExtenderNew(
            json_string_value(json_object_get(value, "name")),
            type,
            json_integer_value(json_object_get(value, "bus")),
            json_integer_value(json_object_get(value, "addr")),
            json_integer_value(json_object_get(value, "base"))
        );

        if (!ExtenderAdd(ext, err)) {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Failed to add Extender \"%s\": %s", ext->name, err);
            return false;
        }

        LogF(LOG_TYPE_INFO, "CONFIGS", "Add Extender name: \"%s\" type: \"%s\" bus: \"%u\" addr: \"%u\" base: \"%u\"",
                ext->name, type_str, ext->bus, ext->addr, ext->base);
    }

    /**
     * Reading GPIO configs
     */

    json_array_foreach(json_object_get(data, "gpio"), index, value) {
        GpioMode    mode;
        GpioPull    pull;
        GpioType    type;

        const char *type_str = json_string_value(json_object_get(value, "type"));
        if (!strcmp(type_str, "analog")) {
            type = GPIO_TYPE_ANALOG;
        } else if (!strcmp(type_str, "digital")) {
            type = GPIO_TYPE_DIGITAL;
        } else {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown GPIO pin type \"%s\"", type_str);
            return false;
        }

        const char *mode_str = json_string_value(json_object_get(value, "mode"));
        if (!strcmp(mode_str, "input")) {
            mode = GPIO_MODE_INPUT;
        } else if (!strcmp(mode_str, "output")) {
            mode = GPIO_MODE_OUTPUT;
        } else {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown GPIO pin mode \"%s\"", mode_str);
            return false;
        }

        const char *pull_str = json_string_value(json_object_get(value, "pull"));
        if (!strcmp(pull_str, "up")) {
            pull = GPIO_PULL_UP;
        } else if (!strcmp(pull_str, "down")) {
            pull = GPIO_PULL_DOWN;
        } else if (!strcmp(pull_str, "none")) {
            pull = GPIO_PULL_NONE;
        } else {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown GPIO pin pull \"%s\"", pull_str);
            return false;
        }

        GpioPin *pin = GpioPinNew(
            json_string_value(json_object_get(value, "name")),
            type,
            json_integer_value(json_object_get(value, "pin")),
            mode,
            pull
        );

        if (!GpioPinAdd(pin, err)) {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Failed to add GPIO pin \"%s\": %s", pin->name, err);
            return false;
        }

        LogF(LOG_TYPE_INFO, "CONFIGS", "Add GPIO name: \"%s\" pin: \"%d\" type: \"%s\" mode: \"%s\" pull: \"%s\"",
                pin->name, pin->pin, type_str, mode_str, pull_str);
    }

    /**
     * Reading LCD configs
     */

    json_array_foreach(json_object_get(data, "lcd"), index, value) {
        LCD *lcd = LcdNew(
            json_string_value(json_object_get(value, "name")),
            json_integer_value(json_object_get(value, "rs")),
            json_integer_value(json_object_get(value, "rw")),
            json_integer_value(json_object_get(value, "e")),
            json_integer_value(json_object_get(value, "k")),
            json_integer_value(json_object_get(value, "d4")),
            json_integer_value(json_object_get(value, "d5")),
            json_integer_value(json_object_get(value, "d6")),
            json_integer_value(json_object_get(value, "d7"))
        );

        if (!LcdAdd(lcd)) {
            json_decref(data);
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Failed to add LCD \"%s\"", lcd->name);
            return false;
        }

        LogF(LOG_TYPE_INFO, "CONFIGS", "Add LCD name: \"%s\"", lcd->name);
    }

    /**
     * Cleanup
     */

    json_decref(data);
    return true;
}

static bool ControllersRead(const char *path)
{
    char            full_path[STR_LEN];
    json_error_t    error;

    snprintf(full_path, STR_LEN, "%s%s", path, CONFIGS_CONTROLLERS_FILE);

    json_t *data = json_load_file(full_path, 0, &error);
    if (!data) {
        return false;
    }

    if (!CfgSecurityLoad(data)) {
        json_decref(data);
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load security configs");
        return false;
    }

    if (!CfgMeteoLoad(data)) {
        json_decref(data);
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load meteo configs");
        return false;
    }

    if (!CfgSocketLoad(data)) {
        json_decref(data);
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load socket configs");
        return false;
    }

    if (!CfgTankLoad(data)) {
        json_decref(data);
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load tank configs");
        return false;
    }

    if (!CfgWatererLoad(data)) {
        json_decref(data);
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load waterer configs");
        return false;
    }

    json_decref(data);
    return true;
}

static bool PlcRead(const char *path)
{
    char            full_path[STR_LEN];
    json_error_t    error;
    size_t          index, ext_index;
    json_t          *value, *ext_value;
    GpioPin         *gpio = NULL;

    snprintf(full_path, STR_LEN, "%s%s", path, CONFIGS_PLC_FILE);

    json_t *data = json_load_file(full_path, 0, &error);
    if (!data) {
        return false;
    }

    json_t *jglobal = json_object_get(data, "global");
    json_t *jggpio = json_object_get(jglobal, "gpio");

    gpio = GpioPinGet(json_string_value(json_object_get(jggpio, "alarm")));
    if (gpio == NULL) {
        LogF(LOG_TYPE_ERROR, "CONFIGS", "Security controller error: Alarm LED GPIO \"%s\" not found",
            json_string_value(json_object_get(jggpio, "alarm")));
        return false;
    }
    PlcGpioSet(PLC_GPIO_ALARM_LED, gpio);

    gpio = GpioPinGet(json_string_value(json_object_get(jggpio, "buzzer")));
    if (gpio == NULL) {
        LogF(LOG_TYPE_ERROR, "CONFIGS", "Security controller error: Buzzer GPIO \"%s\" not found",
            json_string_value(json_object_get(jggpio, "buzzer")));
        return false;
    }
    PlcGpioSet(PLC_GPIO_BUZZER, gpio);

    json_t *server = json_object_get(data, "server");
    const char *ip = json_string_value(json_object_get(server, "ip"));
    const unsigned port = json_integer_value(json_object_get(server, "port"));
    WebServerCredsSet(ip, port);
    LogF(LOG_TYPE_INFO, "CONFIGS", "Add Web Server at ip: \"%s\" port: \"%u\"", ip, port);

    json_t *notifier = json_object_get(data, "notifier");

    json_t *jtg = json_object_get(notifier, "telegram");
    const char *bot = json_string_value(json_object_get(jtg, "bot"));
    const unsigned chat = json_integer_value(json_object_get(jtg, "chat"));
    NotifierTelegramCredsSet(bot, chat);
    LogF(LOG_TYPE_INFO, "CONFIGS", "Add Telegram bot Notifier token: \"%s\" chat: \"%u\"", bot, chat);

    json_t *jsms = json_object_get(notifier, "sms");
    const char *api = json_string_value(json_object_get(jsms, "api"));
    const char *phone = json_string_value(json_object_get(jsms, "phone"));
    NotifierSmsCredsSet(api, phone);
    LogF(LOG_TYPE_INFO, "CONFIGS", "Add SMS Notifier token: \"%s\" phone: \"%s\"", api, phone);

    json_t *tgbot = json_object_get(data, "tgbot");
    TgBotTokenSet(json_string_value(json_object_get(tgbot, "token")));
    if (json_boolean_value(json_object_get(tgbot, "enabled"))) {
        LogF(LOG_TYPE_INFO, "CONFIGS", "Add Telegram bot token: \"%s\"", json_string_value(json_object_get(tgbot, "token")));
        json_array_foreach(json_object_get(tgbot, "users"), index, value) {
            TgBotUser *user = TgBotUserNew(
                json_string_value(json_object_get(value, "name")),
                json_integer_value(json_object_get(value, "id"))
            );

            TgBotUserAdd(user);

            TgMenu *menu = TgMenuNew(
                user->chat_id
            );
            
            TgMenuAdd(menu);

            LogF(LOG_TYPE_INFO, "CONFIGS", "Add Telegram bot user: \"%s\"", user->name);
        }
    } else {
        TgBotDisable();
        Log(LOG_TYPE_INFO, "CONFIGS", "Telegram bot disabled");
    }

    /**
     * Stack configs
     */

    json_array_foreach(json_object_get(data, "stack"), index, value) {
        StackUnit *unit = StackUnitNew(
            json_integer_value(json_object_get(value, "id")),
            json_string_value(json_object_get(value, "name")),
            json_string_value(json_object_get(value, "ip")),
            json_integer_value(json_object_get(value, "port"))
        );
        StackUnitAdd(unit);
        LogF(LOG_TYPE_INFO, "CONFIGS", "Add Stack unit: \"%s\"", unit->name);
    }

    /**
     * Cameras configs
     */

    json_array_foreach(json_object_get(data, "cam"), index, value) {
        CameraType type;

        const char *type_str = json_string_value(json_object_get(value, "type"));

        if (!strcmp(type_str, "ipcam")) {
            type = CAM_TYPE_IP;
        } else {
            LogF(LOG_TYPE_ERROR, "CONFIGS", "Invalid camera type \"%s\"", type_str);
            return false;
        }

        Camera *cam = CameraNew(
            json_string_value(json_object_get(value, "name")),
            type
        );

        if (type == CAM_TYPE_IP) {
            json_t *jipcam = json_object_get(value, "ipcam");

            strncpy(cam->ipcam.ip, json_string_value(json_object_get(jipcam, "ip")), SHORT_STR_LEN);
            strncpy(cam->ipcam.login, json_string_value(json_object_get(jipcam, "login")), SHORT_STR_LEN);
            strncpy(cam->ipcam.password, json_string_value(json_object_get(jipcam, "password")), SHORT_STR_LEN);
            cam->ipcam.stream = json_integer_value(json_object_get(jipcam, "stream"));
        }

        CameraAdd(cam);
        LogF(LOG_TYPE_INFO, "CONFIGS", "Add Camera: \"%s\"", cam->name);
    }

    /**
     * Menu configs
     */

    json_t *jmenu = json_object_get(data, "menu");
    LogF(LOG_TYPE_INFO, "CONFIGS", "Add Menu LCD \"%s\"", json_string_value(json_object_get(jmenu, "lcd")));

    if (strcmp(json_string_value(json_object_get(jmenu, "lcd")), "none")) {
    	LCD *lcd = LcdGet(json_string_value(json_object_get(jmenu, "lcd")));
    	if (lcd == NULL) {
        	LogF(LOG_TYPE_ERROR, "CONFIGS", "LCD of menu not found \"%s\"",
            	json_string_value(json_object_get(jmenu, "lcd"))
        	);
        	return false;
    	}
    	MenuLcdSet(lcd);
    }

    Log(LOG_TYPE_INFO, "CONFIGS", "Add Menu GPIOs");

    json_t *jgpio = json_object_get(jmenu, "gpio");

    gpio = GpioPinGet(json_string_value(json_object_get(jgpio, "up")));
    if (gpio == NULL) {
        LogF(LOG_TYPE_ERROR, "CONFIGS", "Menu button error: GPIO \"%s\" not found",
            json_string_value(json_object_get(jgpio, "up")));
        return false;
    }
    MenuGpioSet(MENU_GPIO_UP, gpio);

    gpio = GpioPinGet(json_string_value(json_object_get(jgpio, "middle")));
    if (gpio == NULL) {
        LogF(LOG_TYPE_ERROR, "CONFIGS", "Menu button error: GPIO \"%s\" not found",
            json_string_value(json_object_get(jgpio, "middle")));
        return false;
    }
    MenuGpioSet(MENU_GPIO_MIDDLE, gpio);

    gpio = GpioPinGet(json_string_value(json_object_get(jgpio, "down")));
    if (gpio == NULL) {
        LogF(LOG_TYPE_ERROR, "CONFIGS", "Menu button error: GPIO \"%s\" not found",
            json_string_value(json_object_get(jgpio, "down")));
        return false;
    }
    MenuGpioSet(MENU_GPIO_DOWN, gpio);

    json_array_foreach(json_object_get(jmenu, "levels"), index, value) {
        MenuLevel *level = MenuLevelNew(json_string_value(json_object_get(value, "name")));

        LogF(LOG_TYPE_INFO, "CONFIGS", "Add Menu Level \"%s\"", level->name);

        json_array_foreach(json_object_get(value, "values"), ext_index, ext_value) {
            MenuController ctrl;

            if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "meteo")) {
                ctrl = MENU_CTRL_METEO;
            } else if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "time")) {
                ctrl = MENU_CTRL_TIME;
            } else if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "tank")) {
                ctrl = MENU_CTRL_TANK;
            } else if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "socket")) {
                ctrl = MENU_CTRL_SOCKET;
            } else if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "light")) {
                ctrl = MENU_CTRL_LIGHT;
            } else if (!strcmp(json_string_value(json_object_get(ext_value, "ctrl")), "security")) {
                ctrl = MENU_CTRL_SECURITY;
            }else {
                LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown ctrl type");
                return false;
            }

            LogF(LOG_TYPE_INFO, "CONFIGS", "Add Menu value ctrl: \"%s\" alias \"%s\"",
                json_string_value(json_object_get(ext_value, "ctrl")),
                json_string_value(json_object_get(ext_value, "alias"))
            );

            MenuValue *value = MenuValueNew(
                json_integer_value(json_object_get(ext_value, "row")),
                json_integer_value(json_object_get(ext_value, "col")),
                json_string_value(json_object_get(ext_value, "alias")),
                ctrl
            );

            if (ctrl == MENU_CTRL_METEO) {
                MeteoSensor *sensor = MeteoSensorGet(
                    json_string_value(json_object_get(ext_value, "meteo"))
                );

                if (sensor == NULL) {
                    LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown menu meteo sensor");
                    return false;
                }

                value->meteo.sensor = sensor;
            } else if (ctrl == MENU_CTRL_TANK) {
                json_t *jtank = json_object_get(ext_value, "tank");

                Tank *tank = TankGet(
                    json_string_value(json_object_get(jtank, "name"))
                );

                if (tank == NULL) {
                    LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown menu tank");
                    return false;
                }

                value->tank.tank = tank;

                if (!strcmp(json_string_value(json_object_get(jtank, "param")), "pump")) {
                    value->tank.param = MENU_TANK_PUMP;
                } else if (!strcmp(json_string_value(json_object_get(jtank, "param")), "valve")) {
                    value->tank.param = MENU_TANK_VALVE;
                } else if (!strcmp(json_string_value(json_object_get(jtank, "param")), "level")) {
                    value->tank.param = MENU_TANK_LEVEL;
                }
            } else if (ctrl == MENU_CTRL_SOCKET) {
                Socket *socket = SocketGet(
                    json_string_value(json_object_get(ext_value, "socket"))
                );

                if (socket == NULL) {
                    LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown menu socket");
                    return false;
                }

                value->socket.sock = socket;
            } else if (ctrl == MENU_CTRL_LIGHT) {
                Socket *socket = SocketGet(
                    json_string_value(json_object_get(ext_value, "light"))
                );

                if (socket == NULL) {
                    LogF(LOG_TYPE_ERROR, "CONFIGS", "Unknown menu light");
                    return false;
                }

                value->light.sock = socket;
            }

            MenuValueAdd(level, value);
        }

        MenuLevelAdd(level);
    }

    json_decref(data);
    return true;
}

static bool ScenarioRead(const char *path)
{
    char            full_path[STR_LEN];
    json_error_t    error;
    size_t          index;
    json_t          *value;

    snprintf(full_path, STR_LEN, "%s%s", path, CONFIGS_SCENARIO_FILE);

    json_t *data = json_load_file(full_path, 0, &error);
    if (!data) {
        return false;
    }

    json_array_foreach(json_object_get(data, "scenario"), index, value) {
        Scenario *scenario = (Scenario *)malloc(sizeof(Scenario));

        scenario->unit = json_integer_value(json_object_get(value, "unit"));

        if (!strcmp(json_string_value(json_object_get(value, "type")), "inhome")) {
            scenario->type = SCENARIO_IN_HOME;
        } else if (!strcmp(json_string_value(json_object_get(value, "type")), "outhome")) {
            scenario->type = SCENARIO_OUT_HOME;
        } else {
            Log(LOG_TYPE_ERROR, "CONFIGS", "Invalid scenario type");
            return false;
        }

        if (!strcmp(json_string_value(json_object_get(value, "ctrl")), "socket")) {
            json_t *jsocket = json_object_get(value, "socket");
            strncpy(scenario->socket.name, json_string_value(json_object_get(jsocket, "name")), SHORT_STR_LEN);
            scenario->socket.status = json_boolean_value(json_object_get(jsocket, "status"));
            scenario->ctrl = SECURITY_CTRL_SOCKET;
        }

        ScenarioAdd(scenario);
    }

    json_decref(data);
    return true;
}

/*********************************************************************/
/*                                                                   */
/*                          PUBLIC FUNCTIONS                         */
/*                                                                   */
/*********************************************************************/

bool ConfigsRead(const char *path)
{
    ConfigsFactory factory;

    if (!FactoryRead(path, &factory)) {
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load Factory configs");
        return false;
    }

    if (!BoardRead(path, &factory)) {
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load Board configs");
        return false;
    }

    if (!ControllersRead(path)) {
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load Controllers configs");
        return false;
    }

    if (!PlcRead(path)) {
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load PLC configs");
        return false;
    }

    if (!ScenarioRead(path)) {
        Log(LOG_TYPE_ERROR, "CONFIGS", "Failed to load Scenario configs");
        return false;
    }

    return true;
}
