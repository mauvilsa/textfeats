/**
 * Logging functions
 *
 * @version $Version: 2017.05.30$
 * @copyright Copyright (c) 2004-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#include "log.h"

#include <stdarg.h>
#include <string.h>

void _proxy_log(FILE *log_file, int log_level, int level, const char *fmt, ...) {
  va_list arg;

  if( ( ! log_level ) || level > log_level )
    return;

  va_start(arg, fmt);
  vfprintf(log_file, fmt, arg);
  va_end(arg);
}
