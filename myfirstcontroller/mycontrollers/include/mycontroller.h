#ifndef MYCONTROLLER_H
#define MYCONTROLLER_H

/*!
 * \file MyController.h
 * \brief MyController is my super-nice controller
 * \author Marcus A Johansson
 * \version 0.1
 * \date 2016-06-21
 */

// STL Includes
#include <string>
#include <vector>

// Boost Includes
#include <boost/shared_ptr.hpp>

// ROS Messages


// ROS Services


// ROS Includes
#include <ros/ros.h>
#include <ros/node_handle.h>
#include <controller_interface/controller.h>
#include <hardware_interface/joint_command_interface.h>


/*!
 * \namespace mycontroller
 * 
 * namespace for mycontroller-related stuff
 */
namespace mycontroller
{

/*!
 * \brief A name for the standard ROS joint velocity controller
 */
typedef 
controller_interface::
Controller<hardware_interface::VelocityJointInterface> 
JointVelocityController;

/*!
 * \class MyController
 * \brief This is my awesome controller
 *
 *  It's awesome!
 */	
class MyController : public JointVelocityController
{
public:

	//typedef HwIfT hardware_interface::VelocityJointInterface;

	/*!
     * \brief Constructor
     * Constructs my awesome controller
     */
	MyController();

	/*!
     * \brief Destructor
     * Destructs my awesome controller :(
     */
	~MyController() noexcept;
	

	/*!
     * \brief Called every time the controller is initialized by the 
     *        ros::controller_manager
     *
     * Does some cool stuff!
     *
     * \param hw : a pointer to the hardware interface used by this controller
     * \param controller_nh : the node handle of this controller
     * \return true if the initialization was successful
     */
	bool init(hardware_interface::VelocityJointInterface *hw, 
			  ros::NodeHandle &controller_nh);

	/*!
     * \brief Called every time the controller is started by the 
     *        ros::controller_manager
     *
     * Does some cool stuff!
     *
     * \param time : the current wall-time in ROS
     * \return true if the starting was successful
     */
	void starting(const ros::Time& time);

	/*!
     * \brief Called every time the controller is updated by the 
     *        ros::controller_manager
     *
     * Does some cool stuff!
     *
     * \param time : the current wall-time in ROS
     * \param period : the time between the last update call and this, i.e.
     *                 the sample time.
     * \return true if the update was successful
     */
	void update(const ros::Time& time, const ros::Duration& period);

	/*!
     * \brief Called every time the controller is stopped by the 
     *        ros::controller_manager
     *
     * Does some cool stuff!
     *
     * \param time : the current wall-time in ROS
     * \return true if the stopping was successful
     */
	void stopping(const ros::Time& time);








private:

	// No copying of this class is allowed !
	MyController(const MyController& other) = delete;
	MyController(MyController&& other) = delete;
	MyController& operator=(const MyController& other) = delete;
	MyController& operator=(MyController&& other) noexcept = delete;

     ros::NodeHandle controller_nh_;
     std::vector< std::string > joint_names_;

};

}

#endif // include guard