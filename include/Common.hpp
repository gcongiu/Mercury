/*
 * Copyright (c) 2013-2015 Seagate Technology.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _COMMON_H
#define _COMMON_H

#include <iostream>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <cstdlib>
#include <iostream>

// log4cpp
#include <log4cpp/Category.hh>
#include <log4cpp/Appender.hh>
#include <log4cpp/OstreamAppender.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/Layout.hh>
#include <log4cpp/BasicLayout.hh>
#include <log4cpp/Priority.hh>
#include <log4cpp/NDC.hh>
#include <log4cpp/PropertyConfigurator.hh>

extern "C"
{
        #include <stdio.h>
        #include <stdarg.h>
        #include <unistd.h>
        #include <fcntl.h>
        #include <stdint.h>
        #include <sys/stat.h>
        #include <sys/types.h>
        #include <sys/time.h>

        // Mutex
        #include <pthread.h>
        #include <assert.h>

        // dirname
        #include <libgen.h>
        #include <limits.h>

        // UNIX Sockets
        #include <sys/socket.h>
        #include <sys/un.h>

        // Errors
        #include <error.h>
        #include <errno.h>
}

// log4cpp defines
#define CODE_LOCATION __FILE__
#define LOG_EMERG(__logstream)  __logstream << log4cpp::Priority::EMERG << CODE_LOCATION
#define LOG_ALERT(__logstream)  __logstream << log4cpp::Priority::ALERT << CODE_LOCATION
#define LOG_CRIT(__logstream)  __logstream << log4cpp::Priority::CRIT << CODE_LOCATION
#define LOG_ERROR(__logstream)  __logstream << log4cpp::Priority::ERROR << CODE_LOCATION
#define LOG_WARN(__logstream)  __logstream << log4cpp::Priority::WARN << CODE_LOCATION
#define LOG_NOTICE(__logstream)  __logstream << log4cpp::Priority::NOTICE << CODE_LOCATION
#define LOG_INFO(__logstream)  __logstream << log4cpp::Priority::INFO << CODE_LOCATION
#define LOG_DEBUG(__logstream)  __logstream << log4cpp::Priority::DEBUG << CODE_LOCATION

// other defines
#define MSGSIZE 512     /* characters */
#define MAXPATHSIZE 128 /* characters */
#define KB(x) (x << 10)
#define MB(x) (x << 20)

#ifndef unlikely
#define unlikely(x) __builtin_expect(!!(x),0)
#endif
#ifndef likely
#define likely(x) __builtin_expect(!!(x),1)
#endif

typedef long long offset_t;
#endif
