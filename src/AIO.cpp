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

//#define _POSIX_C_SOURCE >= 199309L

extern "C"
{
        // for LD_PRELOAD
        #define dlsym __
        #include <dlfcn.h>
        #undef dlsym
};

#include "Common.hpp"
#include "Config.hpp"
#include <jsoncpp/json/json.h>
#include <pcrecpp.h>
#include <algorithm>

// Local Functions Prototypes (Use extern "C" to avoid g++ name mangling)
extern "C" void *(*dlsym( void *handle, const char *symbol ))( );
static int     (*_open)         ( const char *, int, ... )                = NULL;
static int     (*_open_2)       ( const char *, int )                     = NULL;
static int     (*_open64)       ( const char *, int, ... )                = NULL;
static int     (*_close)        ( int )                                   = NULL;
static ssize_t (*_read)         ( int, void*, size_t )                    = NULL;
static ssize_t (*_pread)        ( int, void*, size_t, off_t )             = NULL;
static FILE *  (*_fopen)        ( const char *, const char * )            = NULL;
static FILE *  (*_fopen64)      ( const char *, const char * )            = NULL;
static int     (*_fclose)       ( FILE* )                                 = NULL;
static size_t  (*_fread)        ( void*, size_t, size_t, FILE* )          = NULL;
static int     (*___open_2)     ( const char *, int )                     = NULL;
static int     (*__open)        ( const char *, int, ... )                = NULL;
static int     (*__open64)      ( const char *, int, ... )                = NULL;
static int     (*__close)       ( int )                                   = NULL;
static FILE *  (*__fopen)       ( const char *, const char * )            = NULL;
static FILE *  (*__fopen64)     ( const char *, const char * )            = NULL;
static int     (*__fclose)      ( FILE* )                                 = NULL;
static ssize_t (*__read)        ( int, void*, size_t )                    = NULL;
static ssize_t (*__pread)       ( int, void*, size_t, off_t )             = NULL;
static size_t  (*__fread)       ( void*, size_t, size_t, FILE* )          = NULL;
#if 0
static ssize_t (*__pread64)     ( int, void*, size_t, off64_t )           = NULL;
static ssize_t (*_write)        ( int, const void*, size_t )              = NULL;
static off_t   (*_lseek)        ( int, off_t, int )                       = NULL;
static off64_t (*_lseek64)      ( int, off64_t, int )                     = NULL;
static ssize_t (*_pread64)      ( int, void*, size_t, off64_t )           = NULL;
static ssize_t (*_pwrite)       ( int, const void*, size_t, off_t )       = NULL;
static ssize_t (*_pwrite64)     ( int, const void*, size_t, off64_t )     = NULL;
static ssize_t (*_readv)        ( int const struct iovec*, int )          = NULL;
static ssize_t (*_writev)       ( int, const struct iovec*, int )         = NULL;
static int     (*_fxstat)       ( int, int struct stat* )                 = NULL;
static int     (*_fxstat64)     ( int, int, struct stat64* )              = NULL;
static int     (*_lxstat)       ( int, const char*, struct stat* )        = NULL;
static int     (*_lxstat64)     ( int, const char*, struct stat64* )      = NULL;
static void *  (*_map)          ( void*, size_t, int, int, int, off_t )   = NULL;
static void *  (*_map64)        ( void*, size_t, int, int, int, off64_t ) = NULL;
static FILE *  (*_fopen64)      ( const char *, const char * )            = NULL;
static size_t  (*_fwrite)       ( void*, size_t, size_t, FILE* )          = NULL;
static int     (*_fseek)        ( FILE*, long, int )                      = NULL;
static int     (*_fsync)        ( int )                                   = NULL;
static int     (*_fdatasync)    ( int )                                   = NULL;
static int     (*_aio_read)     ( struct aiocb* )                         = NULL;
static int     (*_aio_read64)   ( struct aiocb64* )                       = NULL;
static int     (*_aio_write)    ( struct aiocb* )                         = NULL;
static int     (*_aio_write64)  ( struct aiocb64* )                       = NULL;
static int     (*_lio_listio)   ( int, struct aiocb*, int, struct sigevent* )   = NULL;
static int     (*_lio_listio64) ( int, struct aiocb64*, int, struct sigevent* ) = NULL;
static ssize_t (*_aio_return)   ( struct aiocb* )                         = NULL;
static ssize_t (*_aio_return64) ( struct aiocb64* )                       = NULL;
#endif

