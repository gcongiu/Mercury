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

#ifndef _REGISTER_H
#define _REGISTER_H

#include "Common.hpp"
#include "AdvisorThread.hpp"

namespace mercury {

        /* key to access registered process: <pid, fd> */
        typedef std::pair <pid_t, int> key_t;

        class RegisterLog
        {
                public:
                        /* constructor */
                        RegisterLog( );

                        /* destructor */
                        ~RegisterLog( );

                        /* register the process with key = <pid, fd> */
                        void registerProcess( key_t key, mercury::AdvisorThread * at );

                        /* register the process with key = fd */
                        void registerProcess( int fd, mercury::AdvisorThread * at );

                        /* unregister the process with key = <pid, fd> */
                        void unregisterProcess( key_t key );

                        /* unregister the process with key = fd> */
                        void unregisterProcess( int fd );

                        /* return true if the process with key is already registered */
                        bool lookup( key_t key );

                        /* return true if the process with key is already registered */
                        bool lookup( int fd );

                        /* return the advisor thread instance for key */
                        mercury::AdvisorThread * getAdvisorThread( key_t key );

                        /* return the advisor thread instance for key */
                        mercury::AdvisorThread * getAdvisorThread( int fd );

                private:
                        /* key_t is used to access the pathname of the file */
                        std::map <key_t, mercury::AdvisorThread *> pathnameMap_;
        };
} //end of namespace
#endif
