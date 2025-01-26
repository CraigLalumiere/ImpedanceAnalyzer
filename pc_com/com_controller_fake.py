from com_controller import ComController
import messages.MotorDataLog_pb2
import time
import numpy as np

class ComControllerFake(ComController):
    def __init__(self):
        super(ComControllerFake, self).__init__()
        self.tick_0 = int(time.time_ns()/1000000)

    def get_received_messages(self):
        """
        Returns fake list of received messages
        """
        msg1 = messages.MotorDataLog_pb2.MotorDataLog()
        msg1.motor_id = 0
        msg1.milliseconds_tick =  int(time.time_ns()/1000000 - self.tick_0)
        msg1.motor_current = np.random.normal()


        msg2 = messages.MotorDataLog_pb2.MotorDataLog()
        msg2.motor_id = 1
        msg2.milliseconds_tick =  int(time.time_ns()/1000000 - self.tick_0)
        msg2.motor_current = np.random.normal()

        msg3 = messages.MotorDataLog_pb2.MotorDataLog()
        msg3.motor_id = 2
        msg3.milliseconds_tick =  int(time.time_ns()/1000000 - self.tick_0)
        msg3.motor_current = np.random.normal()


        received_massages = [msg1, msg2, msg3]
        return received_massages
