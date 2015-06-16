/*
 * Copyright (c) 2013-2014 Seagate Technology.
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

#define _LARGEFILE64_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#define KB(x) (x << 10)
#define MB(x) (x << 20)
#define GB(x) (x << 30)

typedef long long offset_t;

void print_usage( char *argv[] )
{
        fprintf( stderr, "Usage: %s [options]\n\n", argv[0] );
        fprintf( stderr, "options:\n" );
        fprintf( stderr, "  -f NAME  file pathname\n" );
        fprintf( stderr, "  -s DELAY usecs between reads\n" );
        fprintf( stderr, "  -t STEP  seek step between reads\n" );
        fprintf( stderr, "  -u       unlink pathname when done\n\n" );
}

int main( int argc, char *argv[] )
{
        char buffer[MB(1)], *pathname;
        offset_t offset, len = KB(100);
        int fd, perm, opt, cleanup = 0;
        ssize_t ret = -1, written = 0;
        int delay = 0, step = 0;
        struct stat buf;

        /* init pathname to NULL */
        pathname = NULL;

        while( (opt = getopt( argc, argv, "f:s:t:uh" )) != -1 )
        {
                switch( opt )
                {
                        case 'f': pathname = strdup( optarg );
                                  break;
                        case 's': delay = atoi( optarg );
                                  break;
                        case 'u': cleanup = 1;
                                  break;
                        case 't': step = atoi( optarg );
                                  break;
                        case 'h':
                        default: /* '?' */
                                  print_usage( argv );
                                  exit( EXIT_FAILURE );
                }
        }
        if( pathname == NULL )
        {
                print_usage( argv );
                exit( EXIT_FAILURE );
        }

        /* stat file */
        if( stat( pathname, &buf ) == -1 && errno == ENOENT )
        {
                /* file does not exist */
                goto fn_write;
        }
        else
        {
                fd = open( pathname, O_RDONLY );
                goto fn_read;
        }

fn_write:
        /* open a new file */
        perm = umask( 022 ) ^ 0644;
        fd = open( pathname, O_RDWR | O_CREAT, perm );
        if( fd == -1 )
        {
                fprintf( stderr, "[%d] '%s' error %s\n", __LINE__, pathname, strerror( errno ) );
                exit( EXIT_FAILURE );
        }

        /* write data to the file: 1GB */
        while( written <= GB(1) )
        {
                ret = write( fd, buffer, MB(1) );
                if( ret == -1 )
                {
                        fprintf( stderr, "[%d] '%s' error %s\n", __LINE__, pathname, strerror( errno ) );
                        exit( EXIT_FAILURE );
                }
                written += ret;
        }

fn_read:
        /* reset the offset to beginning of file */
        offset = lseek( fd, 0, SEEK_SET );

        /* now read the data back: read 100KB jump ahead 200KB till the EOF */
        while( offset <= GB(1) )
        {
               ret = read( fd, buffer, len );
               if( ret == -1 )
               {
                        fprintf( stderr, "[%d] '%s' error %s\n", __LINE__, pathname, strerror( errno ) );
                        exit( EXIT_FAILURE );
               }
               offset += lseek( fd, KB(step), SEEK_CUR );
               usleep( delay );
        }

        /* close the file */
        close( fd );

        if( cleanup )
        {
                /* delete the file */
                unlink( pathname );
        }

        free( pathname );
        return 0;
}
