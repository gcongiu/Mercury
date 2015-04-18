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

#include "Register.hpp"

/**
 * RegisterLog Class default constructor
 *
 */
mercury::RegisterLog::RegisterLog( ) {}

/**
 * RegisterLog Class default destructor
 *
 */
mercury::RegisterLog::~RegisterLog( )
{
        //std::map<key_t, mercury::AdvisorThread*>::iterator it = pathnameMap_.begin( );
        //for( ; it != pathnameMap_.end( ); it++ )
        //        if( it->second )
        //                delete it->second;
}

/**
 * Register a new process
 *
 * @param key is the key made of the pid of the process and the fd of the file
 * @param at is the pointer to the new advisor thread that will handle new requests
 *
 */
void mercury::RegisterLog::registerProcess( key_t key, mercury::AdvisorThread * at )
{
        /* register the new process with pathname */
        pathnameMap_.insert( std::make_pair( key, at ) );
}

/**
 * Register a new process
 *
 * @param key is the key made of the fd of the file
 * @param at is the pointer to the new advisor thread that will handle new requests
 *
 */
void mercury::RegisterLog::registerProcess( int fd, mercury::AdvisorThread * at )
{
        /* register the new process with pathname */
        registerProcess( key_t( getpid( ), fd ), at );
}

/**
 * Unregister a process
 *
 * @param key is the key made of the pid of the process and the fd of the file
 *
 */
void mercury::RegisterLog::unregisterProcess( key_t key )
{
        std::map<key_t, mercury::AdvisorThread*>::iterator it = pathnameMap_.find( key );
        if( it != pathnameMap_.end( ) )
        {
                /* destroy advisor thread */
                delete it->second;

                /* delete advisor thread */
                pathnameMap_.erase( key );
        }
}

/**
 * Unregister a process
 *
 * @param key is the key made of the fd of the file
 *
 */
void mercury::RegisterLog::unregisterProcess( int fd )
{
        unregisterProcess( key_t( getpid( ), fd ) );
}

/**
 * Lookup a key in the register log
 *
 * @param key is the key made of the pid of the process and the fd of the file
 * @return true if the key is in the register log, false otherwise
 *
 */
bool mercury::RegisterLog::lookup( key_t key )
{
        std::map<key_t, mercury::AdvisorThread*>::iterator it = pathnameMap_.find( key );
        return( it != pathnameMap_.end( ) ) ? true : false;
}

/**
 * Lookup a key in the register log
 *
 * @param key is the key made of the fd of the file
 * @return true if the key is in the register log, false otherwise
 *
 */
bool mercury::RegisterLog::lookup( int fd )
{
        return lookup( key_t( getpid( ), fd ) );
}

/**
 * Return the pointer to the AdvisorThread object corresponding to key
 *
 * @param key is the key made of the pid of the process and the fd of the file
 *
 */
mercury::AdvisorThread * mercury::RegisterLog::getAdvisorThread( key_t key )
{
        return pathnameMap_[key];
}

/**
 * Return the pointer to the AdvisorThread object corresponding to key
 *
 * @param key is the key made of the pid of the process and the fd of the file
 *
 */
mercury::AdvisorThread * mercury::RegisterLog::getAdvisorThread( int fd )
{
        return getAdvisorThread( key_t( getpid( ), fd ) );
}