extern "C"
{
        int __open_2( const char*, int );
};

static int     myopen   ( const char *, int, ... );
static int     myopen_2 ( const char *, int );
static int     myopen64 ( const char *, int, ... );
static int     myclose  ( int );
static FILE *  myfopen  ( const char *, const char * );
static FILE *  myfopen64( const char *, const char * );
static int     myfclose ( FILE* );
static ssize_t myread   ( int, void*, size_t );
static ssize_t mypread  ( int, void*, size_t, off_t );
static size_t  myfread  ( void*, size_t, size_t, FILE* );
//static ssize_t mypread64(int, void*, size_t, off64_t );

// the root object that holds the configs
static Json::Value config_;

// paths to be advised
static std::list<std::string> * paths_ = NULL; //valid paths
static std::list<int> * files_ = NULL;         //tracked files

// timers
static struct timeval start_, end_, iostart_, ioend_;
static double iotime_;

// system socket
#ifndef UNIX_PATH_MAX
#define UNIX_PATH_MAX 108
#endif
__thread struct msghdr msg_ = {0};
static struct sockaddr_un addr_;
static int sockfd_;

static pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;

// Assisted I/O initialization
static void __attribute__(( constructor( 65535 ) )) AIO_init( void )
{
        /* start measuring the time */
        gettimeofday( &start_, NULL );

        std::cerr << "# AIO_init()" << std::endl;

        /* init system socket for interprocess communication */
        /* Address Family type: local unix socket            */
        addr_.sun_family = AF_UNIX;

        /* pathname for the socket in the file system */
        strcpy( addr_.sun_path, "/tmp/channel" );

        /* create a new socket to support datagram type messages (need SOCK_DGRAM to use sendmsg) */
        if( (sockfd_ = socket( AF_UNIX, SOCK_DGRAM, 0 )) == -1 )
        {
                std::cerr << "## error creating socket: " << strerror( errno ) << std::endl;
                exit( EXIT_FAILURE );
        }

        /* connect to the socket */
        int addrsize = sizeof( addr_.sun_family ) + sizeof( addr_.sun_path );
        if( connect( sockfd_, (struct sockaddr *)&addr_, addrsize ) == -1 )
        {
                std::cerr << "## error connecting to socket: " << strerror( errno ) << std::endl;
                exit( EXIT_FAILURE );
        }

        /* initialize files_ amd paths_ map */
        pthread_mutex_lock( &mutex_ );
        files_ = new std::list<int>( );
        pthread_mutex_unlock( &mutex_ );
        paths_ = new std::list<std::string>( );

        // read the json configuration file for the rules
        char *config_file = getenv( "MERCURY_CONFIG" );

        if( config_file == NULL )
        {
                std::cerr << "## ERROR: No configuration file available" << std::endl;
                std::cerr << "## type: 'export MERCURY_CONFIG=\"config.json\"'" << std::endl;
                exit( EXIT_FAILURE );
        }

        // Configure Config data object
        if( !mercury::Config::init( std::string( config_file ) ) )
        {
                exit( EXIT_FAILURE );
        }
        if( !mercury::Config::getAllPaths( paths_ ) )
        {
                exit( EXIT_FAILURE );
        }

        iotime_ = 0;

        std::cerr << "# monitored paths: ";
        std::list<std::string>::iterator it = paths_->begin( );
        for( ; it != paths_->end( ); it++ )
                std::cerr << *it;
        std::cerr << std::endl;

        // redirect calls from within the interposed lib to original functions in libc
        ___open_2 = (int     (*)( const char*, int ))            dlsym( RTLD_NEXT, "__open_2" );
        __open    = (int     (*)( const char*, int, ... ))       dlsym( RTLD_NEXT, "open" );
        __close   = (int     (*)( int ))                         dlsym( RTLD_NEXT, "close" );
        __read    = (ssize_t (*)( int, void*, size_t ))          dlsym( RTLD_NEXT, "read" );
        __fopen   = (FILE *  (*)( const char*, const char* ))    dlsym( RTLD_NEXT, "fopen" );
        __fopen64 = (FILE *  (*)( const char*, const char* ))    dlsym( RTLD_NEXT, "fopen64" );
        __open64  = (int     (*)( const char*, int, ... ))       dlsym( RTLD_NEXT, "open64" );
        __fclose  = (int     (*)( FILE * ))                      dlsym( RTLD_NEXT, "fclose" );
        __pread   = (ssize_t (*)( int, void*, size_t, off_t ))   dlsym( RTLD_NEXT, "pread" );
        __fread   = (size_t  (*)( void*, size_t, size_t, FILE* ))dlsym( RTLD_NEXT, "fread" );
        //__pread64= (ssize_t (*)( int, void*, size_t, off64_t ))  dlsym( RTLD_NEXT, "pread64" );

        _open_2   = &myopen_2;
        _open     = &myopen;
        _close    = &myclose;
        _read     = &myread;
        _fopen    = &myfopen;
        _fopen64  = &myfopen64;
        _open64   = &myopen64;
        _fclose   = &myfclose;
        _pread    = &mypread;
        _fread    = &myfread;
        //_pread64 = &mypread64;
}

