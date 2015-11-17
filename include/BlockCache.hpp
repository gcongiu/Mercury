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

#ifndef _BLOCK_CACHE_H
#define _BLOCK_CACHE_H

#include "Common.hpp"
#include "Config.hpp"

#ifdef HAVE_GPFS_FCNTL_H
#include <gpfs_fcntl.h>
#endif

#define GPFS_SUPER_MAGIC 0x47504653

/*
 * NOTE: BlockCache could be implemented as GenericBlockCache
 *       defining getBlock as virtual function. Then, there
 *       would be other two derived classes (one for each FS
 *       interface) implementing getBlockPosix() for POSIX FS
 *       and getBlockGPFS for GPFS. Nevertheless, this would
 *       require the singleton interface to change a little to
 *       pass the file descriptor to getInstance() in order to
 *       allow the implementation to decide which constructor
 *       to use.
 *       Using function pointers here is more similar to a C
 *       style approach but helps keeping the singleton class
 *       unchanged.
 */
namespace mercury
{
        /* willneed block data structure */
        typedef struct
        {
                offset_t blockNumber_;
                offset_t startOffset_;
                offset_t blkLen_;
                bool     isWrite_;

        } block_t;

        /* block cache class */
        class BlockCache
        {
                public:
                        /* get BlockCache instance for pathname */
                        static BlockCache & getInstance( const std::string & );

                        /* release */
                        void release( );

                        /* get block using either posix_fadvise() or gpfs_fcntl() */
                        int getBlock( offset_t, offset_t );

                        /* set file descriptor */
                        void setFileDescriptor( int );

                private:
                        /* static map containing references to BlockCache instances */
                        static std::map<std::string, mercury::BlockCache *> cacheMap_;

                        /* static mutex */
                        static pthread_mutex_t stmtx_;
                        //static pthread_spinlock_t spinlock_;

                        /* file descriptor */
                        int fd_;

                        /* how many threads are accessing same the file */
                        int refCount_;

                        /* thread mutex lock */
                        pthread_mutex_t mtx_;

                        /* block size */
                        offset_t blockSize_;

                        /* cache size in blocks */
                        unsigned int cacheSize_;

                        /* read ahead blocks number */
                        unsigned int readAheadSize_;

                        /* block cached list */
                        std::list<mercury::block_t> cache_;

                        /* default constructor */
                        BlockCache( );

                        /* destructor */
                        ~BlockCache( );

                        /* copy constructor */
                        BlockCache( mercury::BlockCache & );

                        /* Assignment operator */
                        mercury::BlockCache &operator=( mercury::BlockCache & );

                        /* set hint API type */
                        void configHintApi( );

                        /* get block number from offset */
                        offset_t getBlockNum( offset_t );

                        /* look up blk in cache */
                        bool lookup( mercury::block_t & );

                        /* return pathname for this object */
                        std::string getPathname( );

                        /* remove block from cache */
                        void removeBlockFromCache( std::vector<mercury::block_t>& );

                        /* posix access/release block */
                        void posixAccessReleaseBlock( std::vector<mercury::block_t>&, std::vector<mercury::block_t>& );
#ifdef HAVE_GPFS_FCNTL_H
                        /* gpfs access/release block */
                        void gpfsAccessReleaseBlock( std::vector<mercury::block_t>&, std::vector<mercury::block_t>& );
#else
                        /* fake access/release block */
                        void fakeAccessReleaseBlock( std::vector<mercury::block_t>&, std::vector<mercury::block_t>& );
#endif
                        /* generic access/release routine pointer */
                        void (BlockCache::*accessReleaseBlock)( std::vector<mercury::block_t>&, std::vector<mercury::block_t>& );
        };
}
#endif
