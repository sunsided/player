/***************************************************************************
 * Desc: Tests for the power device
 * Author: Andrew Howard
 * Date: 26 May 2002
 # CVS: $Id$
 **************************************************************************/

#include "test.h"
#include "playerc.h"


// Basic test for power device.
int test_power(playerc_client_t *client, int robot, int index)
{
  int t;
  void *rdevice;
  playerc_power_t *device;

  printf("device [power] index [%d]\n", index);

  device = playerc_power_create(client, robot, index);

  TEST("subscribing (read/write)");
  if (playerc_power_subscribe(device, PLAYER_ALL_MODE) != 0)
  {
    FAIL();
    return -1;
  }
  PASS();

  for (t = 0; t < 3; t++)
  {
    TEST1("reading data (attempt %d)", t);

    do
      rdevice = playerc_client_read(client);
    while (rdevice == client);

    if (rdevice == device)
    {
      PASS();
      printf("power: [%6.1f] \n",
             device->charge);
    }
    else
      FAIL();
  }
  
  TEST("unsubscribing");
  if (playerc_power_unsubscribe(device) != 0)
  {
    FAIL();
    return -1;
  }
  PASS();
  
  playerc_power_destroy(device);
  
  return 0;
}

