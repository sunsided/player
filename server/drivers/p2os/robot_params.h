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
 * robot_params.h 
 *
 * ActivMedia robot parameters, automatically generated by saphconv.tcl from 
 * Saphira parameter files:
 *    amigo.p
 *    p2at.p
 *    p2ce.p
 *    p2de.p
 *    p2dx.p
 *    p2it.p
 *    p2pb.p
 *    p2pp.p
 *    pion1m.p
 *    pion1x.p
 *    pionat.p
 *    robocup.p
*/

#ifndef _ROBOT_PARAMS_H
#define _ROBOT_PARAMS_H


void initialize_robot_params(void);

#define PLAYER_NUM_ROBOT_TYPES 12


typedef struct
{
  double x;
  double y;
  double th;
} sonar_pose_t;


typedef struct
{
  double AngleConvFactor; // 
  char* Class;
  double DiffConvFactor; // 
  double DistConvFactor; // 
  int FrontBuffer; // 
  int Holonomic; // 
  double MaxRVelocity; // 
  double MaxVelocity; // 
  int QuickBuffer; // 
  double RangeConvFactor; // 
  double RobotDiagonal; // 
  double RobotRadius; // 
  int SideBuffer; // 
  int SonarNum; // 
  char* Subclass;
  double Vel2Divisor; // 
  double VelConvFactor; // 
  sonar_pose_t sonar_pose[24];
} RobotParams_t;


extern RobotParams_t PlayerRobotParams[];


#endif
