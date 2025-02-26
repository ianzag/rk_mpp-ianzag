/*
 * Copyright 2015 Rockchip Electronics Co. LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * all os log function will provide two interface
 * os_log and os_err
 * os_log for general message
 * os_err for error message
 */

#ifndef __OS_LOG_H__
#define __OS_LOG_H__

#include <stdarg.h>

typedef void (*os_log_callback)(const char*, const char*, va_list);

#ifdef __cplusplus
extern "C" {
#endif

void os_log_trace(const char* tag, const char* msg, va_list list);
void os_log_debug(const char* tag, const char* msg, va_list list);
void os_log_info (const char* tag, const char* msg, va_list list);
void os_log_warn (const char* tag, const char* msg, va_list list);
void os_log_error(const char* tag, const char* msg, va_list list);
void os_log_fatal(const char* tag, const char* msg, va_list list);

#ifdef __cplusplus
}
#endif

#endif /*__OS_LOG_H__*/

