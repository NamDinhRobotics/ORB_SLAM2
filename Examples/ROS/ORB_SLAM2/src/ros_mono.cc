/**
* This file is part of ORB-SLAM2.
*
* Copyright (C) 2014-2016 Raúl Mur-Artal <raulmur at unizar dot es> (University of Zaragoza)
* For more information see <https://github.com/raulmur/ORB_SLAM2>
*
* ORB-SLAM2 is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* ORB-SLAM2 is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with ORB-SLAM2. If not, see <http://www.gnu.org/licenses/>.
*/


#include<iostream>
#include<algorithm>
#include<fstream>
#include<chrono>

#include<ros/ros.h>
#include<tf/transform_broadcaster.h>        //thnabadee edited
#include<beginner_tutorials/odom_data.h>  //thnabadee edited

#include <cv_bridge/cv_bridge.h>

#include<opencv2/core/core.hpp>

#include"../../../include/System.h"

using namespace std;

   
ros::Publisher publisher_pose;
class ImageGrabber
{
public:
    ImageGrabber(ORB_SLAM2::System* pSLAM):mpSLAM(pSLAM){}

    void GrabImage(const sensor_msgs::ImageConstPtr& msg);

    ORB_SLAM2::System* mpSLAM;

     //thnabadee edited-------------------------
    
    int LOST_COUNT;
    beginner_tutorials::odom_data export_odom_data;
    // Transfor broadcaster (for visualization in rviz)
    tf::TransformBroadcaster mTfBr;
    //-----------------------------------------
};

int main(int argc, char **argv)
{
    ros::init(argc, argv, "Mono");
    ros::start();

    if(argc != 3)
    {
        cerr << endl << "Usage: rosrun ORB_SLAM2 Mono path_to_vocabulary path_to_settings" << endl;        
        ros::shutdown();
        return 1;
    }    

    // Create SLAM system. It initializes all system threads and gets ready to process frames.
    ORB_SLAM2::System SLAM(argv[1],argv[2],ORB_SLAM2::System::MONOCULAR,true);

    ImageGrabber igb(&SLAM);

    ros::NodeHandle nodeHandler;
    ros::Subscriber sub = nodeHandler.subscribe("/camera/image_raw", 1, &ImageGrabber::GrabImage,&igb);
    publisher_pose = nodeHandler.advertise<beginner_tutorials::odom_data>("slam", 10);//thanabadee edited
    ros::spin();

    // Stop all threads
    SLAM.Shutdown();

    // Save camera trajectory
    // SLAM.SaveKeyFrameTrajectoryTUM("KeyFrameTrajectory.txt");

    ros::shutdown();

    return 0;
}

void ImageGrabber::GrabImage(const sensor_msgs::ImageConstPtr& msg)
{
    // Copy the ros image message to cv::Mat.
    cv_bridge::CvImageConstPtr cv_ptr;
    try
    {
        cv_ptr = cv_bridge::toCvShare(msg);
    }
    catch (cv_bridge::Exception& e)
    {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }

    //thanabadee edited-------------------------------------------------------------------------------
    ros::Time currenttime=ros::Time::now();
    cv::Mat mTcw = mpSLAM->TrackMonocular(cv_ptr->image,cv_ptr->header.stamp.toSec());
    export_odom_data.odom_state = mpSLAM->GetState();
    if(export_odom_data.odom_state==3) {
        LOST_COUNT ++ ;
        if(LOST_COUNT>30) {
            mpSLAM->Reset();
            LOST_COUNT=0;
        }
    }else{
        LOST_COUNT=0;
    }
    
    if(!mTcw.empty())
    {    
        
        cv::Mat Rwc = mTcw.rowRange(0,3).colRange(0,3).t();
        cv::Mat twc = -Rwc*mTcw.rowRange(0,3).col(3);
        tf::Matrix3x3 M(Rwc.at<float>(0,0),Rwc.at<float>(0,1),Rwc.at<float>(0,2),
                        Rwc.at<float>(1,0),Rwc.at<float>(1,1),Rwc.at<float>(1,2),
                        Rwc.at<float>(2,0),Rwc.at<float>(2,1),Rwc.at<float>(2,2));
        tf::Vector3 V(twc.at<float>(0), twc.at<float>(1), twc.at<float>(2));

        tf::Transform tfTcw(M,V);

        tf::Quaternion Q = tfTcw.getRotation();
  //       ROS_INFO_STREAM("Cam" << " :\n\tquaternion:\n\t\tw: " << Q.getW() << "\n\t\tx: " << Q.getX() <<
  // "\n\t\ty: " << Q.getY() << "\n\t\tz: " << Q.getZ() << "\n\n"); 
  //       ROS_INFO_STREAM("Cam" << " :\n\tposition:\n\t\tx: " << V.x() <<
  // "\n\t\ty: " << V.y() << "\n\t\tz: " << V.z() << "\n\n"); 
        
        export_odom_data.pose.pose.position.x = V.x();
        export_odom_data.pose.pose.position.y = V.y();
        export_odom_data.pose.pose.position.z = V.z();
        export_odom_data.pose.pose.orientation.x = Q.getX();
        export_odom_data.pose.pose.orientation.y = Q.getY();
        export_odom_data.pose.pose.orientation.z = Q.getZ();
        export_odom_data.pose.pose.orientation.w = Q.getW();
        export_odom_data.track_count = mpSLAM->GetNumTrack();
        

        // tf::Transform vision_to_odom;
        // tf::Quaternion q_ov;
        // q_ov.setEuler(1.5707,0,-1.5707);
        // q_ov.setRPY(-1.5707,0,-1.5707);
        // vision_to_odom.setRotation(q_ov);

        static tf::TransformBroadcaster mTfBr,mTfBr2;
        
        mTfBr.sendTransform(tf::StampedTransform(tfTcw,currenttime, "vision", "Camera"));
        // mTfBr2.sendTransform(tf::StampedTransform(vision_to_odom,currenttime, "odom", "vision"));
    }
    
    export_odom_data.header.stamp = currenttime;
    export_odom_data.lost_count = LOST_COUNT;
    publisher_pose.publish(export_odom_data);//thanabadee edited
    //thanabadee edited-------------------------------------------------------------------------------
}


