/*
 *  Player - One Hell of a Robot Server
 *  Copyright (C) 2000  Brian Gerkey   &  Kasper Stoy
 *                      gerkey@usc.edu    kaspers@robotics.usc.edu
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
///////////////////////////////////////////////////////////////////////////
//
// Desc: Driver for extracting line/corner features from a laser scan.
// Author: Andrew Howard
// Date: 29 Aug 2002
// CVS: $Id$
//
// Theory of operation:
//  TODO
//
// Requires:
//   Laser device.
//
///////////////////////////////////////////////////////////////////////////

#include <errno.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>       // for atoi(3)
#include <netinet/in.h>   // for htons(3)
#include <unistd.h>

#include "player.h"
#include "device.h"
#include "devicetable.h"
#include "drivertable.h"


// Driver for detecting features in laser scan.
class LaserFeature : public CDevice
{
  // Constructor
  public: LaserFeature(char* interface, ConfigFile* cf, int section);

  // Setup/shutdown routines.
  public: virtual int Setup();
  public: virtual int Shutdown();

  // Client interface (this device has no thread).
  public: virtual size_t GetData(unsigned char *dest, size_t maxsize,
                                 uint32_t* timestamp_sec, uint32_t* timestamp_usec);

  // Client interface (this device has no thread).
  public: virtual int PutConfig(player_device_id_t* device, void *client, 
                                void *data, size_t len);

  // Process laser data.  Returns non-zero if the laser data has been
  // updated.
  private: int UpdateLaser();

  // Write laser data to a file (testing).
  private: int WriteLaser();

  // Read laser data from a file (testing).
  private: int ReadLaser();

  // Segment the scan into straight-line segments.
  private: void SegmentLaser();

  // Update the line filter.  Returns an error signal.
  private: double UpdateFilter(double x[2], double P[2][2], double Q[2][2],
                               double R, double z, double res);

  // Update the device data (the data going back to the client).
  private: void UpdateData();

  // Process requests.  Returns 1 if the configuration has changed.
  private: int HandleRequests();

  // Handle geometry requests.
  private: void HandleGetGeom(void *client, void *req, int reqlen);

  // Device pose relative to robot.
  private: double pose[3];

  // Laser stuff.
  private: int laser_index;
  private: CDevice *laser_device;
  private: player_laser_data_t laser_data;
  private: uint32_t laser_timesec, laser_timeusec;

  // TESTING
  private: FILE *laser_file;

  // Fiducila stuff (the data we generate).
  private: player_fiducial_data_t data;
  private: uint32_t timesec, timeusec;
};


// Initialization function
CDevice* LaserFeature_Init(char* interface, ConfigFile* cf, int section)
{
  if (strcmp(interface, PLAYER_FIDUCIAL_STRING) != 0)
  {
    PLAYER_ERROR1("driver \"laserfeature\" does not support interface \"%s\"\n",
                  interface);
    return (NULL);
  }
  return ((CDevice*) (new LaserFeature(interface, cf, section)));
}


// a driver registration function
void LaserFeature_Register(DriverTable* table)
{
  table->AddDriver("laserfeature", PLAYER_READ_MODE, LaserFeature_Init);
}


////////////////////////////////////////////////////////////////////////////////
// Constructor
LaserFeature::LaserFeature(char* interface, ConfigFile* cf, int section)
    : CDevice(sizeof(player_fiducial_data_t), 0, 0, 0)
{
  // Device pose relative to robot.
  this->pose[0] = 0;
  this->pose[1] = 0;
  this->pose[2] = 0;

  // If laser_index is not overridden by an argument here, then we'll
  // use the device's own index, which we can get in Setup() below.
  this->laser_index = cf->ReadInt(section, "laser", -1);
  this->laser_device = NULL;
  this->laser_timesec = 0;
  this->laser_timeusec = 0;

  return;
}


////////////////////////////////////////////////////////////////////////////////
// Set up the device (called by server thread).
int LaserFeature::Setup()
{
  player_device_id_t id;

  // Subscribe to the laser.
  id.code = PLAYER_LASER_CODE;
  id.index = (this->laser_index >= 0 ? this->laser_index : this->device_id.index);
  id.port = this->device_id.port;
  this->laser_device = deviceTable->GetDevice(id);
  if (!this->laser_device)
  {
    PLAYER_ERROR("unable to locate suitable laser device");
    return(-1);
  }
  if (this->laser_device->Subscribe(this) != 0)
  {
    PLAYER_ERROR("unable to subscribe to laser device");
    return(-1);
  }

  // Get the laser geometry.
  // TODO: no support for this at the moment.
  this->pose[0] = 0.10;
  this->pose[1] = 0;
  this->pose[2] = 0;

  // TESTING
  this->laser_file = fopen("laser.log", "w+");
  assert(this->laser_file);
  
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Shutdown the device (called by server thread).
int LaserFeature::Shutdown()
{
  // Unsubscribe from devices.
  this->laser_device->Unsubscribe(this);

  // TESTING
  fclose(this->laser_file);
  
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Get data from buffer (called by client thread)
size_t LaserFeature::GetData(unsigned char *dest, size_t maxsize,
                             uint32_t* timesec, uint32_t* timeusec)
{
  // Get the current laser data.
  this->laser_device->GetData((uint8_t*) &this->laser_data, sizeof(this->laser_data),
                              &this->laser_timesec, &this->laser_timeusec);
  
  // If there is new laser data, update our data.  Otherwise, we will
  // just reuse the existing data.
  if (this->laser_timesec != this->timesec || this->laser_timeusec != this->timeusec)
  {
    this->UpdateLaser();
    this->UpdateData();
  }
  
  // Copy results
  ASSERT(maxsize >= sizeof(this->data));
  memcpy(dest, &this->data, sizeof(this->data));

  // Copy the laser timestamp
  this->timesec = this->laser_timesec;
  this->timeusec = this->laser_timeusec;
  *timesec = this->timesec;
  *timeusec = this->timeusec;

  return (sizeof(this->data));
}


////////////////////////////////////////////////////////////////////////////////
// Process laser data.
int LaserFeature::UpdateLaser()
{
  int i;
  
  // Do some byte swapping on the laser data.
  this->laser_data.resolution = ntohs(this->laser_data.resolution);
  this->laser_data.min_angle = ntohs(this->laser_data.min_angle);
  this->laser_data.max_angle = ntohs(this->laser_data.max_angle);
  this->laser_data.range_count = ntohs(this->laser_data.range_count);
  for (i = 0; i < this->laser_data.range_count; i++)
    this->laser_data.ranges[i] = ntohs(this->laser_data.ranges[i]);

  //this->ReadLaser();
  this->WriteLaser();
    
  // Segment the scan into straight-line segments.
  this->SegmentLaser();
  
  return 1;
}


////////////////////////////////////////////////////////////////////////////////
// Write laser data to a file (testing).
int LaserFeature::WriteLaser()
{
  int i;
  
  fprintf(this->laser_file, "%d %d %d %d ",
          this->laser_data.resolution,
          this->laser_data.min_angle,
          this->laser_data.max_angle,
          this->laser_data.range_count);
    
  for (i = 0; i < this->laser_data.range_count; i++)
    fprintf(this->laser_file, "%d ", this->laser_data.ranges[i]);

  fprintf(this->laser_file, "\n");
  
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Read laser data from a file (testing).
int LaserFeature::ReadLaser()
{
  int i;

  fscanf(this->laser_file, "%hd %hd %hd %hd ",
         &this->laser_data.resolution,
         &this->laser_data.min_angle,
         &this->laser_data.max_angle,
         &this->laser_data.range_count);
    
  for (i = 0; i < this->laser_data.range_count; i++)
    fscanf(this->laser_file, "%hd ", &this->laser_data.ranges[i]);

  fscanf(this->laser_file, "\n");

  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Segment the scan into straight-line segments.
void LaserFeature::SegmentLaser()
{
  int i;
  double r, b;
  double res;
  double x[2], P[2][2];
  double Q[2][2], R;
  double err;

  static int test_count = 0;

  // Angle between successive laser readings.
  res = (double) (this->laser_data.resolution) / 100.0 * M_PI / 180;

  double n = (test_count++) * 0.0001;
  
  // System noise.
  Q[0][0] = n * n;
  Q[0][1] = n * n;
  Q[1][0] = n * n;
  Q[1][1] = n * n;

  // Sensor noise.
  R = 0.02 * 0.02;

  // Initial estimate and covariance.
  x[0] = 0.0;
  x[1] = 0.0;
  P[0][0] = 100;
  P[0][1] = 0.0;
  P[1][0] = 0.0;
  P[1][1] = 100;

  printf("# Q = %f\n", n);
  
  for (i = 0; i < this->laser_data.range_count; i++)
  {
    r = (double) (this->laser_data.ranges[i]) / 1000;
    b = (double) (this->laser_data.min_angle) / 100.0 * M_PI / 180 + i * res;
    
    err = this->UpdateFilter(x, P, Q, R, r, res);

    printf("%f %f %f %f %f\n", r, b, x[0], x[1], err);
  }

  printf("\n\n");

  return;
}


////////////////////////////////////////////////////////////////////////////////
// Update the line filter.  Returns an error signal.
double LaserFeature::UpdateFilter(double x[2], double P[2][2], double Q[2][2],
                                  double R, double z, double res)
{
  int i, j, k, l;
  double x_[2], P_[2][2];
  double F[2][2];
  double r, S;
  double K[2];

  // A priori state estimate.
  x_[0] = sin(x[1]) / sin(x[1] + res) * x[0];
  x_[1] = x[1] + res;

  // Jacobian for the system function.
  F[0][0] = sin(x[1]) / sin(x[1] + res);
  F[0][1] = sin(x[1]) / (sin(x[1] + res) * sin(x[1] + res)) * x[0];
  F[1][0] = 0;
  F[1][1] = 1;
  
  // Covariance of a priori state estimate.
  for (i = 0; i < 2; i++)
  {
    for (j = 0; j < 2; j++)
    {
      P_[i][j] = 0.0;
      for (k = 0; k < 2; k++)
        for (l = 0; l < 2; l++)
          P_[i][j] += F[i][k] * P[k][l] * F[j][l];
      P_[i][j] += Q[i][j];
    }
  }

  // Residual (difference between prediction and measurement).
  r = z - x_[0];

  // Covariance of the residual.
  S = P_[0][0] + R;

  // Kalman gain.
  K[0] = P_[0][0] / S;
  K[1] = P_[1][0] / S;

  // Posterior state estimate.
  x[0] = x_[0] + K[0] * r;
  x[1] = x_[1] + K[1] * r;    

  // Posterior state covariance.
  for (i = 0; i < 2; i++)
    for (j = 0; j < 2; j++)
      P[i][j] = P_[i][j] - K[i] * S * K[j];

  return r * r / S;
}


////////////////////////////////////////////////////////////////////////////////
// Update the device data (the data going back to the client).
void LaserFeature::UpdateData()
{
  this->data.count = 0;

  /*
  for (i = 0; i < this->fiducial_count; i++)
  {
    fiducial = this->fiducials + i;

    // Only report fiducials that where seen in the most recent laser
    // scan.
    if (fiducial->laser_time != this->laser_time)
      continue;
    
    r = sqrt(fiducial->pose[0] * fiducial->pose[0] +
             fiducial->pose[1] * fiducial->pose[1]);
    b = atan2(fiducial->pose[1], fiducial->pose[0]);
    o = fiducial->pose[2];

    data.fiducials[data.count].id = htons(((int16_t) fiducial->id));
    data.fiducials[data.count].pose[0] = htons(((int16_t) (1000 * r)));
    data.fiducials[data.count].pose[1] = htons(((int16_t) (180 * b / M_PI)));
    data.fiducials[data.count].pose[2] = htons(((int16_t) (180 * o / M_PI)));
    data.count++;
  }
  */
  
  this->data.count = htons(this->data.count);
  
  return;
}


