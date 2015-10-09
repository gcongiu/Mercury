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

#include "AdviceManager.hpp"
#include <pcrecpp.h>

/**
 * Create an instance of the AdviceManager Class
 *
 * @param reg is the pointer to the RegisterLog instance
 * @return the pointer to the new instance
 *
 */
mercury::AdviceManager::AdviceManager( RegisterLog * reg ) :
        register_( reg )
{
        commandMap_["Register"] = Register;
        commandMap_["Unregister"] = Unregister;
        commandMap_["Read"] = Read;
        commandMap_["Write"] = Write;
        commandMap_["Configure"] = Configure;

        addr_.sun_family = AF_UNIX;
        strcpy( addr_.sun_path, "/tmp/channel" );

        if( (sockfd_ = socket( AF_UNIX, SOCK_DGRAM, 0 )) == -1 )
        {
                log4cpp::Category::getInstance( "mercury" ).error(
                        "From(%s,%s,%d): Failed with error: %s while creating socket!",
                        __FILE__,
                        __func__,
                        __LINE__,
                        strerror( errno ) );
                std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                exit( EXIT_FAILURE );
        }

        unlink( addr_.sun_path );

        if( (bind( sockfd_, (struct sockaddr *)&addr_, sizeof( addr_.sun_family ) + sizeof( addr_.sun_path ) )) == -1 )
        {
                log4cpp::Category::getInstance( "mercury" ).error(
                        "From(%s,%s,%d): Failed with error: %s while connecting to socket!",
                        __FILE__,
                        __func__,
                        __LINE__,
                        strerror( errno ) );
                std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                exit( EXIT_FAILURE );
        }

        /* initialize control message */
        memset( &msg_, 0, sizeof( struct msghdr ) );
        msg_.msg_name = &addr_;
        msg_.msg_namelen = sizeof( addr_.sun_family ) + sizeof( addr_.sun_path );
        msg_.msg_control = fdesc_;
        msg_.msg_controllen = sizeof( fdesc_ );
        iovec_.iov_base = message_;
        iovec_.iov_len = sizeof( message_ );
        msg_.msg_iov = &iovec_;
        msg_.msg_iovlen = 1;
}

/**
 * Destroy the AdviceManager instance previously allocated
 *
 */
mercury::AdviceManager::~AdviceManager( )
{
        close( sockfd_ );
}

/**
 * Run the Requests Manager which handles requests coming from AIO Lib
 *
 */
