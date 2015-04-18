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

#include "BlockCache.hpp"
#include <sys/vfs.h>
#include <limits>

std::map<std::string, mercury::BlockCache *> mercury::BlockCache::cacheMap_;
pthread_mutex_t mercury::BlockCache::stmtx_ = PTHREAD_MUTEX_INITIALIZER;
void ( mercury::BlockCache::*accessReleaseBlock )( std::vector<mercury::block_t>&, std::vector<mercury::block_t>& ) = NULL;

/**
 * Return the instance of BlockCache Class corresponding to pathname
 *
 * @param pathname is the full pathname of the open file
 * @return the reference to the BlockCache object corresponding to pathname
 *
 */
mercury::BlockCache & mercury::BlockCache::getInstance( const std::string & pathname )
{
        pthread_mutex_lock( &stmtx_ );
        if( cacheMap_.count( pathname ) == 0 )
        {
                cacheMap_[pathname] = new mercury::BlockCache( );
                cacheMap_[pathname]->blockSize_ = mercury::Config::getInstance( pathname ).getBlockSize( ); // this inits config for pathname
                cacheMap_[pathname]->cacheSize_ = mercury::Config::getInstance( pathname ).getCacheSize( );
                cacheMap_[pathname]->readAheadSize_ = mercury::Config::getInstance( pathname ).getReadAheadSize( );

                if( cacheMap_[pathname]->blockSize_ == 0 )
                        cacheMap_[pathname]->blockSize_ = KB(4096);
                if( cacheMap_[pathname]->cacheSize_ == 0 )
                        cacheMap_[pathname]->cacheSize_ = 16;
                if( cacheMap_[pathname]->readAheadSize_ == 0 )
                        cacheMap_[pathname]->readAheadSize_ = 3;
                if( cacheMap_[pathname]->readAheadSize_ >= cacheMap_[pathname]->cacheSize_ )
                        cacheMap_[pathname]->readAheadSize_ = cacheMap_[pathname]->cacheSize_ - 1;
        }
        else
        {
                /* increment refCount_ */
                cacheMap_[pathname]->refCount_++;
        }
        pthread_mutex_unlock( &stmtx_ );

        return *cacheMap_[pathname];
}

/**
 * Release function for the BlockCache Singleton Class
 *
 * @param pathname is the full pathname of the open file
 *
 */
void mercury::BlockCache::release( )
{
        pthread_mutex_lock( &stmtx_ );
        std::string pathname = getPathname( );
        if( this->refCount_ > 1 )
        {
                this->refCount_--;
        }
        else
        {
                this->refCount_ = 0;
                delete this;
                cacheMap_.erase( pathname );
        }
        pthread_mutex_unlock( &stmtx_ );
}

/**
 * Prepare for loading into cache the data block corresponding to offset and length using LRU algorithm
 *
 * @param offset is the starting offset for the incoming request
 * @param length is the length for the incoming request
 * @return 0 if the block is accessed successfully, -1 if the length is too large
 *
 */
