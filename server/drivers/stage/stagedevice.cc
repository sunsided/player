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
 * Stage device that connects to a Stage server and interacts with it
 */

#include <string.h> // for memcpy()
#include <stdlib.h> // for rand()
#include <playercommon.h> // For length-specific types, error macros, etc
#include <device.h>
#include <configfile.h>
#include <sio.h> // from the Stage installation

#include <playertime.h>
extern PlayerTime* GlobalTime;

class StageDevice : public CDevice
{
  private:
    // the following vars are static because they concern the connection to
    // Stage, of which there is only one, for all Stage devices.
    static int stage_conn;  // fd to stage
    static bool initdone; // did we initialize the static data yet?
    static pthread_mutex_t stage_conn_mutex; // protect access to the socket
    static int HandleModel(int conn, char* data, size_t len);
    static int HandleProperty(int conn, char* data, size_t len);
    static int HandleLostConnection(int conn);
    static int HandleLostConnection2(int conn);
    static int pending_key;
    static int pending_id;
    static stage_model_t rootmodel;

    stage_model_t stage_model;

    int CreateModel(stage_model_t* model);

  public:
    StageDevice(int parent, char* interface,
                size_t datasize, size_t commandsize, 
                int reqqueuelen, int repqueuelen);

    stage_model_t GetModel() { return(stage_model); }
    
    // Initialise the device
    //
    virtual int Setup();

    // Terminate the device
    //
    virtual int Shutdown();
    
    // Read data from the device
    //
    virtual size_t GetData(void* client,unsigned char* dest, size_t len,
                           uint32_t* timestamp_sec,
                           uint32_t* timestamp_usec);

    // Write a command to the device
    //
    virtual void PutCommand(void* client, unsigned char * , size_t maxsize);
    
    // Main function for device thread
    //
    virtual void Main();
};

extern int StageDevice::stage_conn;  // fd to stage
extern bool StageDevice::initdone; // did we initialize the static data yet?
extern pthread_mutex_t StageDevice::stage_conn_mutex; // protect access to the socket
extern int StageDevice::pending_key;
extern int StageDevice::pending_id;
extern stage_model_t StageDevice::rootmodel;


StageDevice::StageDevice(int parent, char* interface,
                         size_t datasize, size_t commandsize, 
                         int reqqueuelen, int repqueuelen) :
  CDevice(datasize,commandsize,reqqueuelen,repqueuelen)
{
  // connect to Stage
  if(!initdone)
  {
    // TODO: pass the hostname:port from somewhere
    // connect to Stage
    if((stage_conn = SIOInitClient(0,NULL)) == -1)
    {
      PLAYER_ERROR("unable to connect to Stage");
      return;
    }

    // create a GUI
    stage_gui_config_t gui;
    strcpy( gui.token, "rtk" ); 
    gui.width = 600;
    gui.height = 600;
    gui.ppm = 40;
    gui.originx = 0;//300;
    gui.originy = 0;//300; 
    gui.showsubscribedonly = 0;
    gui.showgrid = 1;
    gui.showdata = 1;

    SIOWriteMessage(stage_conn, 0.0, STG_HDR_GUI, (char*)&gui, sizeof(gui)) ;

  
    // add a root object into the world
    rootmodel.parent_id = -1;
    strncpy(rootmodel.token, "box", STG_TOKEN_MAX);

    rootmodel.id = CreateModel(&rootmodel);

    pthread_mutex_init(&stage_conn_mutex,NULL);
    StartThread();
    initdone = true;
  }

  // acquire the lock on the socket
  pthread_mutex_lock(&stage_conn_mutex);
  
  // add ourselves into the world
  stage_model.parent_id = parent;
  if(!stage_model.parent_id)
    stage_model.parent_id = rootmodel.id;
  // TODO: convert from Player interface name to Stage token name
  strncpy(stage_model.token, interface, STG_TOKEN_MAX);
  
  stage_model.id = CreateModel(&stage_model);
  
  // let Stage know that we're done
  SIOWriteMessage(stage_conn, 0.0, STG_HDR_CONTINUE, NULL, 0 );
  
  // release the lock on the socket
  pthread_mutex_unlock(&stage_conn_mutex);
}

// initialization function
CDevice* 
Stage_Init(char* interface, ConfigFile* cf, int section)
{
  // TODO: get config file args out and store them somewhere
  // TODO: figure out how much space is actually required for these buffers
  return((CDevice*)(new StageDevice(cf->entities[section].parent, interface,
                                    PLAYER_MAX_PAYLOAD_SIZE,
                                    PLAYER_MAX_PAYLOAD_SIZE,
                                    1,1)));
}

///////////////////////////////////////////////////////////////////////////
// Initialise the device
//
int StageDevice::Setup()
{
  // subscribe to the sonar's data, pose, size and rects.
  stage_buffer_t* props;
  assert(props = SIOCreateBuffer());
  int subs[4];
  subs[0] = STG_PROP_ENTITY_DATA;
  subs[1] = STG_PROP_ENTITY_POSE;
  subs[2] = STG_PROP_ENTITY_SIZE;
  subs[3] = STG_PROP_ENTITY_RECTS;

  SIOBufferProperty(props, stage_model.id, STG_PROP_ENTITY_SUBSCRIBE, 
                     (char*)subs, 4*sizeof(subs[0]));

  pthread_mutex_lock(&stage_conn_mutex);
  SIOWriteMessage(stage_conn, 0.0, STG_HDR_PROPS, props->data, props->len);
  SIOWriteMessage(stage_conn, 0.0, STG_HDR_CONTINUE, NULL, 0);
  pthread_mutex_unlock(&stage_conn_mutex);
  
  return(0);
}


