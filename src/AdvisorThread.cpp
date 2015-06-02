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

#include "AdvisorThread.hpp"
#include <pcrecpp.h>

/**
 * Create an instance of the AdvisorThread Class for a new registered process
 *
 * @param fd is the file descriptor of the open file
 * @param pathname is the full pathname of the open file
 * @return a new instance of the AdvisorThread Class
 *
 */
mercury::AdvisorThread::AdvisorThread( int fd, std::string pathname ) :
        fd_( fd ),
        pathname_( pathname ),
        blockCache_( mercury::BlockCache::getInstance( pathname ) )
{
	pthread_attr_t attr;
	pthread_attr_init( &attr );
	pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
	mercury::AdvisorThread( fd, pathname, attr );
	pthread_attr_destroy( &attr );	
}

/**
 * Create an instance of the AdvisorThread Class for a new registered process
 *
 * @param fd is the file descriptor of the open file
 * @param pathname is the full pathname of the open file
 * @param attr are the external pthread attributes to be set
 * @return a new instance of the AdvisorThread Class
 *
 */
mercury::AdvisorThread::AdvisorThread( int fd, std::string pathname, pthread_attr_t attr ) :
        fd_( fd ),
        pathname_( pathname ),
        blockCache_( mercury::BlockCache::getInstance( pathname ) ),
        attr_( attr )
{
        thread_ = ( pthread_t * )malloc( sizeof( pthread_t ) );
        queue_ = new mercury::AtomicQueue<std::string>( );

        /**
         * configuration of pathname by mercury::Config::getInstance( )
         * is done in mercury::BlockCache::getInstance( )
         */

        /* set fd in block cache */
        blockCache_.setFileDescriptor( fd );

        /* initialize hint map */
        hintMap_["Sequential"] = POSIX_FADV_SEQUENTIAL;
        hintMap_["Random"] = POSIX_FADV_RANDOM;
        hintMap_["Normal"] = POSIX_FADV_NORMAL;
        hintMap_["WillNeed"] = POSIX_FADV_WILLNEED;
        hintMap_["DontNeed"] = POSIX_FADV_DONTNEED;
        hintMap_["Nop"] = NOP;

        /* launch generalized hint routine */
        pthread_create( thread_, &attr_, run_generalized_hint_routine, this );
}

/**
 * Destroy the AdvisorThread instance previously allocated
 *
 */
mercury::AdvisorThread::~AdvisorThread( )
{
        int state;

        /* first get rid of all pending requests */
        while( queue_->size( ) > 0 )
                queue_->pop( );

        /* send a cancel message to the advisor thread */
        queue_->push( std::string( "stop thread" ) );

        /* get the type of thread */
        pthread_attr_getdetachstate( &attr_, &state );

        /* if the thread is joinable wait here */
        if( state == PTHREAD_CREATE_JOINABLE )
        {
                /* this is used in. e.g., libSAIO */
                pthread_join( *thread_, NULL );
        }

        /* free thread */
        free( thread_ );

        /* destroy pthread attr */
        pthread_attr_destroy( &attr_ );
}

/**
 * Enqueue a new string message for the AdvisorThread instance
 *
 * @param message is the string containing the new message
 *
 */
void mercury::AdvisorThread::enqueue( std::string & message )
{
        queue_->push( message );
}

/**
 * The pthread routine that processes messages received by the Requests Manager
 *
 */
void mercury::AdvisorThread::generalized_hint_routine( )
{
        std::string operation, body, hint, pathname;
        std::vector<willneed_t>::iterator pit;
        std::vector<iopat_t>::iterator bit;
        mercury::AtomicQueue<std::string> * queue = queue_;
        mercury::BlockCache &blockCache = blockCache_;
        int posix_fadvise_state = POSIX_FADV_NORMAL, ret, fd;
        offset_t offset, length;
        char tmp_message[64];

        /* to cancel the thread */
        pthread_setcancelstate( PTHREAD_CANCEL_DISABLE, NULL );

        const pcrecpp::RE regex_command( "([A-Za-z]+)\\s+(.+)" );
        const pcrecpp::RE regex_read( "(\\d+)\\s+(\\d+)" );

        pathname = pathname_; fd = fd_;

        while( true )
        {
                memcpy( tmp_message, (queue->front( )).c_str( ), 64 );
                queue->pop( );

                /* for now just accept reads from coordinator: operation = Read */
                regex_command.FullMatch( tmp_message, &operation, &body );

                /* received cancel operation */
                if( !operation.compare( std::string( "stop" ) ) )
                {
                        /* release block cache */
                        blockCache.release( );

                        /* delete queue */
                        delete queue;

                        /* close fd_ */
                        close( fd );

                        return;
                }

                regex_read.FullMatch( body.c_str( ), &offset, &length );
                log4cpp::Category::getInstance( "mercury" ).debug(
                        "From(%s,%s,%d,%0x): Operation: %s, fd: %d, offset: %lli, length: %lli",
                        __FILE__,
                        __func__,
                        __LINE__,
                        pthread_self( ),
                        operation.c_str( ),
                        fd,
                        offset,
                        length );

                /* get hint type */
                hint = mercury::Config::getInstance( pathname ).getHintType( offset );

                /* NOTE: for gpfs and lustre sequential/normal/random have no effect */
                switch( hintMap_[hint] )
                {
                        case POSIX_FADV_SEQUENTIAL:
                        {
                                if( posix_fadvise_state != POSIX_FADV_SEQUENTIAL )
                                {
                                        posix_fadvise( fd, 0, 0, POSIX_FADV_SEQUENTIAL );
                                        posix_fadvise_state = POSIX_FADV_SEQUENTIAL;
                                }
                                break;
                        }
                        case POSIX_FADV_RANDOM:
                        {
                                if( posix_fadvise_state != POSIX_FADV_RANDOM )
                                {
                                        posix_fadvise( fd, 0, 0, POSIX_FADV_RANDOM );
                                        posix_fadvise_state = POSIX_FADV_RANDOM;
                                }
                                break;
                        }
                        case POSIX_FADV_NORMAL:
                        {
                                if( posix_fadvise_state != POSIX_FADV_NORMAL )
                                {
                                        posix_fadvise( fd, 0, 0, POSIX_FADV_NORMAL );
                                        posix_fadvise_state = POSIX_FADV_NORMAL;
                                }
                                break;
                        }
                        case POSIX_FADV_WILLNEED:
                        {
                                ret = blockCache.getBlock( offset, length );
                                if( ret == -1 )
                                {
                                        log4cpp::Category::getInstance( "mercury" ).warn(
                                                "From(%s,%s,%d,%0x): Ignoring big request (%lli, %lli).",
                                                __FILE__,
                                                __func__,
                                                __LINE__,
                                                pthread_self( ),
                                                offset,
                                                length );
                                }
                                break;
                        }
                        case POSIX_FADV_DONTNEED:
                        case NOP:
                        {
                                break;
                        }
                        default:
                                log4cpp::Category::getInstance( "mercury" ).warn(
                                        "From(%s,%s,%d,%0x): Unrecognized command.",
                                        __FILE__,
                                        __func__,
                                        __LINE__,
                                        pthread_self( ) );
                }
        }
}