int mercury::BlockCache::getBlock( offset_t offset, offset_t length )
{
        std::vector<mercury::block_t> access, release;
        std::vector<mercury::willneed_t> willneed;
        mercury::block_t * blk;
        std::string pathname;
        offset_t off = offset;
        unsigned int i, j, k, blknum;
        int err;

        /* if req is big ignore it */
        if( length > blockSize_ )
                return -1;

        log4cpp::Category::getInstance( "mercury" ).debug(
                "From(%s,%s,%d,%0x,%d): Request access to block %u",
                __FILE__,
                __func__,
                __LINE__,
                pthread_self( ),
                fd_,
                getBlockNum( offset ) );

        /* allocate space for blocks */
        blk = ( mercury::block_t * )malloc( (1 + readAheadSize_)*sizeof( mercury::block_t ) );

        /* get willneed info from config_ */
        pathname = getPathname( );
        mercury::Config::getInstance( pathname ).getWillneedInfo( offset, length, willneed );

        k = 0;
        for( i = 0; i < willneed.size( ) && k < 1 + readAheadSize_; i++ )
        {
                /* if length is 0 consider the whole file */
                if( willneed[i].length_ == 0 )
                        willneed[i].length_ = std::numeric_limits<offset_t>::max( ) - blockSize_;

                /* initialize a # of blocks equal to 1 + readAheadSize_ */
                blknum = (unsigned int)(willneed[i].offset_ + willneed[i].length_ - off + blockSize_) / blockSize_;
                for( j = 0; j < blknum && j + k < 1 + readAheadSize_; j++ )
                {
                        blk[k+j].blockNumber_ = getBlockNum( off );
                        blk[k+j].startOffset_ = 0;
                        blk[k+j].blkLen_ = blockSize_;
                        blk[k+j].isWrite_ = false;
                        off += blk[k+j].blkLen_;
                        log4cpp::Category::getInstance( "mercury" ).debug(
                                "From(%s,%s,%d,%0x,%d): build blockNumber %u, startOffset %lli, blkLen %lli",
                                __FILE__,
                                __func__,
                                __LINE__,
                                pthread_self( ),
                                fd_,
                                blk[k+j].blockNumber_,
                                blk[k+j].startOffset_,
                                blk[k+j].blkLen_ );
                }
                k += j;

                /* update the offset to the next willneed range */
                if( i + 1 < willneed.size( ) ) off = willneed[i+1].offset_;
        }

        /* acquire lock on cache_ */
        err = pthread_mutex_lock( &mtx_ );
        if( err )
                std::cerr << "thread " << pthread_self( ) << " got error " <<
                        err << " while locking mtx_." << std::endl;

        /* find which of the blocks is not in cache_ */
        for( j = 0; j < k; j++ )
        {
                if( lookup( blk[j] ) )
                {
                        /* cache hit */
                        log4cpp::Category::getInstance( "mercury" ).debug(
                                "From(%s,%s,%d,%0x,%d): hitted Block %u",
                                __FILE__,
                                __func__,
                                __LINE__,
                                pthread_self( ),
                                fd_,
                                blk[j].blockNumber_ );
                }
                else
                {
                        /* cache miss */
                        access.push_back( blk[j] );
                        log4cpp::Category::getInstance( "mercury" ).debug(
                                "From(%s,%s,%d,%0x,%d): missed Block %u",
                                __FILE__,
                                __func__,
                                __LINE__,
                                pthread_self( ),
                                fd_,
                                blk[j].blockNumber_ );

                        if( cache_.size( ) < cacheSize_ )
                        {
                                /* enough space in cache for new block */
                                cache_.push_back( blk[j] );
                                log4cpp::Category::getInstance( "mercury" ).debug(
                                        "From(%s,%s,%d,%0x,%d): Block %u added to cache",
                                        __FILE__,
                                        __func__,
                                        __LINE__,
                                        pthread_self( ),
                                        fd_,
                                        blk[j].blockNumber_ );
                        }
                        else
                        {
                                /* not enough space in cache for new block */
                                release.push_back( cache_.front( ) );
                                log4cpp::Category::getInstance( "mercury" ).debug(
                                        "From(%s,%s,%d,%0x,%d): Block %u removed from cache",
                                        __FILE__,
                                        __func__,
                                        __LINE__,
                                        pthread_self( ),
                                        fd_,
                                        cache_.front( ).blockNumber_ );
                                cache_.pop_front( );
                                cache_.push_back( blk[j] );
                                log4cpp::Category::getInstance( "mercury" ).debug(
                                        "From(%s,%s,%d,%0x,%d): Block %u added to cache",
                                        __FILE__,
                                        __func__,
                                        __LINE__,
                                        pthread_self( ),
                                        fd_,
                                        blk[j].blockNumber_ );
                        }
                }
        }

        /* free blocks */
        free( blk );

        /* release lock on cache_ */
        err = pthread_mutex_unlock( &mtx_ );
        if( err )
                std::cerr << "thread " << pthread_self( ) << " got error " <<
                        err << " while unlocking mtx_." << std::endl;

        /* access release requested blocks */
        (this->*accessReleaseBlock)( access, release );

        /* remove blocks that were rejected by GPFS */
        if( access.size( ) > 0 )
        {
                /* acquire lock on cache_ */
                err = pthread_mutex_lock( &mtx_ );
                if( err )
                        std::cerr << "thread " << pthread_self( ) << " got error " <<
                                err << " while locking mtx_." << std::endl;

                /* remove not accessed blocks */
                removeBlockFromCache( access );

                /* release lock on cache_ */
                err = pthread_mutex_unlock( &mtx_ );
                if( err )
                        std::cerr << "thread " << pthread_self( ) << " got error " <<
                                err << " while unlocking mtx_." << std::endl;
        }
        return 0;
}

/**
 * Default BlockCache Class constructor (not accessible by users)
 *
 * @param fd is the file descriptor for the open file
 * @return a new instance for the BlockCache Class
 *
 */
mercury::BlockCache::BlockCache( )
{
        int err;
        fd_ = -1;
        refCount_ = 1;
        pthread_mutexattr_t attr;
        pthread_mutexattr_init( &attr );
        pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_ERRORCHECK );
        pthread_mutexattr_setrobust( &attr, PTHREAD_MUTEX_ROBUST );
        err = pthread_mutex_init( &mtx_, &attr );
        pthread_mutexattr_destroy( &attr );
        if( err )
                std::cerr << "error initializing mutex: " << strerror( errno ) << std::endl;
}

