import time
from codrone_edu.drone import *

drone = Drone()
drone.pair()



drone.reset_gyro()
time.sleep(2)
drone.takeoff()

drone.send_absolute_position(-1.23, 0, 0.56, velocity=0.52, heading=0, rotationalVelocity=0)
drone.hover(0.2)
#la cos

drone.send_absolute_position(0.50, -1.800, 0.50, velocity=0.42, heading=0, rotationalVelocity=0) #prima poz
drone.send_absolute_position(0.42, -1.700, 0.55, velocity=0.35, heading=0, rotationalVelocity=0) #prima poz

drone.hover(0.3)
#drone.send_absolute_position(-0.6, -1.60, 1.2, velocity=0.35, heading=0, rotationalVelocity=0)

drone.send_absolute_position(-1.30, -1.600, 1.33, velocity=0.25, heading=0, rotationalVelocity=0)
drone.hover(0.1)
drone.send_absolute_position(-1.30, -1.37, 1.75, velocity=0.5, heading=0, rotationalVelocity=0)
drone.hover(0.1)
drone.send_absolute_position(0.5, -1.25, 1.90, velocity=0.25, heading=0, rotationalVelocity=0)

#drone.send_absolute_position(0, 0, 1.8, velocity=0.01, heading=0, rotationalVelocity=0)
#drone.land()
#drone.takeoff()
#drone.hover(1)
drone.land()
drone.close()
