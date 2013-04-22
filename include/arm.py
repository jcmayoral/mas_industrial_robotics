import math

PACKAGE = 'raw_generic_states'

import roslib
roslib.load_manifest(PACKAGE)

import tf
import rospy
import numpy as np
from geometry_msgs.msg import Point, Quaternion
from actionlib import SimpleActionClient
from simple_script_server import simple_script_server
from arm_navigation_msgs.msg import MoveArmAction, MoveArmGoal, \
                                    PositionConstraint, OrientationConstraint


class Arm(object):

    CART_SERVER = '/arm_controller/move_arm_cart'
    ARM_LINK_5_TO_GRIPPER_OFFSET = np.array([-0.004, -0.002, -0.076, 1])

    def __init__(self, planning_mode=''):
        self.move_arm_cart_server = SimpleActionClient(self.CART_SERVER,
                                                       MoveArmAction)
        self.script_server = simple_script_server()
        self.planning_mode = planning_mode
        self.blocking = True

    def move_to(self, where, blocking=True):
        """
        Move the arm to the pose specified in 'where'.
        Supported formats:
            * list/tuple with 5 numbers:
                It is treated as a joint state.
            * string:
                It is treated as a pose name (should be on the parameter
                server).
            * list/tuple with first element string and 6 numbers:
                It is treated as a cartesian pose. First item is the frame of
                reference, the remaining numbers are x, y, z, roll, pitch, yaw.
        """
        self.blocking = blocking
        if isinstance(where, str):
            self._move_to_pose(where)
        elif (len(where) == 5 and
              all(isinstance(x, (float, int)) for x in where)):
            self._move_to_joints(where)
        elif ((len(where) == 7) and
              isinstance(where[0], str) and
              all(isinstance(x, (float, int)) for x in where[1:])):
            self._move_to_cartesian(where)
        else:
            raise Exception('Unsupported arm target pose format (%s)' % where)

    def _move_to_joints(self, joints):
        self.script_server.move('arm', [joints], mode=self.planning_mode,
                                blocking=self.blocking)

    def _move_to_cartesian(self, coordinates):
        rospy.logdebug('Waiting for [%s] server...' % (self.CART_SERVER))
        self.move_arm_cart_server.wait_for_server()
        position, orientation = self._construct_target_pose(coordinates[1:])
        g = MoveArmGoal()
        pc = PositionConstraint()
        pc.header.frame_id = coordinates[0]
        pc.header.stamp = rospy.Time.now()
        pc.position = position
        g.motion_plan_request.goal_constraints.position_constraints.append(pc)
        oc = OrientationConstraint()
        oc.header.frame_id = coordinates[0]
        oc.header.stamp = rospy.Time.now()
        oc.orientation = orientation
        g.motion_plan_request.goal_constraints.orientation_constraints.append(oc)
        self.move_arm_cart_server.send_goal(g)
        rospy.loginfo('Sent move arm goal, waiting for result...')
        # TODO: this is always blocking
        self.move_arm_cart_server.wait_for_result()
        rv = self.move_arm_cart_server.get_result().error_code.val
        if not rv == 1:
            raise Exception('Failed to move the arm to the given pose.')

    def _move_to_pose(self, pose):
        self.script_server.move('arm', pose, mode=self.planning_mode,
                                blocking=self.blocking)

    def _construct_target_pose(self, xyzrpy):
        xyz = np.array(xyzrpy[:3] + [1])
        q = tf.transformations.quaternion_from_euler(*xyzrpy[3:])
        rotation = tf.transformations.quaternion_matrix(q)
        transformed = rotation.dot(self.ARM_LINK_5_TO_GRIPPER_OFFSET) + xyz
        position = Point(*transformed[0:3])
        orientation = Quaternion(*q)
        rospy.loginfo('Requested position: %s' % (str(xyz)))
        rospy.loginfo('Computed position: %s' % (str(position)))
        return position, orientation

    def gripper(self, state):
        self.script_server.move('gripper', state)
