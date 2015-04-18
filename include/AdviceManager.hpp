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

#ifndef __AdviceManager_H
#define __AdviceManager_H

#include "Common.hpp"
#include "Config.hpp"
#include "Register.hpp"
#include "AdvisorThread.hpp"
#include <cstring>

namespace mercury {

        /* incoming commands */
        typedef enum {Register, Unregister, Read, Write, Configure} command_t;

        class AdviceManager
        {
                public:
                        /* constructors */
                        AdviceManager( RegisterLog * reg );

                        /* destructor */
                        ~AdviceManager( );

                        /* IPC support function (Request Manager) */
                        void run( );

                protected:
                        /* default constructor protected */
                        AdviceManager( );

                private:
                        /* register log to register new processes */
                        mercury::RegisterLog * register_;

                        /* map of recognized commands */
                        std::map <std::string, command_t> commandMap_;

                        /* control message and socket */
                        int                sockfd_; // socket descriptor
                        struct msghdr      msg_;    // control message
                        struct cmsghdr *   cmsg_;   // access control message
                        struct sockaddr_un addr_;   // address for the connection (file in the namespace)
                        struct iovec       iovec_;  // iovector carrying the command
                        char               message_[MSGSIZE];
                        char               fdesc_[CMSG_SPACE( sizeof( int ) )];
        };
}// end of namespace
#endif
