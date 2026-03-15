
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include <task_check_info.h>
#include "global.h"

// extern void handleWebSocketMessage(String message);
extern void handleRoot();
extern void handleToggle();
extern void handleSettingPage();
extern void handleConnect();

#endif