/**
 * Default BlockCache Class desctructor (not accessible by users)
 *
 */
mercury::BlockCache::~BlockCache( )
{
        close( fd_ );
        pthread_mutex_destroy( &mtx_ );
}

/**
 * Set file descriptor & configure hint API
 *
 * @param fd is the file descriptor to access the file
 */
void mercury::BlockCache::setFileDescriptor( int fd )
{
        /* set file descriptor */
        if( fd_ == -1 )
        {
                /* get a new file descriptor */
                fd_ = dup( fd );

                /* select the proper hint API */
                configHintApi( );
        }
}

/**
 * Select the proper hint interface to use for prefetching based on the file system type
 *
 */
void mercury::BlockCache::configHintApi( )
{
        struct statfs buf;
        fstatfs( fd_, &buf );

        if( buf.f_type == GPFS_SUPER_MAGIC )
        {
#ifdef HAVE_GPFS_FCNTL_H
                mercury::BlockCache::accessReleaseBlock = &mercury::BlockCache::gpfsAccessReleaseBlock;

                /* adjust parameters for GPFS */
                if( buf.f_bsize != this->blockSize_ )
                        this->blockSize_ = buf.f_bsize;
                if( this->readAheadSize_ >= GPFS_MAX_RANGE_COUNT )
                        this->readAheadSize_ = GPFS_MAX_RANGE_COUNT - 1;
#else
                mercury::BlockCache::accessReleaseBlock = &mercury::BlockCache::fakeAccessReleaseBlock;
                log4cpp::Category::getInstance( "mercury" ).warn(
                        "From(%s,%s,%d,%0x,%d): File resides on GPFS but gpfs_fcntl() is not installed. \
                        Using fakeAccessReleaseBlock() instead.",
                        __FILE__,
                        __func__,
                        __LINE__,
                        pthread_self( ),
                        fd_ );
#endif
        }
        else
        {
                mercury::BlockCache::accessReleaseBlock = &mercury::BlockCache::posixAccessReleaseBlock;
        }
}

/**
 * Return the block number corresponding to offset
 *
 * @param offset is the offeset of the current request
 * @return the number of the block in the file corresponding to offset
 *
 */
offset_t mercury::BlockCache::getBlockNum( offset_t offset )
{
        return( offset / blockSize_ );
}

/**
 * Lookup a block in the LRU cache (moves blk at end of list if hit)
 *
 * @param blk is the block to lookup
 * @return true if lookup is a hit, false otherwise
 *
 */
bool mercury::BlockCache::lookup( mercury::block_t & blk )
{
        std::list<mercury::block_t>::iterator it;

        /* find block */
        for( it = cache_.begin( ); it != cache_.end( ); it++ )
                if( it->blockNumber_ == blk.blockNumber_ )
                {
                        /* move block to end of cache_ (LRU) */
                        cache_.push_back( *it );
                        cache_.erase( it );
                        return true;
                }

        return false;
}

/**
 * Return the full pathname of the current BlockCache instance
 *
 * @return the string containing the full pathname
 *
 */
std::string mercury::BlockCache::getPathname( )
{
        std::map<std::string, mercury::BlockCache *>::iterator it;

        it = cacheMap_.begin( );

        while( it->second != this )
                it++;

        return it->first;
}

/**
 * Remove rejected blocks from the cache (only GPFS can reject hints)
 *
 * @param vec is the vector containing the rejected blocks to be removed
 *
 */
void mercury::BlockCache::removeBlockFromCache( std::vector<mercury::block_t>& vec )
{
        std::list<mercury::block_t>::iterator it;

        for( uint32_t i = 0; i < vec.size( ); i++ )
        {
                it = cache_.begin( );
                while( it->blockNumber_ != vec[i].blockNumber_ )
                        it++;

                if( it != cache_.end( ) )
                {
                        log4cpp::Category::getInstance( "mercury" ).debug(
                                "From(%s,%s,%d,%0x,%d): Rejected block %u removed from cache",
                                __FILE__,
                                __func__,
                                __LINE__,
                                pthread_self( ),
                                fd_,
                                it->blockNumber_ );

                        cache_.erase( it );
                }
        }
        vec.erase(vec.begin( ), vec.end ( ) );
}

/**
 * POSIX access/release function
 *
 * @param access is the vector containing the blocks to be accessed using POSIX_FADV_WILLNEED
 * @param release is the vector containing the blocks to be release using POSIX_FADV_DONTNEED
 *
 */
