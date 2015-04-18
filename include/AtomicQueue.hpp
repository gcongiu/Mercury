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

#ifndef __ATOMIC_QUEUE_H
#define __ATOMIC_QUEUE_H

//#include <mutex>
#include <pthread.h>
#include <queue>
#include <iostream>

namespace mercury {

        template <typename T>
        class AtomicQueue
        {
                public:
                AtomicQueue( ) : mtx_( ( pthread_mutex_t * )malloc( sizeof( pthread_mutex_t ) ) ),
                                 ready_( ( pthread_cond_t * )malloc( sizeof( pthread_cond_t ) ) )
                {
                        pthread_mutex_init( mtx_, NULL );
                        pthread_cond_init( ready_, NULL );
                }

                ~AtomicQueue( )
                {
                        pthread_mutex_destroy( mtx_ );
                        pthread_cond_destroy( ready_ );
                        free( mtx_ );
                        free( ready_ );
                }

                bool empty( )
                {
                        pthread_mutex_lock( mtx_ );
                        bool test = queue_.empty( );
                        pthread_mutex_unlock( mtx_ );
                        return test;
                }

                size_t size( )
                {
                        pthread_mutex_lock( mtx_ );
                        size_t size = queue_.size( );
                        pthread_mutex_unlock( mtx_ );
                        return size;
                }

                T & front( )
                {
                        pthread_mutex_lock( mtx_ );
                        while( queue_.empty( ) )
                                pthread_cond_wait( ready_, mtx_ );
                        T & tmp = queue_.front( );
                        pthread_mutex_unlock( mtx_ );
                        return tmp;
                }

                const T & front( ) const
                {
                        pthread_mutex_lock( mtx_ );
                        while( queue_.empty( ) )
                                pthread_cond_wait( ready_, mtx_ );
                        const T & tmp = queue_.front( );
                        pthread_mutex_unlock( mtx_ );
                        return tmp;
                }

                T & back( )
                {
                        pthread_mutex_lock( mtx_ );
                        while( queue_.empty( ) )
                                pthread_cond_wait( ready_, mtx_ );
                        T & tmp = queue_.back( );
                        pthread_mutex_unlock( mtx_ );
                        return tmp;
                }

                const T & back( ) const
                {
                        pthread_mutex_lock( mtx_ );
                        while( queue_.empty( ) )
                                pthread_cond_wait( ready_, mtx_ );
                        const T & tmp = queue_.back( );
                        pthread_mutex_unlock( mtx_ );
                        return tmp;
                }

                void push( const T & val )
                {
                        pthread_mutex_lock( mtx_ );
                        bool test = queue_.empty( );
                        queue_.push( val );
                        pthread_mutex_unlock( mtx_ );
                        if( test ) pthread_cond_signal( ready_ );
                        return;
                }

                void push( T & val )
                {
                        pthread_mutex_lock( mtx_ );
                        bool test = queue_.empty( );
                        queue_.push( val );
                        pthread_mutex_unlock( mtx_ );
                        if( test ) pthread_cond_signal( ready_ );
                        return ;
                }

                void pop( )
                {
                        pthread_mutex_lock( mtx_ );
                        if( !queue_.empty( ) )
                                queue_.pop( );
                        pthread_mutex_unlock( mtx_ );
                        return ;
                }

                private:
                        pthread_mutex_t * mtx_;
                        pthread_cond_t * ready_;
                        std::queue <T> queue_;
        };
}// end of namespace
#endif
