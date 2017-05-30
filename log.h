/**
 * Logging functions
 *
 * @version $Version: 2017.05.30$
 * @copyright Copyright (c) 2004-present, Mauricio Villegas <mauricio_ville@yahoo.com>
 * @license MIT License
 */

#ifndef __MV_LOG_H__
#define __MV_LOG_H__

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef FAILURE
#define FAILURE 1
#endif
#ifndef SUCCESS
#define SUCCESS 0
#endif

#include <stdio.h>

void _proxy_log(FILE *log_file, int log_level, int level, const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));
#define proxy_log(log_file, log_level, level, fmt, ...) _proxy_log(log_file, log_level, level, fmt"\n", ##__VA_ARGS__)

#define logger( level, fmt, ... ) proxy_log( logfile, verbosity, level, "%s: " fmt, tool, ##__VA_ARGS__ )
#define die( fmt, ... ) { proxy_log( logfile, verbosity, 0, "%s: " fmt, tool, ##__VA_ARGS__ ); return FAILURE; }

#define strbool(cond) ( (cond) ? "true" : "false" )

#endif
