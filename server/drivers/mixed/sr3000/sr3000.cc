/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2006 Radu Bogdan Rusu (rusu@cs.tum.edu)
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

/** @ingroup drivers */
/** @{ */
/** @defgroup driver_sr3000 sr3000
 * @brief SR3000

The sr3000 driver controls the Swiss Ranger SR3000 camera. A broad range of
camera option parameters are supported, via the libusbSR library. The driver
provides a @ref interface_pointcloud3d interface and two @ref interface_camera
interfaces for both distance and intensity images.

@par Compile-time dependencies

- none

@par Provides

- @ref interface_pointcloud3d : the 3d point cloud generated by the SR3000
- @ref interface_camera : snapshot images (both distance and intensity) taken by
                          the SR3000

@par Requires

- none

@par Supported configuration requests

  - none

@par Properties provided

  - auto_illumination (integer): Set to 1 to turn auto illumination on.
  - integration_time (integer): Integration time.
  - modulation_freq (integer): This device employs the following values:
                                40MHz  -> 3.75 m,
                                30MHz  -> 5.0  m,
                                21MHz  -> 7.1  m,
                                20MHz  -> 7.5  m,
                                19MHz  -> 7.9  m,
                                10MHz  -> 15.0 m,
                                6.6MHz -> 22.5 m,
                                5MHz   -> 30.0 m
  - sat_threshold (integer): Saturation threshold.
  - amp_threshold (integer): Amplification threshold.
  - static_delay (double): Temporal IIR static delay (0.0-1.0).
  - dynamic_delay (double): Temporal IIR dynamic delay (0.0-1.0).

@par Configuration file options

  - none

@par Example

@verbatim
driver
(
  name "sr3000"
  provides ["pointcloud3d:0" "distance:::camera:0" "intensity:::camera:1"]
)
@endverbatim

@author Radu Bogdan Rusu
 */
/** @} */

#include <stdlib.h>
#include <libplayercore/playercore.h>
#include "libusbSR.h"

#define MODE (AM_COR_FIX_PTRN | AM_COR_LED_NON_LIN | AM_MEDIAN)
#define CAM_ROWS 144
#define CAM_COLS 176

class SR3000:public Driver
{
  public:
	// constructor
    SR3000 (ConfigFile* cf, int section);
    ~SR3000 ();

    int Setup ();
    int Shutdown ();

    // MessageHandler
    virtual int ProcessMessage (MessageQueue * resp_queue,
                                player_msghdr * hdr,
                                void * data);

  private:

    // Camera MessageHandler
    int ProcessMessageCamera (MessageQueue* resp_queue,
                              player_msghdr * hdr,
                              void * data,
                              player_devaddr_t cam_addr);
    virtual void Main ();
    void RefreshData  ();

    // device identifier
    SwissrangerCam* srCam;

    // SR3000 specific values
    unsigned int rows, cols, bpp, inr;
    unsigned char integration_time;
    //ModulationFrq modulation_freq;
    size_t buffer_size, buffer_points_size;
    void *buffer;
    float *buffer_points, *xp, *yp, *zp;

    // Properties
    IntProperty auto_illumination, integration_time, modulation_freq, sat_threshold, amp_threshold;
    DoubleProperty static_delay, dynamic_delay;

    // device bookkeeping
    player_devaddr_t pcloud_addr;
    player_devaddr_t d_cam_addr, i_cam_addr;
    player_pointcloud3d_data_t pcloud_data;
    player_camera_data_t       d_cam_data, i_cam_data;
    bool providePCloud, provideDCam, provideICam;
};

////////////////////////////////////////////////////////////////////////////////
//Factory creation function. This functions is given as an argument when
// the driver is added to the driver table
Driver*
    SR3000_Init (ConfigFile* cf, int section)
{
  return ((Driver*)(new SR3000 (cf, section)));
}

////////////////////////////////////////////////////////////////////////////////
//Registers the driver in the driver table. Called from the
// player_driver_init function that the loader looks for
void
    SR3000_Register (DriverTable* table)
{
  table->AddDriver ("sr3000", SR3000_Init);
}