void mercury::AdviceManager::run( )
{
        int    pfd;                          // remote process file descriptor
        int    lfd;                          // temporary local file descriptor
        pid_t  pid;                          // process id for register/unregister/read/write
        offset_t offset, count;              // read offset and count
        std::string command, body, pathname; // request command and body
        std::string tmp_message;             // temporary string used to build the message for advisor thread

        /* regular expressions for pattern matching */
        const pcrecpp::RE regex_command( "(\\w+)\\s+(.+)" );
        const pcrecpp::RE regex_register( "(\\d+)\\s+(.+)\\s+(\\d+)" );
        const pcrecpp::RE regex_unregister( "(\\d+)\\s+(\\d+)" );
        const pcrecpp::RE regex_read( "(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)" );
        const pcrecpp::RE regex_write( "(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)" );

        while( true )
        {
                /*TODO: find a better way to handle recv errors */
                memset( &message_, 0, sizeof( message_ ) );
                if( recvmsg( sockfd_, &msg_, 0 ) == -1 )
                {
                        log4cpp::Category::getInstance( "mercury" ).error(\
                                "From(%s,%s,%d): Error receiving message: %s.",
                                __FILE__,
                                __func__,
                                __LINE__,
                                strerror( errno ) );
                        std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                        exit( EXIT_FAILURE );
                }

                if( msg_.msg_controllen != 0 && (cmsg_ = CMSG_FIRSTHDR( &msg_ )) == NULL )
                {
                        log4cpp::Category::getInstance( "mercury" ).error(
                                "From(%s,%s,%d): Received empty message!",
                                __FILE__,
                                __func__,
                                __LINE__ );
                        std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                        exit (EXIT_FAILURE);
                }

                if( msg_.msg_controllen != 0 && cmsg_->cmsg_type != SCM_RIGHTS )
                {
                        log4cpp::Category::getInstance( "mercury" ).error(
                                "From(%s,%s,%d): Received control message of unknown type!",
                                __FILE__,
                                __func__,
                                __LINE__ );
                        std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                        exit( EXIT_FAILURE );
                }

                log4cpp::Category::getInstance( "mercury" ).debug(
                        "From(%s,%s,%d): New message from IOLib: %s.",
                        __FILE__,
                        __func__,
                        __LINE__,
                        message_ );

                /* lookup the command type */
                if( !strncmp( message_, "Shutdown", 8 ) )
                        break;

                regex_command.FullMatch( message_, &command, &body );

                switch( commandMap_[command] )
                {
                        case Register:
                        {
                                /* register only pathnames that match the config rules */
                                regex_register.FullMatch( body.c_str( ), &pid, &pathname, &pfd );
                                if( !register_->lookup( key_t( pid, pfd ) ) )
                                {
                                        /* get local file descriptor */
                                        lfd = *( int * )CMSG_DATA( cmsg_ );

                                        /* new value to register: advisor thread */
                                        mercury::AdvisorThread * value = new mercury::AdvisorThread( lfd, pathname );

                                        /* register new process & related thread */
                                        register_->registerProcess( key_t( pid, pfd ), value );
                                }
                                break;
                        }
                        case Unregister:
                        {
                                regex_unregister.FullMatch( body.c_str( ), &pid, &pfd );
                                if( register_->lookup( key_t( pid, pfd ) ) )
                                {
                                        register_->unregisterProcess( key_t( pid, pfd ) );
                                }
                                break;
                        }
                        case Read:
                        {
                                regex_read.FullMatch( body.c_str( ), &pid, &pfd, &offset, &count );
                                if( register_->lookup( key_t( pid, pfd ) ) )
                                {
                                        sprintf( (char *)tmp_message.c_str( ), "Read %lli %lli", offset, count );

                                        /* enqueue tmp_message for processing in advisor thread */
                                        register_->getAdvisorThread( key_t( pid, pfd ) )->enqueue( tmp_message );
                                }
                                break;
                        }
                        case Write:
                        {
                                /* TODO: add support for write (GPFS) operations in rest of the code */
                                regex_write.FullMatch( body.c_str( ), &pid, &pfd, &offset, &count );
                                if( register_->lookup( key_t( pid, pfd ) ) )
                                {
                                        sprintf( (char * )tmp_message.c_str( ), "Write %lli %lli", offset, count );

                                        /* enqueue tmp_message for processing in advisor thread */
                                        register_->getAdvisorThread( key_t( pid, pfd ) )->enqueue( tmp_message );
                                }
                                break;
                        }
                        case Configure:
                        {
                                /* TODO: receive config file name from AIO Lib */
                                break;
                        }
                        default:
                                log4cpp::Category::getInstance( "mercury" ).error(
                                        "From(%s,%s,%d): Received unrecognized command!",
                                        __FILE__,
                                        __func__,
                                        __LINE__ );
                }
        }
}

int main( int argc, char **argv )
{
        // init logging
        std::string initFileName = "log4cpp.properties";
        log4cpp::PropertyConfigurator::configure( initFileName );
        log4cpp::Category &mercurylog = log4cpp::Category::getInstance( "mercury" );

        // init mercury
        mercury::AdviceManager * manager;
        mercury::RegisterLog * reg;
        char *config_file = NULL;

        /* read the configuration file pathname */
        if( (config_file = std::getenv( "MERCURY_CONFIG" )) != NULL )
        {
                mercurylog.info( "Reading config file." );
        }
        else
        {
                mercurylog.error( "No config file available. Type: 'export MERCURY_CONFIG=\"path_to_config\"'." );
                std::cerr << "## continuing without hints support, see memory.log for more details." << std::endl;
        }

        /* read the configuration file */
        if( config_file && !mercury::Config::init( config_file ) )
        {
                mercurylog.error( "Error while parsing the config file %s!", config_file );
                std::cerr << "## Exited with error, see mercury.log for more info." << std::endl;
                exit( EXIT_FAILURE );
        }

        mercurylog.info( "Config file: %s, read successfully.", config_file );

        /* initialize the registration class */
        reg = new mercury::RegisterLog( );

        /* create a new instance of the AdviceManager */
        manager = new mercury::AdviceManager( reg );

        /* run receiving messages from socket for new commands */
        manager->run( );

        /* shutdown config */
        mercury::Config::shutdown( );

        /* shutdown log4cpp */
        log4cpp::Category::shutdown( );

        /* free memory */
        if( config_file )
                free( config_file );
        delete manager;
        delete reg;

        return 0;
}
