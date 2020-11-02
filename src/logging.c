/*
 * This file is part of Moonlight Embedded.
 *
 * Copyright (C) 2020 TheChoconut
 *
 * Moonlight is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * Moonlight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Moonlight; if not, see <http://www.gnu.org/licenses/>.
 */

#include "logging.h"
#include "util.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

FILE * log_file;

void initialize_log() {
    log_file = fopen("moonlight.log", "w");
    if (log_file == NULL) {
        fprintf(stderr, " Can't open configuration file: %s\n", "moonlight.log");
        exit(1);
    }
    _moonlight_log(INFO, "Logging file has been enabled!\n");
}


void _moonlight_log(enum LOG_TYPE type, char* format, ...) {
    va_list args;

    FILE * file = (log_file == NULL) ? stderr : log_file;
    fprintf(file, "[%s] ", log_type_str[type]);

    va_start(args, format);
    vfprintf(file, format, args);
    va_end(args);
}

void close_log() {
    if (log_file != NULL)
    	fclose(log_file);
}