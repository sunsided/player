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
 *  
 *  The virtual class from which all driver classes inherit.  this
 *  defines the API that all drivers must implement.
 */

#ifndef _DRIVER_H
#define _DRIVER_H

#include <pthread.h>

#include <stddef.h> /* for size_t */
#include <clientdata.h>
#include <playercommon.h>
#include <playerqueue.h>

extern bool debug;
extern bool experimental;

// Forward declarations
class CLock;
class ConfigFile;



class Driver
{
  private:
    // this mutex is used to lock data, command, and req/rep buffers/queues
    // TODO: could implement different mutexes for each data structure, but
    // is it worth it?
    //
    // NOTE: StageDevice won't use this; it declares its own inter-process
    // locking mechanism (and overrides locking methods)
    pthread_mutex_t accessMutex;

    // this mutex is used to mutually exclude calls to Setup and Shutdown
    pthread_mutex_t setupMutex;
    
    // the driver's thread
    pthread_t driverthread;

    // A condition variable (and accompanying mutex) that can be used to
    // signal other drivers that are waiting on this one.
    pthread_cond_t cond;
    pthread_mutex_t condMutex;
    
  protected:

    // signal that new data is available (calls pthread_cond_broadcast()
    // on this driver's condition variable, which will release other
    // drivers that are waiting on this one).
    void DataAvailable(void);

  public:

    // Default device id (single-interface drivers)
    player_device_id_t device_id;
        
    // likewise, we sometimes need to access subscriptions from outside
    // number of current subscriptions
    int subscriptions;

    // Total number of entries in the device table using this driver.
    // This is updated and read by the Device class.
    int entries;

    // If true, driver should be "always on", i.e., player will "subscribe" 
    // at startup, before any clients subscribe. The "alwayson" parameter in
    // the config file can be used to turn this feature on as well (in which
    // case this flag will be set to reflect that setting)
    // WARNING: this feature is experimental and may be removed in the future
    bool alwayson;

    // Last error value; useful for returning error codes from
    // constructors
    int error;

  public:

    // Default constructor for single-interface drivers.  Specify the
    // buffer sizes.
    Driver(ConfigFile *cf, int section, int interface, uint8_t access,
           size_t datasize, size_t commandsize, 
           int reqqueuelen, int repqueuelen);

    // Default constructor for multi-interface drivers; call
    // AddInterface() to add interfaces.
    Driver(ConfigFile *cf, int section);

    // Destructor
    virtual ~Driver();

    // Set/reset error code
    void SetError(int code) {this->error = code;}
    
    // Add a new-style interface; returns 0 on success
    int AddInterface(player_device_id_t id, unsigned char access,
                     size_t datasize, size_t commandsize,
                     size_t reqqueuelen, size_t repqueuelen);
    
    // these are used to control subscriptions to the driver; a driver MAY
    // override them, but usually won't (P2OS being the main exception).
    // The client pointer acts an identifier for drivers that need to maintain
    // their own client lists (such as the broadcast driver).  It's a void*
    // since it may refer to another driver (such as when the laserbeacon driver
    // subscribes to the laser driver).
    virtual int Subscribe(void *client);
    virtual int Unsubscribe(void *client);

    // these are called when the first client subscribes, and when the last
    // client unsubscribes, respectively.
    // these two MUST be implemented by the driver itself
    virtual int Setup() = 0;
    virtual int Shutdown() = 0;


    // This method is called by Player on each driver after all drivers have
    // been loaded, and immediately before entering the main loop, so override
    // it in your driver subclass if you need to do some last minute setup with
    // Player all set up and ready to go
    // WANRING: this feature is experimental and may be removed in the future
    virtual void Prepare() {}

    // This method is called once per loop (in Linux, probably either 50Hz or 
    // 100Hz) by the server.  Threaded drivers can use the default
    // implementation, which does nothing.  Drivers which don't have their
    // own threads and do all their work in an overridden GetData() should 
    // also override Update(), in order to call DataAvailable(), allowing
    // other drivers to Wait() on this driver.
    virtual void Update() {}

    // Note: the Get* and Put* functions MAY be overridden by the
    // driver itself, but then the driver is reponsible for Lock()ing
    // and Unlock()ing appropriately

    // New-style PutData; [id] specifies the interface to be written
    virtual void PutData(player_device_id_t id,
                         void* src, size_t len,
                         uint32_t timestamp_sec, uint32_t timestamp_usec);

