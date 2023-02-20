#include "ros/ros.h"
#include "nav_msgs/Odometry.h"
#include "geometry_msgs/Pose.h"
#include "geometry_msgs/Twist.h"
#include "tf2/LinearMath/Quaternion.h"
#include "tf2_geometry_msgs/tf2_geometry_msgs.h"
#include "geometry_msgs/PoseWithCovarianceStamped.h"

using namespace ros;

geometry_msgs::Pose p_current;
geometry_msgs::Pose p_end;
geometry_msgs::Twist twist;
geometry_msgs::Pose poseAMCLx;

// Orientation saved as Quaternion
geometry_msgs::Quaternion poseAMCLo;
geometry_msgs::Quaternion q_current;
tf2::Quaternion q_end;

// yaw_end will be the end orientation of the rotation
// yaw_current will be the current orientation of the robot
// junk is for not needed references in the GetEulerYPR() function
double yaw_end, yaw_current, junk;

// Quaternion from tf2 namespace to set new orientation with rad (see "ros tf2" tutorial)
tf2::Quaternion q_tf;

/**
States:
0: Start up, set the first end position
1: Move to the first end position
2: Set the new end pose with position and orientation
3: Rotate to end orientation
4: Move to end position, 
5: rotate
*/
int state = 0;

Publisher p;

int round_num = 1;

void run(geometry_msgs::Pose pose, geometry_msgs::Quaternion quat)
{
    // Debug Message
    ROS_INFO("running state: %i", state);

    // State machine
    switch (state)
    {
        case 0:
        {
            p_end = pose;
            p_end.position.x += 1;

            state++;
            break;
        }

        case 1:
        {
            if (pose.position.x < p_end.position.x)
            {
                twist.linear.x = 0.3;
            }
            else
            {
                twist.linear.x = 0;
                state++;
            }
            break;
        }

        case 2:
        {   

            // Set new end position which is -1 x coordinate behind the robot
            p_end = pose;
            p_end.position.x -= 1;

            // Set yaw_end to 180°
            yaw_end = M_PI;

            // Set the Yaw angle to 180° in a quaternion from the tf2 namespace
            q_end.setRPY(0, 0, yaw_end);

            // Normalize quaternion so it has a magnitude of 1 to avoid warnings
            // (this is recommended by tf2 documentation)
            q_end.normalize();
            state++;
            break;
        }

        case 3:
        {  

            if(round_num == 2){
                ROS_INFO("case 3 ran");
            }
            // Get the current yaw from the q_current by transforming into a quaternion from tf2 namespace
            tf2::Quaternion tf2q_current(
                quat.x,
                quat.y,
                quat.z,
                quat.w);

            // Convert tf2 quaternion to rotation matrix
            tf2::Matrix3x3 matrix_current(tf2q_current);

            // Get the current yaw_current by reference

            // We don't need roll and pitch so we reference this info to the junk double
            matrix_current.getRPY(junk, junk, yaw_current);

            // Check if yaw_end and yaw_current are roughly the same
            // This will not rotate the robot by 180° exactly
            if (abs(yaw_end - yaw_current) > 0.05)
            {
                // Pub angular velocity so the robot starts rotating
                twist.angular.z = 0.5;
            }
            else
            {
                // Stop robot if yaw_end and yaw_current are roughly the same
                twist.angular.z = 0;
                state++;
            }
            break;
        }

        case 4:
        {
            // Move to new end position
            if (pose.position.x > p_end.position.x)
            {
                twist.linear.x = 0.3;
            }
            else
            {
                twist.linear.x = 0;
                state++;
                // Set new end position
                p_end = pose;
                // Set yaw_end to 180°
                yaw_end = 0;

                // Set the Yaw angle to 180° in a quaternion from the tf2 namespace
                q_end.setRPY(0, 0, yaw_end);

                // Normalize quaternion so it has a magnitude of 1 to avoid warnings
                // (this is recommended by tf2 documentation)
                q_end.normalize();
            }
            break;
        }


        case 5:
        {
            // Get the current yaw from the q_current by transforming into a quaternion from tf2 namespace
            tf2::Quaternion tf2q_current(
                quat.x,
                quat.y,
                quat.z,
                quat.w);

            // Convert tf2 quaternion to rotation matrix
            tf2::Matrix3x3 matrix_current(tf2q_current);

            // Get the current yaw_current by reference
            // We don't need roll and pitch so we reference this info to the junk double
            matrix_current.getRPY(junk, junk, yaw_current);

            // Check if yaw_end and yaw_current are roughly the same
            // This will not rotate the robot by 180° exactly
            if (abs(yaw_end - yaw_current) > 0.05)
            {
                // Pub angular velocity so the robot starts rotating
                twist.angular.z = 0.5;
            }
            else
            {
                // Stop robot if yaw_end and yaw_current are roughly the same
                twist.angular.z = 0;
                state=0;
                round_num++;
            }
            break;
        }
    }

    if(round_num == 2 && state == 3){

                ROS_INFO("case 3 ran");

    p.publish(twist);
}

void callback(const nav_msgs::Odometry msg)
{
    p_current = msg.pose.pose;
    q_current = msg.pose.pose.orientation;
    if(round_num % 2 == 1){
        run(p_current,q_current);
    }
}



void poseAMCLCallback(const geometry_msgs::PoseWithCovarianceStamped::ConstPtr& msgAMCL)
{
    poseAMCLx = msgAMCL->pose.pose;
    poseAMCLo = msgAMCL->pose.pose.orientation;   
    if(round_num % 2 == 0){
        ROS_INFO("amcl running");
        run(poseAMCLx,poseAMCLo);
    }
}

int main(int argc, char **argv)
{
    ROS_INFO("started");
    init(argc, argv, "solution_node3");
    NodeHandle n;
    Rate loop_rate(5);

    p = n.advertise<geometry_msgs::Twist>("/mobile_base_controller/cmd_vel", 1000);
    
    Subscriber s = n.subscribe("/mobile_base_controller/odom", 1, callback);
   
    Subscriber sub_amcl = n.subscribe("/amcl_pose", 1, poseAMCLCallback);

    spin();

    return 0;
}