static void __attribute__(( destructor( 65535 ) )) AIO_fini( void )
{
        /* stop the timer */
        gettimeofday( &end_, NULL );

        std::cerr << "# AIO_fini()" << std::endl;

        /* print iotime and total runtime to standard output */
        std::cout << "iotime,total" << std::endl;
        std::cout << "" << iotime_
                  << "," << (double)((end_.tv_sec - start_.tv_sec) +
                                     (end_.tv_usec - start_.tv_usec) / 1000000.0)
                  << std::endl;

        /* reset the to original interface */
        _open_2  = (int     (*)( const char*, int ))            dlsym( RTLD_NEXT, "__open_2" );
        _open    = (int     (*)( const char*, int, ... ))       dlsym( RTLD_NEXT, "open" );
        _close   = (int     (*)( int ))                         dlsym( RTLD_NEXT, "close" );
        _read    = (ssize_t (*)( int, void*, size_t ))          dlsym( RTLD_NEXT, "read" );
        _fopen   = (FILE *  (*)( const char*, const char* ))    dlsym( RTLD_NEXT, "fopen" );
        _fopen64 = (FILE *  (*)( const char*, const char* ))    dlsym( RTLD_NEXT, "fopen64" );
        _open64  = (int     (*)( const char*, int, ... ))       dlsym( RTLD_NEXT, "open64" );
        _fclose  = (int     (*)( FILE * ))                      dlsym( RTLD_NEXT, "fclose" );
        _pread   = (ssize_t (*)( int, void*, size_t, off_t ))   dlsym( RTLD_NEXT, "pread" );
        _fread   = (size_t  (*)( void*, size_t, size_t, FILE* ))dlsym( RTLD_NEXT, "fread" );
        //__pread64= (ssize_t (*)( int, void*, size_t, off64_t ))  dlsym( RTLD_NEXT, "pread64");

        paths_->clear( );
        delete paths_;

        /* need to close the fd still open */
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = files_->begin( );
        for( ; it != files_->end( ); it++ )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[32];
                sprintf( command_text, "Unregister %u %d", getpid( ), *it );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }
        }
        delete files_;
        pthread_mutex_unlock( &mutex_ );
        _close( sockfd_ );
}

/** ==============================================================
 *  ========================= open ===============================
 */
int __open_2( const char *pathname, int flags )
{
        int ret;

        /* before the constructor is called _open points to open in libc
         * after the constructor is called _open points to myopen
         * after the destructor is called _open points back to open in libc
         */
        if( unlikely( _open_2 == NULL ) )
                _open_2 = (int (*)( const char *, int ))dlsym( RTLD_NEXT, "__open_2" );

        gettimeofday( &iostart_, NULL );
        ret = _open_2( pathname, flags );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

int myopen_2( const char *pathname, int flags )
{
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );

        int fd = ___open_2( pathname, flags );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[MSGSIZE];
                sprintf( command_text, "Register %u %s %d", getpid( ), pathname, fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                struct cmsghdr *cmsg;
                char fdesc[CMSG_SPACE( sizeof fd )];
                msg_.msg_control = fdesc;
                msg_.msg_controllen = sizeof fdesc;
                cmsg = CMSG_FIRSTHDR( &msg_ );
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN( sizeof( int ) );

                /* initialize the payload with the file descriptor */
                *(int *)CMSG_DATA( cmsg ) = fd;
                msg_.msg_controllen = cmsg->cmsg_len;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                std::list<int>::iterator file = std::find( files_->begin( ), files_->end( ), fd );
                if( file == files_->end( ) )
                {
                        files_->push_back( fd );
                }
                pthread_mutex_unlock( &mutex_ );
        }
        return fd;
}

