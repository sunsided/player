/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2000  
 *     Brian Gerkey, Kasper Stoy, Richard Vaughan, & Andrew Howard
 *                      
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/*
 * $Id$
 */

#include <p2os.h>

class P2OSdio: public P2OS 
{
 public:

   P2OSdio( ConfigFile* cf, int section) : 
           P2OS( cf, section){}
   size_t GetData(void*, unsigned char *, size_t maxsize, 
                   uint32_t* timestamp_sec, uint32_t* timestamp_usec);
};

Driver* P2OSdio_Init( ConfigFile* cf, int section)
{
  if(strcmp( PLAYER_DIO_STRING))
  {
    PLAYER_ERROR1("driver \"p2os_dio\" does not support interface \"%s\"\n",
                  interface);
    return(NULL);
  }
  else
    return((Driver*)(new P2OSdio( cf, section)));
}

// a driver registration function
void 
P2OSdio_Register(DriverTable* table)
{
  table->AddDriver("p2os_dio", PLAYER_READ_MODE, P2OSdio_Init);
}

size_t P2OSdio::GetData(void* client,unsigned char *dest, size_t maxsize,
                            uint32_t* timestamp_sec, uint32_t* timestamp_usec)
{
  Lock();
  *((player_dio_data_t*)dest) = ((player_p2os_data_t*)device_data)->dio;
  *timestamp_sec = data_timestamp_sec;
  *timestamp_usec = data_timestamp_usec;
  Unlock();

  return( sizeof( player_dio_data_t ));
}

