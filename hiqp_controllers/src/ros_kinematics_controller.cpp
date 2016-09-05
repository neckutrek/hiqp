// The HiQP Control Framework, an optimal control framework targeted at robotics
// Copyright (C) 2016 Marcus A Johansson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.




/*!
 * \file   ros_kinematics_controller.cpp
 * \Author Marcus A Johansson (marcus.adam.johansson@gmail.com)
 * \date   July, 2016
 * \brief  Brief description of file.
 *
 * Detailed description of file.
 */



#include <hiqp/ros_kinematics_controller.h>
#include <hiqp/hiqp_utils.h>

#include <hiqp_msgs_srvs/PerfMeasMsg.h>
#include <hiqp_msgs_srvs/MonitorDataMsg.h>

#include <pluginlib/class_list_macros.h> // to allow the controller to be loaded as a plugin

#include <XmlRpcValue.h>  
#include <XmlRpcException.h> 

#include <iostream>

#include <geometry_msgs/PoseStamped.h> // teleoperation magnet sensors




namespace hiqp
{
	









ROSKinematicsController::ROSKinematicsController()
: is_active_(true), monitoring_active_(false), task_manager_(&ros_visualizer_)
{
}










ROSKinematicsController::~ROSKinematicsController() noexcept
{
}










bool ROSKinematicsController::init
(
	hardware_interface::VelocityJointInterface *hw, 
	ros::NodeHandle &controller_nh
)
{
	// Store the handle of the node that runs this controller
	controller_nh_ = controller_nh;
	ros_visualizer_.init(&controller_nh_);



	// Load the names of all joints specified in the .yaml file
	std::string param_name = "joints";
	std::vector< std::string > joint_names;
	if (!controller_nh.getParam(param_name, joint_names))
    {
        ROS_ERROR_STREAM("In ROSKinematicsController: Call to getParam('" 
        	<< param_name 
        	<< "') in namespace '" 
        	<< controller_nh.getNamespace() 
        	<< "' failed.");
        return false;
    }





    // Load the monitoring setup specified in the .yaml file
	XmlRpc::XmlRpcValue task_monitoring;
	if (!controller_nh.getParam("task_monitoring", task_monitoring))
    {
        ROS_ERROR_STREAM("In ROSKinematicsController: Call to getParam('" 
        	<< "task_monitoring" 
        	<< "') in namespace '" 
        	<< controller_nh.getNamespace() 
        	<< "' failed.");
        return false;
    }
    int active = static_cast<int>(task_monitoring["active"]);
    monitoring_active_ = (active == 1 ? true : false);
    monitoring_publish_rate_ = 
    	static_cast<double>(task_monitoring["publish_rate"]);
    monitoring_pub_ = controller_nh_.advertise<hiqp_msgs_srvs::MonitorDataMsg>
		("monitoring_data", 1);





    // Load the urdf-formatted robot description to build a KDL tree
	std::string full_parameter_path;
    std::string robot_urdf;
    if (controller_nh_.searchParam("robot_description", full_parameter_path))
    {
        controller_nh_.getParam(full_parameter_path, robot_urdf);
        ROS_ASSERT(kdl_parser::treeFromString(robot_urdf, kdl_tree_));
    }
    else
    {
        ROS_ERROR_STREAM("In ROSKinematicsController: Could not find"
        	<< " parameter 'robot_description' on the parameter server.");
        return false;
    }
    std::cout << "KDL Tree loaded successfully! Printing it below.\n\n"
    	<< kdl_tree_ << "\n\n";




    // Load all joint handles for all joint name references
	for (auto&& name : joint_names)
	{
		try
		{
			unsigned int q_nr = kdl_getQNrFromJointName(kdl_tree_, name);
			joint_handles_map_.insert( 
				JointHandleMapEntry(q_nr, hw->getHandle(name))
			);
		}
		catch (const hardware_interface::HardwareInterfaceException& e)
		{
			ROS_ERROR_STREAM("Exception thrown: " << e.what());
            return false;
		}
		// catch (MAP INSERT FAIL EXCEPTION)
		// catch (HIQP Q_NR NOT AVAILABLE EXCEPTION)
	}




	// Set the joint position and velocity and the control vectors to all zero
	unsigned int n_joint_names = joint_names.size();
	unsigned int n_kdl_joints = kdl_tree_.getNrOfJoints();
	if (n_joint_names > n_kdl_joints)
	{
		ROS_ERROR_STREAM("In ROSKinematicsController: The .yaml file"
			<< " includes more joint names than specified in the .urdf file."
			<< " Could not succeffully initialize controller. Aborting!\n");
		return false;
	}
	kdl_joint_pos_vel_.resize(n_kdl_joints);
	output_controls_ = std::vector<double>(n_kdl_joints, 0.0);






	// Sample the first joint positions and velocities
	KDL::JntArray& q = kdl_joint_pos_vel_.q;
	KDL::JntArray& qdot = kdl_joint_pos_vel_.qdot;
	handles_mutex_.lock();
		sampling_time_ = std::chrono::steady_clock::now();
		for (auto&& handle : joint_handles_map_)
		{
			q(handle.first) = handle.second.getPosition();
			qdot(handle.first) = handle.second.getVelocity();
		}
	handles_mutex_.unlock();






	// Setup topic subscription
	topic_subscriber_.init( task_manager_.getGeometricPrimitiveMap() );
	
	topic_subscriber_.addSubscription<geometry_msgs::PoseStamped>(
		controller_nh_, "/wintracker/pose", 100
	);






	// Advertise available ROS services and link the callback functions
	add_task_service_ = controller_nh_.advertiseService
	(
		"add_task",
		&ROSKinematicsController::addTask,
		this
	);

	remove_task_service_ = controller_nh_.advertiseService
	(
		"remove_task",
		&ROSKinematicsController::removeTask,
		this
	);

	remove_all_tasks_service_ = controller_nh_.advertiseService
	(
		"remove_all_tasks",
		&ROSKinematicsController::removeAllTasks,
		this
	);

	add_geomprim_service_ = controller_nh_.advertiseService
	(
		"add_primitive",
		&ROSKinematicsController::addGeometricPrimitive,
		this
	);

	remove_geomprim_service_ = controller_nh_.advertiseService
	(
		"remove_primitive",
		&ROSKinematicsController::removeGeometricPrimitive,
		this
	);

	remove_all_geomprims_service_ = controller_nh_.advertiseService
	(
		"remove_all_primitives",
		&ROSKinematicsController::removeAllGeometricPrimitives,
		this
	);




	task_manager_.init(n_kdl_joints);







	// Preload the:
	// - joint limitations
	// - geometric primitives
	// - tasks
	// in among the preload parameters in the .yaml file, if there are any
	loadJointLimitsFromParamServer();
	loadGeometricPrimitivesFromParamServer();
	loadTasksFromParamServer();




	return true;
}










void ROSKinematicsController::starting
(
	const ros::Time& time
)
{}










void ROSKinematicsController::update
(
	const ros::Time& time, 
	const ros::Duration& period
)
{
	// If the controller is inactive just return
	if (!is_active_) return;





	// Lock the mutex and read all joint positions from the handles
	KDL::JntArray& q = kdl_joint_pos_vel_.q;
	KDL::JntArray& qdot = kdl_joint_pos_vel_.qdot;
	handles_mutex_.lock();
		sampling_time_ = std::chrono::steady_clock::now();
		for (auto&& handle : joint_handles_map_)
		{
			q(handle.first) = handle.second.getPosition();
			qdot(handle.first) = handle.second.getVelocity();
		}
	handles_mutex_.unlock();


	// Calculate the kinematic controls
	task_manager_.getKinematicControls(sampling_time_,
									   kdl_tree_, 
									   kdl_joint_pos_vel_,
									   output_controls_);



	// Lock the mutex and write the controls to the joint handles
	handles_mutex_.lock();
		for (auto&& handle : joint_handles_map_)
		{
			handle.second.setCommand(output_controls_.at(handle.first));
		}
	handles_mutex_.unlock();



	// Redraw all geometric primitives
	task_manager_.getGeometricPrimitiveMap()->redrawAllPrimitives();



	// If monitoring is turned on, generate monitoring data and publish it
	if (monitoring_active_)
	{
		ros::Time now = ros::Time::now();
		ros::Duration d = now - last_monitoring_update_;
		if (d.toSec() >= 1.0/monitoring_publish_rate_)
		{
			last_monitoring_update_ = now;
			std::vector<TaskMonitoringData> data;
			task_manager_.getTaskMonitoringData(data);

			hiqp_msgs_srvs::MonitorDataMsg mon_msg;
			mon_msg.ts = now;

			std::vector<TaskMonitoringData>::iterator it = data.begin();
			while (it != data.end())
			{
				hiqp_msgs_srvs::PerfMeasMsg per_msg;
				
				per_msg.task_id = it->task_id_;
				per_msg.task_name = it->task_name_;
				per_msg.measure_tag = it->measure_tag_;
				per_msg.data.insert(per_msg.data.begin(),
					                it->performance_measures_.cbegin(),
					                it->performance_measures_.cend());

				mon_msg.data.push_back(per_msg);
				++it;
			}
			
			monitoring_pub_.publish(mon_msg);

/*
			std::cout << "Monitoring performance:\n";
			std::vector<TaskMonitoringData>::iterator it = data.begin();
			while (it != data.end())
			{
				std::size_t t = it->task_id_;

				std::cout << it->task_id_ << "   "
				          << it->task_name_ << "   ";
				std::vector<double>::const_iterator it2 = it->performance_measures_.begin();
				while (it2 != it->performance_measures_.end())
				{
					std::cout << *it2 << ",";
					it2++;
				}
				std::cout << std::endl;
				it++;
			}
			*/
		}

	}



	return;
}










void ROSKinematicsController::stopping
(
	const ros::Time& time
)
{}







bool ROSKinematicsController::addTask
(
	hiqp_msgs_srvs::AddTask::Request& req, 
    hiqp_msgs_srvs::AddTask::Response& res
)
{
	int retval = task_manager_.addTask(req.name, req.type, req.behaviour,
									   req.priority, req.visibility, 
									   req.parameters,
									   sampling_time_,
									   kdl_tree_,
									   kdl_joint_pos_vel_);



	res.success = (retval < 0 ? false : true);

	if (res.success)
	{
		res.task_id = retval;
		printHiqpInfo("Added task of type '" + req.type + "'"
			+ " with priority " + std::to_string(req.priority)
			+ " and identifier " + std::to_string(res.task_id));
	}

	return true;
}





bool ROSKinematicsController::removeTask
(
	hiqp_msgs_srvs::RemoveTask::Request& req, 
    hiqp_msgs_srvs::RemoveTask::Response& res
)
{
	res.success = false;
	if (task_manager_.removeTask(req.task_id) == 0)
		res.success = true;

	if (res.success)
	{
		printHiqpInfo("Removed task '" + std::to_string(req.task_id) + "' successfully!");
	}
	else
	{
		printHiqpInfo("Couldn't remove task '" + std::to_string(req.task_id) + "'!");	
	}

	return true;
}





bool ROSKinematicsController::removeAllTasks
(
    hiqp_msgs_srvs::RemoveAllTasks::Request& req, 
    hiqp_msgs_srvs::RemoveAllTasks::Response& res
)
{
	task_manager_.removeAllTasks();
	printHiqpInfo("Removed all tasks successfully!");
	res.success = true;
	return true;
}





bool ROSKinematicsController::addGeometricPrimitive
(
    hiqp_msgs_srvs::AddGeometricPrimitive::Request& req, 
    hiqp_msgs_srvs::AddGeometricPrimitive::Response& res
)
{
	int retval = task_manager_.addGeometricPrimitive(
		req.name, req.type, req.frame_id, req.visible, req.color, req.parameters
	);

	res.success = (retval == 0 ? true : false);

	if (res.success)
	{
		printHiqpInfo("Added geometric primitive of type '" 
			+ req.type + "' with name '" + req.name + "'.");
	}

	return true;
}





bool ROSKinematicsController::removeGeometricPrimitive
(
    hiqp_msgs_srvs::RemoveGeometricPrimitive::Request& req, 
    hiqp_msgs_srvs::RemoveGeometricPrimitive::Response& res
)
{
	res.success = false;
	if (task_manager_.removeGeometricPrimitive(req.name) == 0)
		res.success = true;

	if (res.success)
	{
		printHiqpInfo("Removed primitive '" + req.name + "' successfully!");
	}
	else
	{
		printHiqpInfo("Couldn't remove primitive '" + req.name + "'!");	
	}

	return true;
}





bool ROSKinematicsController::removeAllGeometricPrimitives
(
    hiqp_msgs_srvs::RemoveAllGeometricPrimitives::Request& req, 
    hiqp_msgs_srvs::RemoveAllGeometricPrimitives::Response& res
)
{
	task_manager_.removeAllGeometricPrimitives();
	printHiqpInfo("Removed all primitives successfully!");
	res.success = true;
	return true;
}



























/*** PRIVATE ***/


void ROSKinematicsController::loadJointLimitsFromParamServer()
{
	XmlRpc::XmlRpcValue hiqp_preload_jnt_limits;
	if (!controller_nh_.getParam("hiqp_preload_jnt_limits", hiqp_preload_jnt_limits))
    {
    	ROS_WARN_STREAM("No hiqp_preload_jnt_limits parameter found on "
    		<< "the parameter server. No joint limits were loaded!");
	}
	else
	{
	    for (int i=0; i<hiqp_preload_jnt_limits.size(); ++i)
	    {
	    	std::string link_frame = static_cast<std::string>(
	    		hiqp_preload_jnt_limits[i]["link_frame"] );

	    	XmlRpc::XmlRpcValue& limitations = 
	    		hiqp_preload_jnt_limits[i]["limitations"];

	    	std::vector<std::string> parameters;
	    	parameters.push_back(link_frame);
	    	parameters.push_back( std::to_string(
	    		static_cast<double>(limitations[0]) ) );
	    	parameters.push_back( std::to_string(
	    		static_cast<double>(limitations[1]) ) );
	    	parameters.push_back( std::to_string(
	    		static_cast<double>(limitations[2]) ) );

			// int addTask
			//     (
			//        const std::string& name,
			//        const std::string& type,
			//        const std::vector<std::string>& behaviour_parameters,
			//        unsigned int priority,
			//        bool visibility,
			//        const std::vector<std::string>& parameters,
			//        const std::chrono::steady_clock::time_point& sampling_time,
			//        const KDL::Tree& kdl_tree,
			//        const KDL::JntArrayVel& kdl_joint_pos_vel
			//    );

	    	task_manager_.addTask(
	    		link_frame + "_jntlimits",
	    		"TaskJntLimits",
	    		std::vector<std::string>(),
	    		1,
	    		false,
	    		parameters,
	    		sampling_time_,
	    		kdl_tree_,
	    		kdl_joint_pos_vel_
	    	);
	    }

	    ROS_INFO("Loaded and initiated joint limit tasks successfully!");
	}
}







void ROSKinematicsController::loadGeometricPrimitivesFromParamServer()
{
	XmlRpc::XmlRpcValue hiqp_preload_geometric_primitives;
	if (!controller_nh_.getParam(
		"hiqp_preload_geometric_primitives", 
		hiqp_preload_geometric_primitives)
		)
    {
    	ROS_WARN_STREAM("No hiqp_preload_geometric_primitives parameter "
    		<< "found on the parameter server. No geometric primitives "
    		<< "were loaded!");
	}
	else
	{
		bool parsing_success = true;
		for (int i=0; i<hiqp_preload_geometric_primitives.size(); ++i)
	    {
	    	try {
		    	std::string name = static_cast<std::string>(
		    		hiqp_preload_geometric_primitives[i]["name"] );

		    	std::string type = static_cast<std::string>(
		    		hiqp_preload_geometric_primitives[i]["type"] );

		    	std::string frame_id = static_cast<std::string>(
		    		hiqp_preload_geometric_primitives[i]["frame_id"] );

		    	bool visible = static_cast<bool>(
		    		hiqp_preload_geometric_primitives[i]["visible"] );

		    	XmlRpc::XmlRpcValue& color_xml = 
		    		hiqp_preload_geometric_primitives[i]["color"];

		    	XmlRpc::XmlRpcValue& parameters_xml = 
		    		hiqp_preload_geometric_primitives[i]["parameters"];

		    	std::vector<double> color;
		    	color.push_back( static_cast<double>(color_xml[0]) );
		    	color.push_back( static_cast<double>(color_xml[1]) );
		    	color.push_back( static_cast<double>(color_xml[2]) );
		    	color.push_back( static_cast<double>(color_xml[3]) );

		    	std::vector<double> parameters;
		    	for (int j=0; j<parameters_xml.size(); ++j)
		    	{
		    		parameters.push_back( 
		    			static_cast<double>(parameters_xml[j]) 
		    		);
		    	}

		    	// int addGeometricPrimitive
			    // (
			    //     const std::string& name,
			    //     const std::string& type,
			    //     const std::string& frame_id,
			    //     bool visible,
			    //     const std::vector<double>& color,
			    //     const std::vector<double>& parameters
			    // );

		    	task_manager_.addGeometricPrimitive(
		    		name, type, frame_id, visible, color, parameters
		    	);

		    }
		    catch (const XmlRpc::XmlRpcException& e)
		    {
		    	ROS_WARN_STREAM("Error while loading "
		    		<< "hiqp_preload_geometric_primitives parameter from the "
		    		<< "parameter server. XmlRcpException thrown with message: "
		    		<< e.getMessage());
		    	parsing_success = false;
		    	break;
		    }
	    }

	    if (parsing_success)
	    	ROS_INFO("Loaded and initiated geometric primitives successfully!");
	}
}








void ROSKinematicsController::loadTasksFromParamServer()
{

}











} // namespace hiqp


// make the controller available to the library loader
PLUGINLIB_EXPORT_CLASS(hiqp::ROSKinematicsController, controller_interface::ControllerBase)
