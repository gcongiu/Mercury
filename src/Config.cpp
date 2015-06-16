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

#include "Config.hpp"
#include <cstdio>
#include <fstream> // NOLINT
#include <string.h>
#include <limits>

/* the following is needed in SAIO because config is a static value of type Json::Value */
Json::Value mercury::Config::config_ __attribute__(( init_priority( 65534 ) ));
std::map<std::string, mercury::Config *> mercury::Config::configMap_;

/**
 * Default Config Class constructor
 *
 */
mercury::Config::Config( ) {}

/**
 * Default Config Class destructor
 *
 */
mercury::Config::~Config( ) {}

/**
 * Initialization function for the Config Singleton Class
 *
 * @param fname is the full pathname of the json configuration file
 *
 */
bool mercury::Config::init( std::string fname )
{
        Json::Reader reader;
        std::ifstream config_file( fname.c_str( ), std::ifstream::binary );
        bool rv = true;

        if( !config_file.is_open( ) )
        {
                std::cerr << "Failed to open " << fname.c_str( ) << ":" << strerror( errno ) << std::endl;
                return false;
        }

        if( !reader.parse( config_file, config_, false ) )
        {
                std::cerr << "Error parsing the config file: " << fname << std::endl;
                return false;
        }

        return rv;
}

/**
 * Return the instance for the Config Class corresponding to pathname
 *
 * @param path is the full pathname of the file
 *
 */
mercury::Config & mercury::Config::getInstance( const std::string & path )
{
        unsigned int i, j;
        char * basedir, * fname;
        mercury::willneed_t willneed;
        mercury::iopat_t iopat;
        Json::Value nil, record, value;
        Json::Value::Members members, submembers;
        std::string type, pathname;

        /* instanciate a new Config object for path if not existing */
        if( configMap_.count( path ) == 0 )
                configMap_[path] = new mercury::Config( );
        else
                return *configMap_[path];

        fname = strdup( path.c_str( ) );
        basedir = dirname( fname );

        /* member are File and Directory */
        members = config_.getMemberNames( );

        /* there must be something in the config file */
        if( members.size( ) == 0 )
        {
                std::cerr << "Error: no members in config file." << std::endl;
                exit( EXIT_FAILURE );
        }

        for( i = 0; i < members.size( ); i++ )
        {
                /* type can be either a "File" or "Directory" */
                type = members[i];
                if( !type.compare( "File" ) || !type.compare( "Directory" ) )
                {
                        /* record is the content of a "File"/"Directory" section */
                        record = config_[type].get( i, nil );

                        /* get pathname */
                        pathname = record["Path"].asString( );

                        /* submembers are hints type */
                        submembers = record.getMemberNames( );

                        if( !pathname.compare( path ) || !pathname.compare( std::string( basedir ) ) )
                        {
                                //std::cerr << "\"Path\": " << path << std::endl;

                                /* init block cache parameters to default */
                                configMap_[path]->blockSize_ = 0;
                                configMap_[path]->cacheSize_ = 0;
                                configMap_[path]->readAheadSize_ = 0;

                                for( j = 0; j < submembers.size( ); j++ )
                                {
                                        /* type can be: "WillNeed", "Sequential", etc */
                                        type = submembers[j];

                                        if( !type.compare( "WillNeed" ) )
                                        {
                                                value = record[type].get( j, nil );
                                                willneed.offset_ = (offset_t)value["Offset"].asDouble( );
                                                willneed.length_ = (offset_t)value["Length"].asDouble( );
                                                configMap_[path]->willneed_.push_back( willneed );
                                                //std::cerr << "\"" << type << "\": {" << std::endl;
                                                //std::cerr << "    Offset: " << value["Offset"].asDouble( ) << std::endl;
                                                //std::cerr << "    Length: " << value["Length"].asDouble( ) << std::endl;
                                                //std::cerr << "}" << std::endl;
                                        }
                                        else if( !type.compare( "Sequential" ) ||
                                                 !type.compare( "Normal" )     ||
                                                 !type.compare( "Random" ) )
                                        {
                                                value = record[type].get( j, nil );
                                                iopat.offset_ = (offset_t)value["Offset"].asDouble( );
                                                iopat.length_ = (offset_t)value["Length"].asDouble( );
                                                iopat.type_ = type;
                                                configMap_[path]->iopat_.push_back( iopat );
                                                //std::cerr << "\"" << type << "\": {" << std::endl;
                                                //std::cerr << "    Offset: " << value["Offset"].asDouble( ) << std::endl;
                                                //std::cerr << "    Length: " << value["Length"].asDouble( ) << std::endl;
                                                //std::cerr << "}" << std::endl;
                                        }
                                        else if( !type.compare( "BlockSize" ) )
                                        {
                                                configMap_[path]->blockSize_ = (offset_t)record[type].asDouble( );
                                                //std::cerr << "\"" << type << "\": " << record[type].asDouble( ) << std::endl;
                                        }
                                        else if( !type.compare( "CacheSize" ) )
                                        {
                                                configMap_[path]->cacheSize_ = (int)record[type].asInt( );
                                                //std::cerr << "\"" << type << "\": " << record[type].asInt( ) << std::endl;
                                        }
                                        else if( !type.compare( "ReadAheadSize" ) )
                                        {
                                                configMap_[path]->readAheadSize_ = (int)record[type].asInt( );
                                                //std::cerr << "\"" << type << "\": " << record[type].asInt( ) << std::endl;
                                        }
                                }
                                goto fn_exit;
                        }
                }
        }
fn_exit:
        /* free memory */
        free( fname );
        return *configMap_[path];
}

