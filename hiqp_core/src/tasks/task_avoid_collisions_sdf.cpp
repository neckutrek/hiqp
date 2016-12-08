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

#include <hiqp/tasks/task_avoid_collisions_sdf.h>
#include <hiqp/utilities.h>
#include <hiqp_collision_check/sdf_collision_checker.h>
#include <iostream>

namespace hiqp
{
namespace tasks
{
//==================================================================================

int TaskAvoidCollisionsSDF::init(const std::vector<std::string>& parameters,
				   RobotStatePtr robot_state) {
//   std::cout<<"printing parameters:"<<std::endl;
//   for (int i=0; i<parameters.size();i++)
//     {
//       std::cout<<parameters.at(i)<<std::endl;
// }
  int size = parameters.size();
  if (size < 2) {
    printHiqpWarning("TaskAvoidCollisionsSDF requires at least 2 parameters, got " 
		     + std::to_string(size) + "! Initialization failed!");
    return -2;
  }

  reset();

//loop through all the geometric primitives intended for the obstacle avoidance and extract the pointers
std::shared_ptr<GeometricPrimitiveMap> gpm = this->getGeometricPrimitiveMap();
 for (unsigned int i=1; i<size; i++)
   {
     //make sure the primitive is either a point or a sphere - avoidance with other primitives is not currently implemented
     if (gpm->getGeometricPrimitive<GeometricPoint>(parameters.at(i)) != nullptr)
       {
	 std::shared_ptr<GeometricPoint> point=gpm->getGeometricPrimitive<GeometricPoint>(parameters.at(i));
	 //make sure the avoidance point is attached to the manipulator
	 if(kdl_getQNrFromLinkName(robot_state->kdl_tree_, point->getFrameId()) == -1)
	   {
	     printHiqpWarning("TaskAvoidCollisionsSDF::init, avoidance point '" + parameters.at(i) + "' is not attached to the manipulator! Initialization failed.");
	     return -2; 
	   }
	 primitives_.push_back(point);
	 n_dimensions_++;
       }
 else if (gpm->getGeometricPrimitive<GeometricSphere>(parameters.at(i)) != nullptr)
   {
     std::shared_ptr<GeometricSphere> sphere = gpm->getGeometricPrimitive<GeometricSphere>(parameters.at(i));
     //make sure the avoidance sphere is attached to the manipulator
     if(kdl_getQNrFromLinkName(robot_state->kdl_tree_, sphere->getFrameId()) == -1)
       {
	 printHiqpWarning("TaskAvoidCollisionsSDF::init, avoidance sphere '" + parameters.at(i) + "' is not attached to the manipulator! Initialization failed.");
	 return -2; 
       }
     primitives_.push_back(sphere);
     n_dimensions_++;
   }
 else
   { 
     printHiqpWarning("TaskAvoidCollisionsSDF::init, couldn't find primitive '" + parameters.at(i) + "'! Initialization failed.");
     return -2; 
   }
  }

 performance_measures_.resize(0);
 task_types_.insert(task_types_.begin(), n_dimensions_, -1); // -1 leq, 0 eq, 1 geq

 fk_solver_pos_ = std::make_shared<KDL::TreeFkSolverPos_recursive>(robot_state->kdl_tree_);
 fk_solver_jac_ = std::make_shared<KDL::TreeJntToJacSolver>(robot_state->kdl_tree_);

 root_frame_id_=robot_state->kdl_tree_.getRootSegment()->second.segment.getName();

 printHiqpInfo("Initializing collision checker");
 collision_checker_ = std::make_shared<hiqp::SDFCollisionCheck> ();
 collision_checker_->init();
 return 0;
}
//==================================================================================
int TaskAvoidCollisionsSDF::update(RobotStatePtr robot_state) {


 // e_.resize(n_dimensions_);
 // e_.setZero();
 // J_.resize(n_dimensions_, n_controls);
 // J_.setZero();

  for(unsigned int i=0; i<primitives_.size();i++)
    {
      // compute forward kinematics for each primitive (yet unimplemented primitives such as capsules could have more than one ee_/J associated with them, hence the vector-valued argument
      std::vector<KinematicQuantities> kin_q_list;
      if(primitiveForwardKinematics(kin_q_list, primitives_[i], robot_state) < 0)
	{
	  printHiqpWarning("TaskAvoidCollisionsSDF::update, failed.");
	  return -2;
	}

      //get the gradient vectors associated with the current primitive from the SDF map
      typedef std::vector<Eigen::Vector3d,Eigen::aligned_allocator<Eigen::Vector3d> > SamplesVector;
      SamplesVector test_pts;
      for (unsigned int j=0; j<kin_q_list.size(); j++)
	{
	  Eigen::Vector3d p(kin_q_list[j].ee_pose_.p.x(),kin_q_list[j].ee_pose_.p.y(),kin_q_list[j].ee_pose_.p.z());
	  test_pts.push_back(p);
	}

      SamplesVector gradients;
      if(!collision_checker_->obstacleGradientBulk(test_pts, gradients,root_frame_id_))
	{
	  printHiqpWarning("TaskAvoidCollisionsSDF::update, collision checker failed.");
	  //DEBUG HACK, JUST FOR TESTING IN ABSENCE OF A MAP!!!!!!!!!!!!!!
	  //	  return -2;
          gradients.clear();
          gradients.push_back(Eigen::Vector3d(0.6, -0.3, 0.3));
	  //DEBUG HACK, END!!!!!!!!!!!!!!
	}

      //compute the task jacobian for the current geometric primitive
      //int appendTaskJacobian(const std::vector<KinematicQuantities> kin_q_list ,const SamplesVector& gradients )
      //int appendTaskFunction(const std::string& primitive_type, const std::vector<KinematicQuantities> kin_q_list, const SamplesVector& gradients)

      //to check if a gradient to an obstacle is valid
      //  virtual bool isValid (const Eigen::Vector3d &grad) = 0; 

    }

 return 0;
}
  //==================================================================================
  int TaskAvoidCollisionsSDF::monitor() {
    return 0;
  }
  //==================================================================================
  void TaskAvoidCollisionsSDF::reset() {
    n_dimensions_=0;
    task_types_.clear();
    primitives_.clear();
  }
  //==================================================================================
  int TaskAvoidCollisionsSDF::forwardKinematics(KinematicQuantities& kin_q, const KDL::JntArray& q)const
  {
    if(fk_solver_pos_->JntToCart(q, kin_q.ee_pose_, kin_q.frame_id_) < 0)
      {
	printHiqpWarning("TaskAvoidCollisionsSDF::forwardKinematics, end-effector FK for link '" + kin_q.frame_id_ + "' failed.");
	return -2;
      }
    //std::cout<<"ee_pose: "<<std::endl<<kin_q.ee_pose_<<std::endl;
    if(fk_solver_jac_->JntToJac(q, kin_q.ee_J_, kin_q.frame_id_) < 0)
      {
	printHiqpWarning("TaskAvoidCollisionsSDF::forwardKinematics, Jacobian computation for link '" + kin_q.frame_id_ + "' failed.");
	return -2;
      }
    // std::cout<<"ee_J: "<<std::endl<<kin_q.ee_J_<<std::endl;


    return 0;
  }
  //==================================================================================
  int TaskAvoidCollisionsSDF::primitiveForwardKinematics(std::vector<KinematicQuantities>& kin_q_list, const std::shared_ptr<geometric_primitives::GeometricPrimitive>& primitive, RobotStatePtr robot_state )const
  {

    kin_q_list.clear();
    std::string type=primitive->getType();

    if(strcmp(type.c_str(),"point")==0)
      {
	std::shared_ptr<GeometricPoint> point(std::static_pointer_cast<GeometricPoint>(primitive)); // downcast the primitive to the actual derived type
	KDL::Vector coord=point->getPointKDL();
	KinematicQuantities kin_q;
	kin_q.ee_J_.resize(robot_state->kdl_jnt_array_vel_.q.rows());
	kin_q.frame_id_=point->getFrameId();
	if(forwardKinematics(kin_q,robot_state->kdl_jnt_array_vel_.q) < 0)
	  {
	    printHiqpWarning("TaskAvoidCollisionsSDF::primitiveForwardKinematics, primitive forward kinematics for GeometricPoint primitive '" + point->getName() + "' failed.");
	    return -2;
	  }  
	//shift the Jacobian reference point
	kin_q.ee_J_.changeRefPoint(kin_q.ee_pose_.M*coord);

	kin_q_list.push_back(kin_q);
      }
    else if(strcmp(type.c_str(),"sphere")==0)
      {
	std::shared_ptr<GeometricSphere> sphere(std::static_pointer_cast<GeometricSphere>(primitive)); // downcast the primitive to the actual derived type
	KDL::Vector coord(sphere->getX(), sphere->getY(), sphere->getZ());
	KinematicQuantities kin_q;
	kin_q.ee_J_.resize(robot_state->kdl_jnt_array_vel_.q.rows());
	kin_q.frame_id_=sphere->getFrameId();
	if(forwardKinematics(kin_q,robot_state->kdl_jnt_array_vel_.q) < 0)
	  {
	    printHiqpWarning("TaskAvoidCollisionsSDF::primitiveForwardKinematics, primitive forward kinematics for GeometricSphere primitive '" + sphere->getName() + "' failed.");
	    return -2;
	  }   
	//shift the Jacobian reference point
	kin_q.ee_J_.changeRefPoint(kin_q.ee_pose_.M*coord);

	kin_q_list.push_back(kin_q);
      }
    else{
      printHiqpWarning("TaskAvoidCollisionsSDF::forwardKinematics, FK for primitive type '" + type + "' not implemented yet.");
      return -2;
    }

    return 0;
  }
  //==================================================================================
} // namespace tasks

} // namespace hiqp