    // New-style GetData; [id] specifies the interface to be read
    virtual size_t GetNumData(player_device_id_t id, void* client);
    virtual size_t GetData(player_device_id_t id, void* client,  
                           void* dest, size_t len,
                           uint32_t* timestamp_sec, uint32_t* timestamp_usec);

    // New-style: Write a new command to the driver
    virtual void PutCommand(player_device_id_t id, void* client,
                            void* src, size_t len);

    // New-style: Read the current command for the driver
    virtual size_t GetCommand(player_device_id_t id, void* dest, size_t len);

    // New-style: Clear the current command buffer
    virtual void ClearCommand(player_device_id_t id);

    // New-style: Write configuration request to driver
    // The src_id field is a legacy hack for p2os-syle request/reply tom-foolery.
    virtual int PutConfig(player_device_id_t id, player_device_id_t *src_id,
                          void *client, void *data, size_t len);

    // New-style: Get next configuration request for driver
    // The src_id field is a legacy hack for p2os-syle request/reply tom-foolery.    
    virtual size_t GetConfig(player_device_id_t id, player_device_id_t *src_id,
                             void **client, void *data, size_t len);

    // New-style: Write configuration reply to driver
    virtual int PutReply(player_device_id_t id, player_device_id_t *src_id,
                         void* client, unsigned short type, struct timeval* ts, 
                         void* data, size_t len);

    // New-style: Read configuration reply from driver;
    // The src_id field is a legacy hack for p2os-syle request/reply tom-foolery.
    virtual int GetReply(player_device_id_t id, player_device_id_t *src_id, void* client, 
                         unsigned short* type, struct timeval* ts, 
                         void* data, size_t len);

    ///////////////////////////////////////////////////////////////////////////////////
    // Deprecated functions (safe to call, but not safe to overload)

    // Write new data to the driver
    void PutData(void* src, size_t len,
                 uint32_t timestamp_sec, uint32_t timestamp_usec);

    // Read new data from the driver
    size_t GetNumData(void* client);
    size_t GetData(void* client, unsigned char* dest, size_t len,
                   uint32_t* timestamp_sec, uint32_t* timestamp_usec);

    // Write a new command to the driver
    void PutCommand(void* client, unsigned char* src, size_t len);

    // Read the current command for the driver
    size_t GetCommand(void* dest, size_t len);

    // Write configuration request to driver
    int PutConfig(player_device_id_t* driver, void* client, 
                  void* data, size_t len);

    // Read configuration request from driver
    size_t GetConfig(player_device_id_t* device, void** client, 
                     void *data, size_t len);

    // Write configuration reply to driver
    int PutReply(player_device_id_t* driver, void* client, 
                 unsigned short type, struct timeval* ts, 
                 void* data, size_t len);

    // Read configuration reply from driver
    virtual int GetReply(player_device_id_t* driver, void* client, 
                         unsigned short* type, struct timeval* ts, 
                         void* data, size_t len);

    // Convenient short form
    size_t GetConfig(void** client, void* data, size_t len);

    // Convenient short form
    int PutReply(void* client, unsigned short type);

    // Convenient short form
    int PutReply(void* client, unsigned short type, struct timeval* ts, 
                 void* data, size_t len);

    // End deprecated functions
    ///////////////////////////////////////////////////////////////////////////////////
    
    /* start a thread that will invoke Main() */
    virtual void StartThread();

    /* cancel (and wait for termination) of the thread */
    virtual void StopThread();
    
    // Main function for driver thread
    //
    virtual void Main();

    // Cleanup function for driver thread (called when main exits)
    //
    virtual void MainQuit(void);

    // A helper method for internal use; e.g., when one driver wants to make a
    // request of another driver.
    virtual int Request(player_device_id_t* device, void* requester, 
                        void* request, size_t reqlen,
                        unsigned short* reptype, struct timeval* ts,
                        void* reply, size_t replen);

    // Waits on the condition variable associated with this driver.
    // This method is called by other drivers to wait for new data.
    void Wait(void);

  protected:
    // Dummy main (just calls real main).  This is used to simplify thread
    // creation.
    //
    static void* DummyMain(void *driver);

    // Dummy main cleanup (just calls real main cleanup).  This is
    // used to simplify thread termination.
    //
    static void DummyMainQuit(void *driver);

    // these methods are used to lock and unlock the various buffers and
    // queues; they are implemented with mutexes in Driver and overridden
    // in CStageDriver
    virtual void Lock();
    virtual void Unlock();

};

#endif