/**
 * shutdown the config singleton class
 *
 */
void mercury::Config::shutdown( )
{
        std::map<std::string, mercury::Config *>::iterator it;

        for( it = configMap_.begin( ); it != configMap_.end( ); it++ )
                it->second->release( );
}

/**
 * release the config object
 *
 */
void mercury::Config::release( )
{
        std::string pathname = getPathname( );

        delete this;

        //configMap_.erase( pathname );
}

/**
 * Return the hint for the current request
 *
 * @param offset is the offset of the current request
 * @param len is the length of the current request
 *
 */
std::string mercury::Config::getHintType( offset_t offset )
{
        std::string hint = std::string( "Nop" );
        std::string pathname;
        std::vector<mercury::willneed_t>::iterator wit;
        std::vector<mercury::iopat_t>::iterator iit;
        offset_t length;

        /* get pathname */
        pathname = getPathname( );

        wit = configMap_[pathname]->willneed_.begin( );
        iit = configMap_[pathname]->iopat_.begin( );

        for( ; wit != configMap_[pathname]->willneed_.end( ); wit++ )
        {
                length = ( !wit->length_ ) ?
                        std::numeric_limits<offset_t>::max( ) - wit->offset_ :
                        wit->length_;

                if( offset >= wit->offset_ && offset < wit->offset_ + length )
                {
                        hint = "WillNeed";
                        break;
                }
        }

        for( ; iit != configMap_[pathname]->iopat_.end( ); iit++ )
        {
                length = ( !iit->length_ ) ?
                        std::numeric_limits<offset_t>::max( ) - iit->offset_ :
                        iit->length_;

                if( offset >= iit->offset_ && offset < iit->offset_ + length )
                {
                        hint = iit->type_;
                        break;
                }
        }

        return hint;
}

/**
 * Return the vector containing the WillNeed information contained in the configuration file for pathname
 *
 * @param offset is the offset of the request
 * @param len is the length of the request
 * @param willneed is the vector containing the hint information
 *
 */
void mercury::Config::getWillneedInfo( offset_t offset, offset_t len, std::vector<mercury::willneed_t> & willneed )
{
        std::vector<mercury::willneed_t>::iterator it;
        std::string pathname;
        offset_t length;

        pathname = getPathname( );

        it = configMap_[pathname]->willneed_.begin( );

        for( ; it != configMap_[pathname]->willneed_.end( ); it++ )
        {
                length = ( !it->length_ ) ?
                        std::numeric_limits<offset_t>::max( ) - it->offset_ :
                        it->length_;

                if( offset >= it->offset_ && offset < it->offset_ + length)
                {
                        /* return all the willneed sections from here on */
                        while( it != configMap_[pathname]->willneed_.end( ) )
                        {
                                willneed.push_back( *it );
                                it++;
                        }
                        break;
                }
        }
}

/**
 * Return the size of the cache in number of blocks
 *
 * @return the cache size
 *
 */
unsigned int mercury::Config::getCacheSize( )
{
        std::string pathname;

        pathname = getPathname( );

        return configMap_[pathname]->cacheSize_;
}

/**
 * Return the block size for the cache
 *
 * @return the block size
 *
 */
unsigned int mercury::Config::getBlockSize( )
{
        std::string pathname;

        pathname = getPathname( );

        return configMap_[pathname]->blockSize_;
}

/**
 * Return the read ahead size in blocks
 *
 * @return the read ahead size
 *
 */
unsigned int mercury::Config::getReadAheadSize( )
{
        std::string pathname;

        pathname = getPathname( );

        return configMap_[pathname]->readAheadSize_;
}

/**
 * Return the pathname for the current Config instance
 *
 * @return a string containing the full pathname for the file
 *
 */
std::string mercury::Config::getPathname( )
{
        std::map<std::string, mercury::Config *>::iterator it;

        it = configMap_.begin( );

        while( it->second != this )
                it++;

        return it->first;
}

/**
 * Return all the pathnames present in the config file
 *
 * @return a list of pathnames
 *
 */
bool mercury::Config::getAllPaths( std::list<std::string> * pathnames )
{
        unsigned int i;
        Json::Value value, nil;
        std::string file, type;
        Json::Value::Members members = config_.getMemberNames( );

        if( !members.size( ) )
        {
                std::cerr << "No members in config file." << std::endl;
                return false;
        }

        for( i = 0; i < members.size( ); i++ )
        {
                type = members[i];
                value = config_[type].get( i, nil );
                if( !type.compare( "File" ) )
                {
                        file = value["Path"].asString( );
                        pathnames->push_back( file );
                }
                else if( !type.compare( "Directory" ) )
                {
                        file = value["Path"].asString( );
                        pathnames->push_back( file );
                }
                else
                {
                        std::cerr << "error no path or directory members in config." << std::endl;
                        return false;
                }
        }
        return true;
}
