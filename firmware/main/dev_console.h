/**
 * @file dev_console.h
 * @brief 개발용 USB 콘솔 (CONFIG_APP_DEV_CONSOLE)
 */
#pragma once

#include "esp_err.h"

/** 명령을 등록하고 USB-Serial-JTAG 줄 입력 태스크를 시작한다 */
esp_err_t dev_console_start(void);
