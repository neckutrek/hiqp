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
 * \file   task_factory.h
 * \Author Marcus A Johansson (marcus.adam.johansson@gmail.com)
 * \date   July, 2016
 * \brief  Brief description of file.
 *
 * Detailed description of file.
 */




#ifndef HIQP_TASK_FACTORY_H
#define HIQP_TASK_FACTORY_H

// HiQP Includes
#include <hiqp/task_function.h>
#include <hiqp/task_dynamics.h>
#include <hiqp/visualizer.h>
#include <hiqp/geometric_primitive_map.h>

#include <hiqp/geometric_primitives/geometric_point.h>
#include <hiqp/geometric_primitives/geometric_line.h>
#include <hiqp/geometric_primitives/geometric_plane.h>
#include <hiqp/geometric_primitives/geometric_box.h>
#include <hiqp/geometric_primitives/geometric_cylinder.h>
#include <hiqp/geometric_primitives/geometric_sphere.h>

// STL Includes
#include <string>
#include <vector>

// Orocos KDL
#include <kdl/tree.hpp>
#include <kdl/jntarrayvel.hpp>









namespace hiqp {





class TaskFactory
{

public:



	/*!
     * \brief Constructor
     * Constructs my awesome controller
     */
    TaskFactory() {}

	void init
	(
		GeometricPrimitiveMap* geometric_primitive_map,
		Visualizer* visualizer,
		unsigned int num_controls
	);




	/*!
     * \brief Destructor
     * Destructs my awesome manager
     */
	~TaskFactory() noexcept {}




	/*!
	 * \brief
	 *
	 */
	TaskFunction* buildTaskFunction
	(
		const std::string& name,
		std::size_t id,
    	const std::string& type,
    	unsigned int priority,
    	bool visibility,
    	const std::vector<std::string>& parameters,
    	TaskDynamics* dynamics
	);




	/*!
	 * \brief
	 *
	 */
	TaskDynamics* buildTaskDynamics
	(
		const std::vector<std::string>& parameters
	);








private:

	// No copying of this class is allowed !
	TaskFactory(const TaskFactory& other) = delete;
	TaskFactory(TaskFactory&& other) = delete;
	TaskFactory& operator=(const TaskFactory& other) = delete;
	TaskFactory& operator=(TaskFactory&& other) noexcept = delete;




	GeometricPrimitiveMap* 				geometric_primitive_map_;

	Visualizer* 						visualizer_;

	unsigned int 						num_controls_;

};







} // namespace hiqp

#endif // include guard