void mercury::BlockCache::posixAccessReleaseBlock( std::vector<mercury::block_t>& access, std::vector<mercury::block_t>& release )
{
        unsigned int i;
        offset_t offset = 0, len = 0;

        /* try to access adjacent blocks at once */
        for( i = 0; i < access.size( ); i++ )
        {
                offset = (access[i].blockNumber_ * blockSize_) + access[i].startOffset_;
                len = access[i].blkLen_;
                while( i + 1 < access.size( ) )
                {
                        if( access[i].blockNumber_ + 1 == access[i+1].blockNumber_ )
                        {
                                len += access[i+1].blkLen_;
                                i++;
                        }
                        else
                                break;
                }
                posix_fadvise( fd_, offset, len, POSIX_FADV_WILLNEED );
        }

        /* try to release adjacent blocks at once */
        for( i = 0; i < release.size( ); i++ )
        {
                offset = (release[i].blockNumber_ * blockSize_) + release[i].startOffset_;
                len = release[i].blkLen_;
                while( i + 1 < release.size( ) )
                {
                        if( release[i].blockNumber_ + 1 == release[i+1].blockNumber_ )
                        {
                                len += release[i+1].blkLen_;
                                i++;
                        }
                        else
                                break;
                }
                posix_fadvise( fd_, offset, len, POSIX_FADV_DONTNEED );
        }

        /* remove the accessed and released block */
        access.clear( );
        release.clear( );
}

#ifdef HAVE_GPFS_FCNTL_H
/**
 * GPFS access/release function
 *
 * @param access is the vector containing the blocks to be accessed using gpfs_fcntl()
 * @param release is the vector containing the blocks to be released using gpfs_fcntl()
 *
 */
void mercury::BlockCache::gpfsAccessReleaseBlock( std::vector<mercury::block_t>& access, std::vector<mercury::block_t>& release )
{
        struct
        {
                gpfsFcntlHeader_t hdr;
                gpfsMultipleAccessRange_t marh;

        } accHint;

        accHint.hdr.totalLength = sizeof( accHint );
        accHint.hdr.fcntlVersion = GPFS_FCNTL_CURRENT_VERSION;
        accHint.hdr.fcntlReserved = 0;
        accHint.marh.structLen = sizeof( accHint.marh );
        accHint.marh.structType = GPFS_MULTIPLE_ACCESS_RANGE;
        accHint.marh.accRangeCnt = access.size( );
        accHint.marh.relRangeCnt = release.size( );

        for( int i = 0; i < accHint.marh.accRangeCnt && i < GPFS_MAX_RANGE_COUNT; i++ )
        {
                accHint.marh.accRangeArray[i].blockNumber = access[i].blockNumber_;
                accHint.marh.accRangeArray[i].start       = access[i].startOffset_;
                accHint.marh.accRangeArray[i].length      = access[i].blkLen_;
                accHint.marh.accRangeArray[i].isWrite     = access[i].isWrite_;
        }
        for( int i = 0; i < accHint.marh.relRangeCnt && i < GPFS_MAX_RANGE_COUNT; i++ )
        {
                accHint.marh.relRangeArray[i].blockNumber = release[i].blockNumber_;
                accHint.marh.relRangeArray[i].start       = release[i].startOffset_;
                accHint.marh.relRangeArray[i].length      = release[i].blkLen_;
                accHint.marh.relRangeArray[i].isWrite     = release[i].isWrite_;
        }

        /* issue the hints to gpfs */
        if( gpfs_fcntl( fd_, &accHint ) )
        {
                std::cerr << "gpfs_fcntl access range hint failed for file " <<
                        fd_ << " errno=" << errno << " errorOffset=" <<
                        accHint.hdr.errorOffset << std::endl;
                exit( EXIT_FAILURE );
        }

        /* remove the accessed and released blocks */
        access.erase( access.begin( ), access.begin( ) + accHint.marh.accRangeCnt );
        release.erase( release.begin( ), release.begin( ) + accHint.marh.relRangeCnt );

        for( uint32_t i = 0; i < access.size( ); i++ )
        {
                log4cpp::Category::getInstance( "mercury" ).warn(
                        "From(%s,%s,%d,%0x,%d): Block %u rejected by gpfs_fcntl(); try reducing 'CacheSize'",
                        __FILE__,
                        __func__,
                        __LINE__,
                        pthread_self( ),
                        fd_,
                        access[i].blockNumber_ );
        }
}
#else
/**
 * Fake access/release function
 *
 * @param access is the vector containing the blocks to be accessed using POSIX_FADV_WILLNEED
 * @param release is the vector containing the blocks to be release using POSIX_FADV_DONTNEED
 *
 */
void mercury::BlockCache::fakeAccessReleaseBlock( std::vector<mercury::block_t>& access, std::vector<mercury::block_t>& release )
{
        /* remove the accessed and released block */
        access.clear( );
        release.clear( );
}
#endif
