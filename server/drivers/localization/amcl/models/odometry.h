
/**************************************************************************
 * Desc: Sensor/action models for odometry.
 * Author: Andrew Howard
 * Date: 15 Dec 2002
 * CVS: $Id$
 *************************************************************************/

#ifndef ODOMETRY_H
#define ODOMETRY_H

#include "../pf/pf.h"
#include "../pf/pf_pdf.h"
#include "../map/map.h"


// Model information
typedef struct
{
  // Pointer to the map map
  map_t *map;

  // Odometry poses
  pf_vector_t pose;
  
  // Stall sensor value
  int stall;

  // PDF used to generate initial samples
  pf_pdf_gaussian_t *init_pdf;
  
  // PDF used to generate action samples
  pf_pdf_gaussian_t *action_pdf;

} odometry_t;


// Create an sensor model
odometry_t *odometry_alloc(map_t *map);

// Free an sensor model
void odometry_free(odometry_t *sensor);

// Set the initial pose (initialization model)
void odometry_init_pose(odometry_t *sensor, pf_vector_t pose, pf_matrix_t pose_cov);

// Set the new odometric pose (action model)
void odometry_set_pose(odometry_t *sensor, pf_vector_t old_pose, pf_vector_t new_pose);

// Set the stall bit
void odometry_set_stall(odometry_t *sensor, int stall);

// The initialization model function
pf_vector_t odometry_init_model(odometry_t *sensor);

// The sensor model function
double odometry_sensor_model(odometry_t *sensor, pf_vector_t pose);

// The action model function
pf_vector_t odometry_action_model(odometry_t *sensor, pf_vector_t pose);

#endif

