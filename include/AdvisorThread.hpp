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

#ifndef _ADVISOR_THREAD_H
#define _ADVISOR_THREAD_H

#include "Common.hpp"
#include "AtomicQueue.hpp"
#include "Config.hpp"
#include "BlockCache.hpp"

#define GPFS_SUPER_MAGIC 0x47504653
#define NOP 100

namespace mercury
{
        /* advisor thread information */
        class AdvisorThread
        {
                public:
                        /* constructor */
                        AdvisorThread( int fd, std::string pathname );

                        /* constructor with attr */
                        AdvisorThread( int fd, std::string pathname, pthread_attr_t attr );

                        /* destructor */
                        ~AdvisorThread( );

                        /* enqueue string for pthread instance */
                        void enqueue( std::string & message );

                        /* generalized hint routine */
                        void generalized_hint_routine( );

                protected:
                        /* run generalized hint thread instance */
                        static void *run_generalized_hint_routine( void *ptr )
                        {
                                (( mercury::AdvisorThread * )ptr)->generalized_hint_routine( );
                                pthread_exit( NULL );
                        }

                        /* default destructor */
                        AdvisorThread( );

                private:
                        /* file descriptor for access */
                        int fd_;

                        /* pathname */
                        std::string pathname_;

                        /* block cache */
                        mercury::BlockCache & blockCache_;

                        /* pthread attributes */
                        pthread_attr_t attr_;

                        /* pthread instance */
                        pthread_t * thread_;

                        /* advice map */
                        std::map<std::string, int> hintMap_;

                        /* communication queue with pthread instance */
                        mercury::AtomicQueue<std::string> * queue_;
        };
}
#endif
