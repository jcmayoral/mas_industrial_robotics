/*
 * dynamixel_gripper_node.cpp
 *
 *  Created on: Apr 14, 2015
 *      Author: fred
 */

#include <mir_gripper_controller/dynamixel_gripper_node.h>

DynamixelGripperNode::DynamixelGripperNode(ros::NodeHandle &nh) :
        action_server_(nh, "gripper_controller", false), joint_states_received_(false), torque_limit_(0.5)
{
    pub_dynamixel_command_ = nh_.advertise < std_msgs::Float64 > ("dynamixel_command", 1);
    sub_dynamixel_motor_states_ = nh_.subscribe("dynamixel_motor_states", 10, &DynamixelGripperNode::jointStatesCallback, this);

    pub_joint_states_ = nh_.advertise < sensor_msgs::JointState > ("joint_state", 10);

    // ToDo: as parameters
    torque_limit_ = 0.5;

    action_server_.registerGoalCallback(boost::bind(&DynamixelGripperNode::gripperCommandGoalCallback, this));
    action_server_.start();
}

DynamixelGripperNode::~DynamixelGripperNode()
{
    pub_dynamixel_command_.shutdown();
    pub_joint_states_.shutdown();
    sub_dynamixel_motor_states_.shutdown();
}

void DynamixelGripperNode::jointStatesCallback(const dynamixel_msgs::JointState::Ptr &msg)
{
    joint_states_ = msg;
    joint_states_received_ = true;

    // publish dynamixel joint states as sensor_msgs joint states
    sensor_msgs::JointState joint_state;
    joint_state.header.stamp = msg->header.stamp;

    joint_state.name.push_back("gripper_finger_joint_r");
    joint_state.position.push_back(mapFromRadiansToMeter(msg->current_pos)); 
    joint_state.velocity.push_back(msg->velocity);
    joint_state.effort.push_back(msg->load);

    joint_state.name.push_back("gripper_finger_joint_l");
    joint_state.position.push_back(mapFromRadiansToMeter(msg->current_pos)); 
    joint_state.velocity.push_back(msg->velocity);
    joint_state.effort.push_back(msg->load);

    pub_joint_states_.publish(joint_state);
}

double DynamixelGripperNode::mapFromRadiansToMeter(const double &radians)
{
    return (radians * 3.33);    // ToDo: as a parameter
}

void DynamixelGripperNode::gripperCommandGoalCallback()
{
    ros::Rate loop_rate(100);

    // accept and get goal position
    double set_pos = action_server_.acceptNewGoal()->command.position;

    // publish goal position
    std_msgs::Float64 gripper_pos;   
    gripper_pos.data = set_pos;
    pub_dynamixel_command_.publish(gripper_pos);
    
    // wait until position is or max. torque read
    while(ros::ok())
    {
        joint_states_received_ = false;       
        while(!joint_states_received_ && ros::ok())
        {
            ros::spinOnce();
            loop_rate.sleep();
        }

        if(joint_states_->load >= torque_limit_)
        {
            gripper_pos.data = joint_states_->current_pos;
            pub_dynamixel_command_.publish(gripper_pos);
            ROS_WARN_STREAM("Torque soft limit reached (cur: " << joint_states_->load <<  ", limit: " << torque_limit_ <<  ")  " << set_pos << " reached");

            break;
        }

        ROS_DEBUG_STREAM("Positions diff: " << (joint_states_->current_pos - set_pos));
        
        if(fabs(joint_states_->current_pos - set_pos) < 0.05)
        {
            ROS_INFO_STREAM("Position " << set_pos << " reached");
            break;
        }
    }

    joint_states_received_ = false;       
    while(!joint_states_received_ && ros::ok())
    {
        ros::spinOnce();
        loop_rate.sleep();
    }

    gripper_result_.position = joint_states_->current_pos;
    gripper_result_.effort = joint_states_->load;
    gripper_result_.stalled = false;
    gripper_result_.reached_goal = true;

    action_server_.setSucceeded(gripper_result_);
}

int main(int argc, char **argv)
{
    ros::init(argc, argv, "dynamixel_gripper");
    ros::NodeHandle nh;

    DynamixelGripperNode gripper(nh);

    ros::spin();

    return 0;
}