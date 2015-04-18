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

#ifndef _CONFIG_H
#define _CONFIG_H

#include <jsoncpp/json/json.h>
#include <string>
#include "Common.hpp"

namespace mercury {

        /* i/o pattern in the file: e.g. Sequential, Random, etc */
        typedef struct
        {
                offset_t    offset_;
                offset_t    length_;
                std::string type_;

        } iopat_t;

        /* prefetch for the file: i.e. WillNeed */
        typedef struct
        {
                offset_t offset_;
                offset_t length_;

        } willneed_t;

        class Config
        {
                public:
                        /* initialization */
                        static bool init( std::string fname );

                        /* get instance */
                        static mercury::Config & getInstance( const std::string & );

                        /* get pathnames */
                        static bool getAllPaths( std::list<std::string> * );

                        /* shutdown */
                        static void shutdown( );

                        /* release */
                        void release( );

                        /* return a string with the type of hint */
                        std::string getHintType( offset_t );

                        /* get willneed */
                        void getWillneedInfo( offset_t, offset_t, std::vector<mercury::willneed_t> & );

                        /* get cache size */
                        unsigned int getCacheSize( );

                        /* get block size */
                        unsigned int getBlockSize( );

                        /* get read ahead size */
                        unsigned int getReadAheadSize( );

                private:
                        /* prefetch behavior */
                        std::vector<mercury::willneed_t> willneed_;

                        /* i/o pattern behavior */
                        std::vector<mercury::iopat_t> iopat_;

                        /* block size */
                        unsigned int blockSize_;

                        /* cache size */
                        unsigned int cacheSize_;

                        /* read ahead size */
                        unsigned int readAheadSize_;

                        /* Constructor */
                        Config( );

                        /* Destructor */
                        ~Config( );

                        /* copy constructor */
                        Config( mercury::Config & );

                        /* assignment operator */
                        mercury::Config &operator=( mercury::Config & );

                        /* get pathname */
                        std::string getPathname( );

                        /* json configuration value */
                        static Json::Value config_;

                        /* configure map */
                        static std::map<std::string, mercury::Config *> configMap_;
        };
}
#endif /* _CONFIG_H */