////////////////////////////////////////////////////////////////////////////////
// Put configuration in buffer (called by client thread)
int LaserFeature::PutConfig(player_device_id_t* device, void *client, void *data, size_t len) 
{
  uint8_t subtype;

  subtype = ((uint8_t*) data)[0];
  
  switch (subtype)
  {
    case PLAYER_FIDUCIAL_GET_GEOM:
      HandleGetGeom(client, data, len);
      break;
    default:
      if (PutReply(client, PLAYER_MSGTYPE_RESP_NACK) != 0)
        PLAYER_ERROR("PutReply() failed");
      break;
  }
  return 0;
}


////////////////////////////////////////////////////////////////////////////////
// Handle geometry requests.
void LaserFeature::HandleGetGeom(void *client, void *request, int len)
{
  player_fiducial_geom_t geom;

  if (len != 1)
  {
    PLAYER_ERROR2("geometry request len is invalid (%d != %d)", len, 1);
    if (PutReply(client, PLAYER_MSGTYPE_RESP_NACK) != 0)
      PLAYER_ERROR("PutReply() failed");
    return;
  }

  geom.pose[0] = htons((short) (this->pose[0] * 1000));
  geom.pose[1] = htons((short) (this->pose[1] * 1000));
  geom.pose[2] = htons((short) (this->pose[2] * 180/M_PI));

  if (PutReply(client, PLAYER_MSGTYPE_RESP_ACK, NULL, &geom, sizeof(geom)) != 0)
    PLAYER_ERROR("PutReply() failed");

  return;
}