///////////////////////////////////////////////////////////////////////////
// Terminate the device
//
int StageDevice::Shutdown()
{
  // Reset the subscribed flag
  //m_info->subscribed--;
  return 0;
};

int
StageDevice::HandleModel(int conn, char* data, size_t len)
{
  stage_model_t* model = (stage_model_t*)data;
  
  // if we see the key we've been waiting for, the model request is
  // confirmed
  if(model->key == pending_key )
    pending_id = model->id; // find out what id we were given
  return(0);
}

int
StageDevice::HandleProperty(int conn, char* data, size_t len)
{
  stage_property_t* prop = (stage_property_t*)data;

  printf( "Received %d bytes  property (%d,%s,%d) on connection %d\n",
          (int)len, prop->id, SIOPropString(prop->property), (int)prop->len, 
          stage_conn);
  return(0); //success
}

// this one's called from the StageDevice constructor, and thus has the
// context of the server read thread.
int
StageDevice::HandleLostConnection2(int conn)
{
  // TODO: this doesn't work, cause we don't have CDevice object context;
  //       need some other mechanism to kill the Stage interaction thread
  //StopThread();
  return(0);
}

// this one's called from the Stage interaction thread
int
StageDevice::HandleLostConnection(int conn)
{
  pthread_exit(NULL);
  return(0);
}

// Main function for device thread
//
void 
StageDevice::Main()
{
  for(;;)
  {
    pthread_mutex_lock(&stage_conn_mutex);
    // interact with Stage
    SIOServiceConnections(&StageDevice::HandleLostConnection,
                          NULL,
                          &StageDevice::HandleModel, 
                          &StageDevice::HandleProperty,
                          NULL);
    SIOWriteMessage(stage_conn, 0.0, STG_HDR_CONTINUE, NULL, 0);
    pthread_mutex_unlock(&stage_conn_mutex);
  }
}

int 
StageDevice::CreateModel(stage_model_t* model)
{
  // set up the request packet and local state variables for getting the reply
  model->id = pending_id = -1; // CREATE a model
  model->key = pending_key = rand(); //(some random integer number)

  SIOWriteMessage(stage_conn, 0.0, STG_HDR_MODEL, 
                  (char*)model, sizeof(stage_model_t));

  // loop on read until we hear that our model was created
  while(pending_id == -1)
  {
    SIOServiceConnections(&StageDevice::HandleLostConnection2,
                          NULL,
                          &StageDevice::HandleModel, 
                          &StageDevice::HandleProperty,
                          NULL);
  }
  return(pending_id);
}

///////////////////////////////////////////////////////////////////////////
// Read data from the device
//
size_t StageDevice::GetData(void* client,unsigned char *data, size_t size,
                        uint32_t* timestamp_sec, 
                        uint32_t* timestamp_usec)
{
  /*
#ifdef DEBUG
  printf( "P: getting (%d,%d,%d) info at %p, data at %p, buffer len %d, %d bytes available, size parameter %d\n", 
          m_info->player_id.port, 
          m_info->player_id.code, 
          m_info->player_id.index, 
          m_info, device_data,
          m_info->data_len,
          m_info->data_avail,
          size );
  fflush( stdout );
#endif

  // See if there is any data
  //
  size_t data_avail = m_info->data_avail;

  // Check for overflows 1
  //
  if (data_avail > PLAYER_MAX_MESSAGE_SIZE )
  {
    printf( "Available data (%d bytes) is larger than Player's"
            " maximum message size (%d bytes)\n", 
            data_avail, PLAYER_MAX_MESSAGE_SIZE );
  }

  // Check for overflows 2
  //
  if (data_avail > device_datasize )
  {
    printf("warning: available data (%d bytes) > buffer size (%d bytes); ignoring data\n", data_avail, device_datasize );
    Unlock();
    return 0;
    //data_avail = m_data_len;
  }
    
  // Check for overflows 3
  //
  if( data_avail > size)
  {
    printf("warning data available (%d bytes) > space in Player packet (%d bytes); ignoring data\n", data_avail, size );
    Unlock();
    return 0;
  }
    
  // Copy the data
  memcpy(data, device_data, data_avail);

  // store the timestamp in the device, because other devices may
  // wish to read it
  data_timestamp_sec = m_info->data_timestamp_sec;
  data_timestamp_usec = m_info->data_timestamp_usec;

  // also return the timestamp to the caller
  if(timestamp_sec)
    *timestamp_sec = data_timestamp_sec;
  if(timestamp_usec)
    *timestamp_usec = data_timestamp_usec;
    
  return data_avail;
  */
  return(0);
}


///////////////////////////////////////////////////////////////////////////
// Write a command to the device
//
void StageDevice::PutCommand(void* client,unsigned char *command, size_t len)
{
  /*
#ifdef DEBUG
  printf( "P: StageDevice::PutCommand (%d,%d,%d) info at %p,"
	  " command at %p\n", 
          m_info->player_id.port, 
          m_info->player_id.code, 
          m_info->player_id.index, 
          m_info, command);
  fflush( stdout );
#endif

  // Check for overflows
  if (len > device_commandsize)
    PLAYER_ERROR("invalid command length; ignoring command");
    
  // Copy the command
  memcpy(device_command, command, len);

  // Set flag to indicate command has been set
  m_info->command_avail = len;

  // set timestamp for this command
  struct timeval tv;
  GlobalTime->GetTime(&tv);

  m_info->command_timestamp_sec = tv.tv_sec;
  m_info->command_timestamp_usec = tv.tv_usec;
  */
}
    