////////////////////////////////////////////////////////////////////////////////
// Constructor.  Retrieve options from the configuration file and do any
// pre-Setup() setup.
SR3000::SR3000 (ConfigFile* cf, int section)
	: Driver (cf, section),
	auto_illumination (0), integration_time (0), modulation_freq (0),
	sat_threshold (0), amp_threshold (0), static_delay (0), dynamic_delay (0)
{
  memset (&this->pcloud_addr, 0, sizeof (player_devaddr_t));
  memset (&this->d_cam_addr,  0, sizeof (player_devaddr_t));
  memset (&this->i_cam_addr,  0, sizeof (player_devaddr_t));

  this->RegisterProperty ("auto_illumination", &this->auto_illumination, cf, section);
  this->RegisterProperty ("integration_time", &this->integration_time, cf, section);
  this->RegisterProperty ("modulation_freq", &this->modulation_freq, cf, section);
  this->RegisterProperty ("sat_threshold", &this->sat_threshold, cf, section);
  this->RegisterProperty ("amp_threshold", &this->amp_threshold, cf, section);
  this->RegisterProperty ("static_delay", &this->static_delay, cf, section);
  this->RegisterProperty ("dynamic_delay", &this->dynamic_delay, cf, section);

  providePCloud = FALSE; provideDCam = FALSE; provideICam = FALSE;

  // Outgoing pointcloud interface
  if (cf->ReadDeviceAddr (&(this->pcloud_addr), section, "provides",
      PLAYER_POINTCLOUD3D_CODE, -1, NULL) == 0)
  {
    if (this->AddInterface (this->pcloud_addr) != 0)
    {
      this->SetError (-1);
      return;
    }
    providePCloud = TRUE;
  }

  // Outgoing distance::camera:0 interface
  if (cf->ReadDeviceAddr (&(this->d_cam_addr), section, "provides",
      PLAYER_CAMERA_CODE, -1, "distance") == 0)
  {
	if (this->AddInterface (this->d_cam_addr) != 0)
	{
      this->SetError (-1);
      return;
	}
    provideDCam = TRUE;
  }

  // Outgoing intensity::camera:1 interface
  if (cf->ReadDeviceAddr (&(this->i_cam_addr), section, "provides",
      PLAYER_CAMERA_CODE, -1, "intensity") == 0)
  {
    if (this->AddInterface (this->i_cam_addr) != 0)
    {
      this->SetError (-1);
      return;
    }
    provideICam = TRUE;
  }

}

////////////////////////////////////////////////////////////////////////////////
// Destructor.
SR3000::~SR3000 ()
{
}

////////////////////////////////////////////////////////////////////////////////
// Set up the device.  Return 0 if things go well, and -1 otherwise.
int
    SR3000::Setup ()
{
  int res;
  // ---[ Open the camera ]---
  res = SR_Open (&srCam);          //returns the device ID used in other calls

  PLAYER_MSG0 (1, "> Connecting to SR3000... [done]");

  // ---[ Get the number of rows, cols, bpp, ... ]---
  rows = SR_GetRows (srCam);
  cols = SR_GetCols (srCam);
  bpp  = SR_GetBytePerPix (srCam);
  inr  = SR_GetNumImg (srCam);
  modulation_freq  = SR_GetModulationFrequency (srCam);
  integration_time = SR_GetIntegrationTime (srCam);
  buffer_size      = SR_GetBufferSize (srCam);
  PLAYER_MSG5 (2, ">> Expecting %dx%dx%dx%d (%d bytes)", cols, rows, bpp, inr, buffer_size);

  if ((cols != CAM_COLS) || (rows != CAM_ROWS) || ((unsigned int)buffer_size < 0))
  {
    PLAYER_ERROR ("> Error while connecting to camera!");
    SR_Close (srCam);
    return (-1);
  }

  // ---[ Alloc memory for the buffer ]---
  buffer = malloc (buffer_size);
  memset (buffer, 0, buffer_size);

  // ---[ Set the buffer ]---
  SR_SetBuffer (srCam, buffer, buffer_size);

  buffer_points_size = rows * cols * 3 * sizeof (float);
  buffer_points      = (float*)malloc (buffer_points_size);
  memset (buffer_points, 0, buffer_points_size);
  xp = buffer_points;
  yp = &xp[rows*cols];
  zp = &yp[rows*cols];

  StartThread ();
  return (0);
}


