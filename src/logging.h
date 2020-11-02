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

#define FOREACH_LOG_TYPE(LOG_TYPE) \
        LOG_TYPE(ERR)   \
        LOG_TYPE(WARN)  \
        LOG_TYPE(INFO)   \
        LOG_TYPE(DEBUG)  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum LOG_TYPE { ERR, WARN, INFO, DEBUG };

static const char *log_type_str[] = {
    FOREACH_LOG_TYPE(GENERATE_STRING)
};

void initialize_log();
void close_log();
void _moonlight_log(enum LOG_TYPE type, char* format, ...);