int open( const char *pathname, int flags, ... )
{
        int ret;
        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        /* before the constructor is called _open points to open in libc
         * after the constructor is called _open points to myopen
         * after the destructor is called _open points back to open in libc
         */
        if( unlikely( _open == NULL ) )
                _open = (int (*)( const char *, int, ... ))dlsym( RTLD_NEXT, "open" );

        gettimeofday( &iostart_, NULL );
        ret = _open( pathname, flags, ap );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

int myopen( const char *pathname, int flags, ... )
{
        char * filename = strdup( pathname );
        char * basedir = dirname( (char *)filename );
        std::string path( pathname );
        std::string base( basedir );

        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        int fd = __open( pathname, flags, ap );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[MSGSIZE];
                sprintf( command_text, "Register %u %s %d", getpid( ), pathname, fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                struct cmsghdr *cmsg;
                char fdesc[CMSG_SPACE( sizeof fd )];
                msg_.msg_control = fdesc;
                msg_.msg_controllen = sizeof fdesc;
                cmsg = CMSG_FIRSTHDR( &msg_ );
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN( sizeof( int ) );

                /* initialize the payload with the file descriptor */
                *(int *)CMSG_DATA( cmsg ) = fd;
                msg_.msg_controllen = cmsg->cmsg_len;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                std::list<int>::iterator file = std::find( files_->begin( ), files_->end( ), fd );
                if( file == files_->end( ) )
                {
                        files_->push_back( fd );
                }
                pthread_mutex_unlock( &mutex_ );
        }
        return fd;
}

/** ==============================================================
 *  ========================= open64 =============================
 */
int open64( const char *pathname, int flags, ... )
{
        int ret;
        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        /* before the constructor is called _open64 points to open in libc
         * after the constructor is called _open64 points to myopen64
         * after the destructor is called _open64 points back to open64 in libc
         */
        if( unlikely( _open64 == NULL ) )
                _open64 = (int (*)( const char *, int, ... ))dlsym( RTLD_NEXT, "open" );

        gettimeofday( &iostart_, NULL );
        ret = _open64( pathname, flags, ap );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

int myopen64( const char *pathname, int flags, ... )
{
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );

        va_list ap;
        va_start( ap, flags );
        va_end( ap );
        int fd = __open64( pathname, flags, ap );

        if( fd == -1 )
        {
                std::cerr << "Cannot open " << pathname << ": " << strerror( errno ) << std::endl;
                return -1;
        }

        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( (dir != paths_->end( ) || file != paths_->end( )) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[MSGSIZE];
                sprintf( command_text, "Register %u %s %d", getpid( ), pathname, fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                struct cmsghdr *cmsg;
                char fdesc[CMSG_SPACE( sizeof fd )];
                msg_.msg_control = fdesc;
                msg_.msg_controllen = sizeof fdesc;
                cmsg = CMSG_FIRSTHDR( &msg_ );
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN( sizeof( int ) );

                /* initialize the payload with the file descriptor */
                *(int *)CMSG_DATA( cmsg ) = fd;
                msg_.msg_controllen = cmsg->cmsg_len;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                std::list<int>::iterator file = std::find( files_->begin( ), files_->end( ), fd );
                if( file == files_->end( ) )
                {
                        files_->push_back( fd );
                }
                pthread_mutex_unlock( &mutex_ );
        }
        return fd;
}

/** ==============================================================
 *  ========================= fopen ==============================
 */
FILE* fopen( const char *pathname, const char *mode )
{
        FILE *fp;

        /* before the constructor is called _fopen points to fopen in libc
         * after the constructor is called _fopen points to myfopen
         * after the destructor is called _fopen points back to fopen in libc
         */
        if( unlikely( _fopen == NULL ) )
                _fopen = (FILE* (*)( const char *, const char * ))dlsym( RTLD_NEXT, "fopen" );

        gettimeofday( &iostart_, NULL );
        fp = _fopen( pathname, mode );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return fp;
}

FILE* myfopen( const char *pathname, const char *mode )
{
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        FILE* fp = __fopen( pathname, mode );
        if( fp == NULL )
        {
                std::cerr << "Cannot open " << pathname <<": " << strerror( errno ) << std::endl;
                return NULL;
        }

        int fd = fileno( fp );
        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[MSGSIZE];
                sprintf( command_text, "Register %u %s %d", getpid( ), pathname, fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                struct cmsghdr *cmsg;
                char fdesc[CMSG_SPACE( sizeof fd )];
                msg_.msg_control = fdesc;
                msg_.msg_controllen = sizeof fdesc;
                cmsg = CMSG_FIRSTHDR( &msg_ );
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN( sizeof( int ) );

                /* initialize the payload with the file descriptor */
                *(int *)CMSG_DATA( cmsg ) = fd;
                msg_.msg_controllen = cmsg->cmsg_len;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                std::list<int>::iterator file = std::find( files_->begin( ), files_->end( ), fd );
                if( file == files_->end( ) )
                {
                        files_->push_back( fd );
                }
                pthread_mutex_unlock( &mutex_ );
        }
        return fp;
}

/** ==============================================================
 *  ========================= fopen64 ============================
 */
FILE* fopen64( const char *pathname, const char *mode )
{
        FILE *fp;

        /* before the constructor is called _fopen points to fopen in libc
         * after the constructor is called _fopen points to myfopen
         * after the destructor is called _fopen points back to fopen in libc
         */
        if( unlikely( _fopen64 == NULL ) )
                _fopen64 = (FILE* (*)( const char *, const char * ))dlsym( RTLD_NEXT, "fopen64" );

        gettimeofday( &iostart_, NULL );
        fp = _fopen64( pathname, mode );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return fp;
}

FILE* myfopen64( const char *pathname, const char *mode )
{
        char * filename = strdup( pathname );
        char * basedir = dirname( (char*)filename );
        std::string path( pathname );
        std::string base( basedir );
        FILE* fp = __fopen64( pathname, mode );
        if( fp == NULL )
        {
                std::cerr << "Cannot open " << pathname <<": " << strerror( errno ) << std::endl;
                return NULL;
        }

        int fd = fileno( fp );
        std::list<std::string>::iterator dir = std::find( paths_->begin( ), paths_->end( ), base );
        std::list<std::string>::iterator file = std::find( paths_->begin( ), paths_->end( ), path );
        if( dir != paths_->end( ) || file != paths_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[MSGSIZE];
                sprintf( command_text, "Register %u %s %d", getpid( ), pathname, fileno( fp ) );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                struct cmsghdr *cmsg;
                char fdesc[CMSG_SPACE( sizeof fd )];
                msg_.msg_control = fdesc;
                msg_.msg_controllen = sizeof fdesc;
                cmsg = CMSG_FIRSTHDR( &msg_ );
                cmsg->cmsg_level = SOL_SOCKET;
                cmsg->cmsg_type = SCM_RIGHTS;
                cmsg->cmsg_len = CMSG_LEN( sizeof( int ) );

                /* initialize the payload with the file descriptor */
                *(int *)CMSG_DATA( cmsg ) = fd;
                msg_.msg_controllen = cmsg->cmsg_len;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                std::list<int>::iterator file = std::find( files_->begin( ), files_->end( ), fd );
                if( file == files_->end( ) )
                {
                        files_->push_back( fd );
                }
                pthread_mutex_unlock( &mutex_ );
        }
        return fp;
}

 /** ==============================================================
 *  ========================= close ==============================
 */
int close( int fd )
{
        int ret;

        /* before the constructor is called _close points to close in libc
         * after the constructor is called _close points to myclose
         * after the destructor is called _close points back to close in libc
         */
        if( unlikely( _close == NULL ) )
                _close = (int (*)( int ))dlsym( RTLD_NEXT, "close" );

        gettimeofday( &iostart_, NULL );
        ret = _close( fd );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

int myclose( int fd )
{
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = std::find( files_->begin( ), files_->end( ), fd );
        pthread_mutex_unlock( &mutex_ );

        if( it != files_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[32];
                sprintf( command_text, "Unregister %u %d", getpid( ), fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                files_->erase( it );
                pthread_mutex_unlock( &mutex_ );
        }
        return __close( fd );
}

/** ==============================================================
 *  ========================= fclose =============================
 */
int fclose( FILE *fp )
{
        int ret;

        /* before the constructor is called _fclose points to fclose in libc
         * after the constructor is called _fclose points to myfclose
         * after the destructor is called _fclose points back to fclose in libc
         */
        if( unlikely( _fclose == NULL ) )
                _fclose = (int (*)( FILE * ))dlsym( RTLD_NEXT, "fclose" );

        gettimeofday( &iostart_, NULL );
        ret = _fclose( fp );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

int myfclose( FILE *fp )
{
        int fd = fileno( fp );
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = std::find( files_->begin( ), files_->end( ), fd );
        pthread_mutex_unlock( &mutex_ );

        if( it != files_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[32];
                sprintf( command_text, "Unregister %u %d", getpid( ), fd );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }

                pthread_mutex_lock( &mutex_ );
                files_->erase( it );
                pthread_mutex_unlock( &mutex_ );
        }
        return __fclose( fp );
}

/** ==============================================================
 *  ========================= read ===============================
 */
ssize_t read( int fd, void *buf, size_t count )
{
        ssize_t ret;

        /* before the constructor is called _read points to read in libc
         * after the constructor is called _read points to myread
         * after the destructor is called _read points back to read in libc
         */
        if( unlikely( _read == NULL ) )
                _read = (ssize_t (*)( int, void*, size_t ))dlsym( RTLD_NEXT, "read" );

        gettimeofday( &iostart_, NULL );
        ret = _read( fd, buf, count );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

ssize_t myread( int fd, void *buf, size_t count )
{
        off_t offset = lseek( fd, 0, SEEK_CUR );
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = std::find( files_->begin( ), files_->end( ), fd );
        pthread_mutex_unlock( &mutex_ );

        if( it != files_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[64];
                sprintf( command_text, "Read %u %d %li %lu", getpid( ), fd, offset, count );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit (EXIT_FAILURE);
                }
        }
        return __read( fd, buf, count );
}

/** ==============================================================
 *  ========================= pread ==============================
 */
ssize_t pread( int fd, void *buf, size_t count, off_t offset )
{
        ssize_t ret;

        /* before the constructor is called _pread points to pread in libc
         * after the constructor is called _pread points to mypread
         * after the destructor is called _pread points back to pread in libc
         */
        if( unlikely( _pread == NULL ) )
                _pread = (ssize_t (*)( int, void*, size_t, off_t ))dlsym( RTLD_NEXT, "pread" );

        gettimeofday( &iostart_, NULL );
        ret = _pread( fd, buf, count, offset );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

ssize_t mypread( int fd, void* buf, size_t count, off_t offset )
{
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = std::find( files_->begin( ), files_->end( ), fd );
        pthread_mutex_unlock( &mutex_ );

        if( it != files_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[64];
                sprintf( command_text, "Read %u %d %li %lu", getpid( ), fd, offset, count );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info*/
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }
        }
        return __pread( fd, buf, count, offset );
}

/** ==============================================================
 *  ========================= fread ==============================
 */
size_t fread( void *ptr, size_t size, size_t nmemb, FILE *stream )
{
        size_t ret;

        /* before the constructor is called _fread points to fread in libc
         * after the constructor is called _fread points to myfread
         * after the destructor is called _fread points back to fread in libc
         */
        if( unlikely( _fread == NULL ) )
                _fread = (size_t (*)( void*, size_t, size_t, FILE* ))dlsym( RTLD_NEXT, "fread" );

        gettimeofday( &iostart_, NULL );
        ret = _fread( ptr, size, nmemb, stream );
        gettimeofday( &ioend_, NULL );
        iotime_ += (double)((ioend_.tv_sec - iostart_.tv_sec) +
                            (ioend_.tv_usec - iostart_.tv_usec) / 1000000.0 );

        return ret;
}

size_t myfread( void *ptr, size_t size, size_t nmemb, FILE *stream )
{
        int fd = fileno( stream );
        pthread_mutex_lock( &mutex_ );
        std::list<int>::iterator it = std::find( files_->begin( ), files_->end( ), fd );
        pthread_mutex_unlock( &mutex_ );

        if( it != files_->end( ) )
        {
                /* initialize the actual message              */
                /* message format: 'Register pid pathname fd' */
                char command_text[64];
                sprintf( command_text, "Read %u %d %li %lu", getpid( ), fileno( stream ), lseek( fd, 0, SEEK_CUR ), size * nmemb );
                struct iovec command;
                command.iov_base = command_text;
                command.iov_len = sizeof( command_text );
                msg_.msg_iov = &command;
                msg_.msg_iovlen = 1;

                /* access control message info */
                msg_.msg_control = NULL;
                msg_.msg_controllen = 0;
                msg_.msg_flags = 0;

                /* finally send the message through the socket */
                if( sendmsg( sockfd_, (struct msghdr *)&msg_, 0 ) == -1 )
                {
                        std::cerr << "## error sending message: " << strerror( errno ) << std::endl;
                        exit( EXIT_FAILURE );
                }
        }
        return __fread( ptr, size, nmemb, stream );
}