////////////////////////////////////////////////////////////////////////////////
// Shutdown the device
int
    SR3000::Shutdown ()
{
  StopThread ();

  // ---[ Close the camera ]---
  int res = SR_Close (srCam);

  PLAYER_MSG1 (1, "> SR3000 driver shutting down... %d [done]", res);

  // ---[ Free the allocated memory buffer ]---
  free (buffer);
  free (buffer_points);

  return (0);
}

////////////////////////////////////////////////////////////////////////////////
// Process messages from/for a camera interface
int
    SR3000::ProcessMessageCamera (MessageQueue* resp_queue,
                                  player_msghdr * hdr,
                                  void * data,
                                  player_devaddr_t cam_addr)
{
  int res;
  Property *property = NULL;

  // Check for properties
  if (Message::MatchMessage (hdr, PLAYER_MSGTYPE_REQ, PLAYER_SET_INTPROP_REQ, cam_addr))
  {
    player_intprop_req_t req = *reinterpret_cast<player_intprop_req_t*> (data);
    if ((property = propertyBag.GetProperty (req.key)) == NULL)
      return (-1);
    if (property->KeyIsEqual ("auto_illumination"))
    {
      // ---[ Set Autoillumination
      if (req->value == 1)
        res = SR_SetAutoIllumination (srCam, 5, 255, 10, 45);
      else
        res = SR_SetAutoIllumination (srCam, 255, 0, 0, 0);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    else if (property->KeyIsEqual ("integration_time")
    {
      // ---[ Set integration time
      res = SR_SetIntegrationTime (srCam, req->value);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    else if (property->KeyIsEqual ("modulation_freq")
    {
      // ---[ Set modulation frequency
      res = SR_SetModulationFrequency (srCam, (ModulationFrq)req->value);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    else if (property->KeyIsEqual ("sat_threshold")
    {
      // ---[ Set saturation threshold
      res = SR_SetSaturationThreshold (srCam, req->value);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    else if (property->KeyIsEqual ("amp_threshold")
    {
      // ---[ Set amplitude threshold
      res = SR_SetAmplitudeThreshold (srCam, req->value);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_INTPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    return (-1);    // Let the default property handling handle it
  }
  else if(Message::MatchMessage(hdr, PLAYER_MSGTYPE_REQ, PLAYER_GET_INTPROP_REQ, device_addr))
  {
    player_intprop_req_t req = *reinterpret_cast<player_intprop_req_t*> (data);
    if ((property = propertyBag.GetProperty (req.key)) == NULL)
      return (-1);
    if (property->KeyIsEqual ("modulation_freq"))
    {
      // ---[ Get modulation frequency
      reinterpret_cast<IntProperty*> (property)->SetValue (SR_GetModulationFrequency (srCam));
      property->GetValueToMessage (reinterpret_cast<void*> (&req));
      Publish(device_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_GET_INTPROP_REQ, reinterpret_cast<void*> (&req), sizeof(player_intprop_req_t), NULL);
      return (0);
    }
    else if (property->KeyIsEqual ("integration_time"))
    {
      // ---[ Get integration time
      reinterpret_cast<IntProperty*> (property)->SetValue (SR_GetIntegrationTime (srCam));
      property->GetValueToMessage (reinterpret_cast<void*> (&req));
      Publish(device_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_GET_INTPROP_REQ, reinterpret_cast<void*> (&req), sizeof(player_intprop_req_t), NULL);
      return (0);
    }
    return (-1);    // Let the default property handling handle it
  }
  else if (Message::MatchMessage (hdr, PLAYER_MSGTYPE_REQ, PLAYER_SET_DBLPROP_REQ, cam_addr))
  {
    player_dblprop_req_t req = *reinterpret_cast<player_dblprop_req_t*> (data);
    if ((property = propertyBag.GetProperty (req.key)) == NULL)
      return (-1);
    if (property->KeyIsEqual ("static_delay"))
    {
      // ---[ Set IIR static delay
      DoubleProperty *dynamic_delay = propertyBag.GetProperty ("dynamic_delay");
      res = SR_SetTemporalIIR (srCam, req->value, dynamic_delay);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_DBLPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_DBLPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    else if (property->KeyIsEqual ("dynamic_delay"))
    {
      // ---[ Set IIR dynamic delay
      DoubleProperty *static_delay = propertyBag.GetProperty ("static_delay");
      res = SR_SetTemporalIIR (srCam, static_delay, req->value);

      // Check the error code
      if (res == 0)
      {
        property->SetValueFromMessage (reinterpret_cast<void*> (&req));
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_ACK, PLAYER_SET_DBLPROP_REQ, NULL, 0, NULL);
      }
      else
      {
        Publish(cam_addr, resp_queue, PLAYER_MSGTYPE_RESP_NACK, PLAYER_SET_DBLPROP_REQ, NULL, 0, NULL);
      }
      return (0);
    }
    return (-1);    // Let the default property handling handle it
  }

  return (0);
}

////////////////////////////////////////////////////////////////////////////////
// ProcessMessage
int
    SR3000::ProcessMessage (MessageQueue* resp_queue,
                            player_msghdr * hdr,
                            void * data)
{
  assert (hdr);
  assert (data);

  ProcessMessageCamera (resp_queue, hdr, data, d_cam_addr);
  ProcessMessageCamera (resp_queue, hdr, data, i_cam_addr);

  return (0);
}



////////////////////////////////////////////////////////////////////////////////
void SR3000::Main ()
{
  timespec sleepTime = {0, 10000};

  memset (&pcloud_data, 0, sizeof (pcloud_data));
  memset (&d_cam_data,  0, sizeof (d_cam_data ));
  memset (&i_cam_data,  0, sizeof (i_cam_data ));

  for (;;)
  {
    // handle commands and requests/replies --------------------------------
    pthread_testcancel ();
    ProcessMessages ();

    // get data ------------------------------------------------------------
    if ((rows != 1) && (cols != 1))
      RefreshData ();
    nanosleep (&sleepTime, NULL);
  }
}

////////////////////////////////////////////////////////////////////////////////
// RefreshData function
void SR3000::RefreshData ()
{
  int res;
  unsigned int i;

  res = SR_Acquire (srCam, MODE);

  // Publish pointcloud3d data if subscribed
  if (providePCloud)
  {
    res = SR_CoordTrfFlt (srCam, xp, yp, zp, sizeof (float), sizeof (float), sizeof (float));

    pcloud_data.points_count = rows * cols;
    for (i = 0; i < rows*cols; i++)
    {
      player_pointcloud3d_element_t element;
      element.point.px = xp[i];
      element.point.py = yp[i];
      element.point.pz = zp[i];

      element.color.alpha = 255;
      element.color.red   = 255;
      element.color.green = 255;
      element.color.blue  = 255;
      pcloud_data.points[i] = element;
    }
    // Write the Pointcloud3d data
    Publish (pcloud_addr, NULL, PLAYER_MSGTYPE_DATA, PLAYER_POINTCLOUD3D_DATA_STATE,
             &pcloud_data, 4 + pcloud_data.points_count*sizeof (player_pointcloud3d_element_t),
             NULL);

  }

  // Publish distance camera data if subscribed
  if (provideDCam)
  {
    d_cam_data.width       = cols;
    d_cam_data.height      = rows;
    d_cam_data.bpp         = 16;
    d_cam_data.format      = PLAYER_CAMERA_FORMAT_MONO16;
    d_cam_data.fdiv        = 1;
    d_cam_data.compression = PLAYER_CAMERA_COMPRESS_RAW;
    d_cam_data.image_count = rows*cols*2;
    memcpy (d_cam_data.image, (unsigned char*)buffer, rows*cols*2);

    // Write the distance camera data
    Publish (d_cam_addr, NULL, PLAYER_MSGTYPE_DATA, PLAYER_CAMERA_DATA_STATE,
             &d_cam_data, 28+(d_cam_data.image_count), NULL);
  }

  // Publish intensity camera data if subscribed
  if (provideICam)
  {
    i_cam_data.width       = cols;
    i_cam_data.height      = rows;
    i_cam_data.bpp         = 16;
    i_cam_data.format      = PLAYER_CAMERA_FORMAT_MONO16;
    i_cam_data.fdiv        = 1;
    i_cam_data.compression = PLAYER_CAMERA_COMPRESS_RAW;
    i_cam_data.image_count = rows*cols*2;
    memcpy (i_cam_data.image, (unsigned char*)buffer + buffer_size/2, rows*cols*2);

    // Write the intensity camera data
    Publish (i_cam_addr, NULL, PLAYER_MSGTYPE_DATA, PLAYER_CAMERA_DATA_STATE,
             &i_cam_data, 28+(i_cam_data.image_count), NULL);
  }

  return;
}
